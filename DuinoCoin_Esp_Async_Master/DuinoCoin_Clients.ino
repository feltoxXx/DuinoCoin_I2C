/*
  DuinoCoin_Clients.ino
  created 10 05 2021
  by Luiz H. Cassettari
  
  Modified by JK Rolling
  
  Some changes to get a good performance in H/s. 
  Modified by feltoxxx
  01/03/2025
*/

#include <WiFi.h>
#include <WiFiClient.h>


// ------------------------------
// Parámetros y definiciones
// ------------------------------
#define CLIENTS 30
#define FIRST_SLAVE 8
#define CLIENT_CONNECT_EVERY 30000
#define CLIENT_TIMEOUT_CONNECTION 50000
#define CLIENT_TIMEOUT_REQUEST 100

#define GROUPING true
#define GROUP_ID "3666"  // Random number for grouping
#define DIFF_RESET_THRESHOLD 4025 // Diff threshold for reset slave

#define END_TOKEN  '\n'
#define SEP_TOKEN  ','
#define BAD "Rejected"
#define GOOD "Accepted"
#define BLOCK "Block"

#define HASHRATE_FORCE false
#define HASHRATE_SPEED 258.0

#define IS_AVR true    //define if you are using Arduino as slave


String host = "162.55.103.174";
int port = 6000;
String final_mining_key = "None";

const float alpha = 0.2;         // Weight factor (0 < alpha <= 1), higher value gives more weight to recent values


void SetHostPort(String h, int p) { host = h; port = p; }

String getHostPort() { return host + ":" + String(port); }

String SetHost(String h) { host = h; return host; }

int SetPort(int p) { port = p; return port; }

void SetMiningKey(String key) { final_mining_key = key; }

// ------------------------------
enum Duino_State {
  DUINO_STATE_NONE,
  DUINO_STATE_VERSION_WAIT,
  DUINO_STATE_MOTD_REQUEST,
  DUINO_STATE_MOTD_WAIT,
  DUINO_STATE_JOB_REQUEST,
  DUINO_STATE_JOB_WAIT,
  DUINO_STATE_JOB_DONE_SEND,
  DUINO_STATE_JOB_DONE_WAIT,
};

// ------------------------------
// New Struct for better info organization of each slave
struct DuinoClient {
  WiFiClient client;
  Duino_State state;
  int shares;       
  String buffer;    
  unsigned long times;
  unsigned long timeOut;
  unsigned long pingTime;
  unsigned long getTime;
  unsigned long elapsedTime;
  float hashRate;         
  unsigned int diff;      
  byte badJob;            
  byte forceReconnect;    
  bool crc8Status;        
  uint8_t currentJobID;   

  float weightedMean;        // Variable to store the weighted mean value
  int processedResults;        // Counter to track the number of processed results

  DuinoClient() {
    state = DUINO_STATE_NONE;
    shares = 0;
    buffer = "";
    times = 0;
    timeOut = 0;
    pingTime = 0;
    getTime = 0;
    elapsedTime = 0;
    hashRate = 0;
    diff = 0;
    badJob = 0;
    forceReconnect = 0;
    crc8Status = false;
    currentJobID = 0;
    weightedMean = 0.0;
    processedResults = 0;
  }
};

DuinoClient duinoClients[CLIENTS];


String poolMOTD;
unsigned int share_count = 0;
unsigned int accepted_count = 0;
unsigned int block_count = 0;
unsigned int last_share_count = 0;
unsigned long globalStartTime = millis();
unsigned long clientsConnectTime = 0;
bool clientsMOTD = true;
bool clientsQuery = true;
byte firstClientAddr = 0;

// ------------------------------
bool clients_connected(byte i) {
  return duinoClients[i].client.connected();
}

bool clients_connect(byte i)
{
  if (duinoClients[i].client.connected()) return true;
  
  wire_readLine(i);
  clients_query(i);
  clients_waitAns(i);

  Serial.printf("[Client %d] Connecting to Duino-Coin server... %s %d\n", i, host.c_str(), port);

  duinoClients[i].client.setTimeout(30000);
  duinoClients[i].client.flush();
  if (!duinoClients[i].client.connect(host.c_str(), port)) {
    Serial.printf("[Client %d] Connection failed..\n", i);
    UpdatePool();
    return false;
  }

  duinoClients[i].client.setTimeout(1);
  duinoClients[i].shares = 0;
  duinoClients[i].badJob = 0;
  duinoClients[i].times = millis();
  duinoClients[i].buffer = "";
  clients_state(i, DUINO_STATE_VERSION_WAIT);
  duinoClients[i].forceReconnect = false;
  
  // share_count = 0;
  // accepted_count = 0;
  // block_count = 0;
  // last_share_count = 0;

  vTaskDelay(1500);
  return true;

}

void clients_state(byte i, byte state) {
  duinoClients[i].state = (Duino_State) state;
  duinoClients[i].timeOut = millis();
}

bool clients_stop(byte i) {
  clients_state(i, DUINO_STATE_NONE);
  // share_count = share_count - duinoClients[i].shares;
  // accepted_count = accepted_count - duinoClients[i].shares;
  // last_share_count = last_share_count - duinoClients[i].shares;
  duinoClients[i].client.stop();
  return true;
}

void force_clients_reconnect() {
  for (byte j = FIRST_SLAVE; j < CLIENTS; j++) {
    duinoClients[j].forceReconnect = 1;
  }
  clientsMOTD = true;
}

void ClientConnector() {
  while (true) {
    
    if (clients_runEvery(clientsConnectTime)) {
    
      clientsConnectTime = CLIENT_CONNECT_EVERY;
    
      for (int i = FIRST_SLAVE; i < CLIENTS; i++) {
    
        if (wire_exists(i) && !clients_connect(i)) {
    
          break;
        }
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void clients_loop() {
  
  
  for (int i = FIRST_SLAVE; i < CLIENTS; i++) {
    if (wire_exists(i) && duinoClients[i].client.connected()) {
  
      for (int j = 0; j < 3; j++) {
        switch (duinoClients[i].state) {
          case DUINO_STATE_VERSION_WAIT:
            clients_waitRequestVersion(i);
            break;
          case DUINO_STATE_JOB_REQUEST:
            clients_requestJob(i);
            break;
          case DUINO_STATE_JOB_WAIT:
            clients_waitRequestJob(i);
            break;
          case DUINO_STATE_JOB_DONE_SEND:
            clients_sendJobDone(i);
            break;
          case DUINO_STATE_JOB_DONE_WAIT:
            clients_waitFeedbackJobDone(i);
            break;
          case DUINO_STATE_MOTD_REQUEST:
            clients_requestMOTD(i);
            break;
          case DUINO_STATE_MOTD_WAIT:
            clients_waitMOTD(i);
            break;
          default:
            break;
        }
      }
      
  
      if (millis() - duinoClients[i].timeOut > CLIENT_TIMEOUT_CONNECTION) {
        Serial.printf("[Client %d] --------------- TIMEOUT ------------- %s\n", i, duinoClients[i].state);
        clients_stop(i);
      }
    }
  }
}

// ------------------------------
void clients_query(byte i) {
  // Serial.print("[" + String(i) + "] ");
  // Serial.println("Worker: Query CRC8 status");
  if (clientsQuery) {
    firstClientAddr = i;
    Wire_sendCmd(i, "get,crc8");
  }
}

void clients_waitAns(byte i) {
  if (clientsQuery) {
    query_runEvery(0);
    bool done = false;
    while (!done) {
      String responseJob = wire_readLine(i);
      if (responseJob.length() > 0) {
        StreamString response;
        response.print(responseJob);
        duinoClients[i].crc8Status = response.readStringUntil('\n').toInt();
        Serial.printf("[Client %d] CRC8 update status: %d\n", i, duinoClients[i].crc8Status);
        clientsQuery = false; // set this to true if each client have different crc8 config
        done = true;
        break;
      }
      if (query_runEvery(100)) {
        duinoClients[i].crc8Status = false;
        Serial.printf("[Client %d] CRC8 fallback status: %d\n", i, duinoClients[i].crc8Status);
        Wire_sendln(i, ",1000");
        vTaskDelay(10);
        wire_readLine(i);
        clientsQuery = false;
        done = true;
      }
    }
  }
  else {
    // common use case where all slave have same crc8 settings
    duinoClients[i].crc8Status = duinoClients[firstClientAddr].crc8Status;
    // Serial.print("[" + String(i) + "] ");
    // Serial.println("CRC8 status: "+String(clientsCRC8Status[i]));
  }
}

void clients_waitMOTD(byte i) {
  if (duinoClients[i].client.available()) {
    String buffer = duinoClients[i].client.readString();
    poolMOTD = buffer;
    Serial.printf("[Client %d] MOTD: %s\n", i, buffer.c_str());
    clients_state(i, DUINO_STATE_JOB_REQUEST);
  }
}

void clients_requestMOTD(byte i) {
  Serial.printf("[Client %d] MOTD Request: %s\n", i, ducouser);
  duinoClients[i].client.print("MOTD");
  clients_state(i, DUINO_STATE_MOTD_WAIT);
}

String printMOTD() {
  return poolMOTD;
}

void clients_waitRequestVersion(byte i) {
  if (duinoClients[i].client.available()) {
    String buffer = duinoClients[i].client.readStringUntil(END_TOKEN);
    Serial.printf("[Client %d] %s\n", i, buffer.c_str());
    clients_state(i, DUINO_STATE_JOB_REQUEST);
    if (clientsMOTD) clients_state(i, DUINO_STATE_MOTD_REQUEST);
    clientsMOTD = false;
  }
}

void clients_requestJob(byte i) {
  // Serial.printf("[Client %d] Job Request: %s\n", i, ducouser);
  duinoClients[i].client.print("JOB," + String(ducouser) + "," + JOB + "," + String(final_mining_key));
  duinoClients[i].getTime = millis();
  clients_state(i, DUINO_STATE_JOB_WAIT);
}

void clients_waitRequestJob(byte i) {
  String clientBuffer = clients_readData(i);
  if (clientBuffer.length() > 0) {
    unsigned long getJobTime = millis() - duinoClients[i].getTime;
    
    // Serial.print("[" + String(i) + "] ");
    // Serial.print("Job ");
    // Serial.println(clientBuffer);

    
    if (clientBuffer.indexOf(SEP_TOKEN) == -1) {
      clients_stop(i);
      return;
    }

    String hash = getValue(clientBuffer, SEP_TOKEN, 0);
    String job = getValue(clientBuffer, SEP_TOKEN, 1);
    unsigned int diff = getValue(clientBuffer, SEP_TOKEN, 2).toInt();
    duinoClients[i].diff = diff;
    duinoClients[i].elapsedTime = millis();

    if (duinoClients[i].diff > DIFF_RESET_THRESHOLD) {
      Serial.print("[" + String(i) + "] ");
      Serial.print("Job ");
      Serial.println(clientBuffer);
      Serial.printf("[Client %d] Disconnecting due to Diff Threshold reached\n", i);
      clients_stop(i);
      // share_count--;
      return;
    }

    if (duinoClients[i].crc8Status) {
      String data = hash + SEP_TOKEN + job + SEP_TOKEN + String(diff) + SEP_TOKEN;
      uint8_t crc8 = calc_crc8(data);
      data += String(crc8);
      Serial.print("," + String(crc8));
      Wire_sendln(i, data);
    } else {
      wire_sendJob(i, hash, job, diff);
    }
    clients_state(i, DUINO_STATE_JOB_DONE_SEND);
  }
}

void clients_sendJobDone(byte i) {
  String responseJob = wire_readLine(i);
  if (responseJob.length() > 0) {
    duinoClients[i].elapsedTime = (millis() - duinoClients[i].elapsedTime);
    StreamString response;
    response.print(responseJob);
    int job = getValue(response, SEP_TOKEN, 0).toInt();
    int time = getValue(response, SEP_TOKEN, 1).toInt();

    if (IS_AVR) {
      time = time * 0.001f;
    }

    // handles less than a millisecond
    if (time == 0) time = 1;

    // handles outlier
    float current_hr = job / time;
    // If fewer than 10 results have been processed, calculate a simple mean
    if (duinoClients[i].processedResults < 10) {
      duinoClients[i].weightedMean = ((duinoClients[i].weightedMean * duinoClients[i].processedResults) + current_hr) / (duinoClients[i].processedResults + 1);
      duinoClients[i].processedResults++;
    } else {
      // Calculate the weighted mean value
      duinoClients[i].weightedMean = (alpha * current_hr) + ((1 - alpha) * duinoClients[i].weightedMean);
  
      // Check if the new result deviates by more than 6% from the weighted mean
      if ((current_hr > duinoClients[i].weightedMean) && (abs(current_hr - duinoClients[i].weightedMean) > duinoClients[i].weightedMean * 0.06)) {
        time += 1;
        // Serial.println(" ");
        // Serial.print("[Client " + String(i) + " ]: WM=" + String(duinoClients[i].weightedMean));
        // Serial.print(" current_hr=" + String(current_hr));
        // Serial.print(" new_hr=" + String(job / time));
        // Serial.println(" result=" + String(job) + " et="+ String(time));
        // Serial.println(" ");

      }

    }

    String id;
    if (duinoClients[i].crc8Status) {
      id = response.readStringUntil(SEP_TOKEN);
      uint8_t received_crc8 = response.readStringUntil('\n').toInt();
      String data = String(job) + SEP_TOKEN + String(time) + SEP_TOKEN + id + SEP_TOKEN;
      uint8_t expected_crc8 = calc_crc8(data);
      if (received_crc8 != expected_crc8) {
        // CRC Error 
      }
    } else {
      id = getValue(response, SEP_TOKEN, 2);
    }

    duinoClients[i].hashRate = job / (time * 0.001f);

    if (HASHRATE_FORCE) {
      Serial.printf("[Client %d] Slow down HashRate: %.2f\n", i, duinoClients[i].hashRate);
      duinoClients[i].hashRate = HASHRATE_SPEED + random(-50, 50) / 100.0;
    }
    if (id.length() > 0) id = "," + id;
    // String identifier = rigIdentifier + "-" + String(i);
    String identifier = String(rigIdentifier) + "-" + String(i);
    
    if (GROUPING) {
      duinoClients[i].client.print(String(job) + "," + String(duinoClients[i].hashRate, 2) + "," + MINER + "," + identifier + id + "," + String(GROUP_ID));
    } else {
      duinoClients[i].client.print(String(job) + "," + String(duinoClients[i].hashRate, 2) + "," + MINER + "," + identifier + id);
    }
    // Serial.printf("[Client %d] %s\n", i, (String(job) + "," + String(duinoClients[i].hashRate, 2) + "," + MINER + "," + identifier + id + "," + String(GROUP_ID));
    
    duinoClients[i].pingTime = millis();
    clients_state(i, DUINO_STATE_JOB_DONE_WAIT);
  }
}

void clients_waitFeedbackJobDone(byte i) {
  String clientBuffer = clients_readData(i);
  if (clientBuffer.length() > 0) {
    unsigned long pingTime = (millis() - duinoClients[i].pingTime);
    duinoClients[i].shares++;
    share_count++;
    String verdict = "";
    if (clientBuffer == "GOOD") {
      accepted_count++;
      verdict = GOOD;
    } else if (clientBuffer == "BLOCK") {
      block_count++;
      verdict = BLOCK;
    } else {
      verdict = BAD;
    }

    Serial.printf("[Client %d] %s  ⛏ %d/%d (%.2f%%)  %.2fs  %.2f H/s  ⚙ diff %d  ping %lums\n",
                  i, verdict.c_str(), accepted_count, share_count,
                  (float)accepted_count/(float)share_count*100, duinoClients[i].elapsedTime/1000.0,
                  duinoClients[i].hashRate, duinoClients[i].diff, pingTime);
    
    clients_state(i, DUINO_STATE_JOB_REQUEST);
    if (clientBuffer == "BAD") {
      if (duinoClients[i].badJob++ > 9) {
        Serial.printf("[Client %d] BAD BAD BAD BAD\n", i);
        clients_stop(i);
      }
    } else {
      duinoClients[i].badJob = 0;
    }
    if (duinoClients[i].forceReconnect) {
      Serial.printf("[Client %d] Forced disconnect\n", i);
      clients_stop(i);
    }
  }
}

String clients_string() {
  String str = "I2C [ ";
  for (int i = FIRST_SLAVE; i < CLIENTS; i++) {
    if (wire_exists(i)) {
      str += String(i);
      str += clients_connected(i) ? "." : "";
      str += " ";
    }
  }
  str += "]";
  return str;
}

String timeString(unsigned long t) {
  String str = "";
  t /= 1000;
  int s = t % 60;
  int m = (t / 60) % 60;
  int h = (t / 3600);
  str += String(h) + ":" + (m < 10 ? "0" : "") + String(m) + ":" + (s < 10 ? "0" : "") + String(s);
  return str;
}

String clients_readData(byte i) {
  String str = "";
  if (duinoClients[i].client.available()) {  
    str = duinoClients[i].client.readStringUntil(END_TOKEN); // Lee hasta encontrar END_TOKEN
  }
  if (str != "") {  
    if (millis() - duinoClients[i].timeOut > CLIENT_TIMEOUT_REQUEST) {  
      duinoClients[i].buffer = "";  // Limpiar el buffer si el tiempo de espera expiró
    }
  }
  return str; // Retornar la data recibida
}

// https://stackoverflow.com/questions/9072320/split-string-into-string-array
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

String waitForClientData(int i) {
  unsigned long previousMillis = millis();
  unsigned long interval = 10000;  // 10 secs timeout
  String buffer;
  
  while (duinoClients[i].client.connected()) {
    if (duinoClients[i].client.available()) {
      Serial.println(duinoClients[i].client.available());
      buffer = duinoClients[i].client.readStringUntil(END_TOKEN);

      if (buffer.length() == 1 && buffer[0] == END_TOKEN)
        buffer = "???\n"; // NOTE: Should never happen...
      if (buffer.length() > 0)
        break;
    }
    if (millis() - previousMillis >= interval) break;
    handleSystemEvents(); 
  }
  
  return buffer;
}

void periodic_report(unsigned long interval) {
  unsigned long uptime = (millis() - globalStartTime);
  unsigned int report_shares = share_count - last_share_count;
  float average_hashrate = calc_avg_hashrate();

  Serial.println(" ");
  Serial.println("Periodic mining report:");
  Serial.println("‖ During the last " + String(interval/1000.0, 1) + " seconds");
  Serial.println("‖ You've mined " + String(report_shares)+ " shares (" + String((float)report_shares/(interval/1000.0), 2) + " shares/s)");
  Serial.println("‖ Block(s) found: " + String(block_count));
  Serial.println("‖ With the average hashrate of " + String(average_hashrate, 2) + " H/s  ");
  Serial.println("‖ In this time period, you've solved " + String(average_hashrate * (interval/1000),0) + " hashes");
  Serial.println("‖ Total miner uptime: " + timeString(uptime));
  Serial.println(" ");
  
  last_share_count = share_count;
}

float calc_avg_hashrate() {
  float hashrate_sum = 0;
  int client_count = 0;
  for (int i = FIRST_SLAVE; i < CLIENTS; i++) {
    if (duinoClients[i].hashRate > 0.0) {
      client_count++;
      hashrate_sum += duinoClients[i].hashRate;
    }
  }
  return (client_count == 0) ? 0 : hashrate_sum / float(client_count);
}

// https://stackoverflow.com/questions/51731313/cross-platform-crc8-function-c-and-python-parity-check
// CRC8 functions
uint8_t crc8(uint8_t *addr, uint8_t len) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; i++) {
    uint8_t inbyte = addr[i];
    for (uint8_t j = 0; j < 8; j++) {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix)
        crc ^= 0x8C;
      inbyte >>= 1;
    }
  }
  return crc;
}

uint8_t calc_crc8(String msg) {
  int msg_len = msg.length() + 1;
  char char_array[msg_len];
  msg.toCharArray(char_array, msg_len);
  return crc8((uint8_t *)char_array, msg.length());
}

boolean clients_runEvery(unsigned long interval) {
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}

boolean query_runEvery(unsigned long interval) {
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}
