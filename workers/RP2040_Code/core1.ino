/*
 * core1.ino
 * created by JK Rolling
 * 27 06 2022
 * 
 * Dual core and dual I2C (400kHz/1MHz)
 * DuinoCoin worker
 * 
 * See kdb.ino on Arduino IDE settings to compile for RP2040
 * 
 * with inspiration from 
 * * Revox (Duino-Coin founder)
 * * Ricaun (I2C Nano sketch)

  This is a modified version of the core1.ino from JK Rolling to works both, via single core USB
  and dual core I2C. 
  Modified by feltoxxx
  01/03/2025

 */

#pragma GCC optimize ("-Ofast")

#ifdef I2C_2_CORES

StreamString core1_bufferReceive;
StreamString core1_bufferRequest;

duco_hash_state_t core1_hashState;

// float core1_weightedMean = 0.0;        // Variable to store the weighted mean value
// const float core1_alpha = 0.2;         // Weight factor (0 < alpha <= 1), higher value gives more weight to recent values
// int core1_processedResults = 0;        // Counter to track the number of processed results

void core1_setup_i2c() {
  byte addr = 2 * DEV_INDEX + I2CS_START_ADDRESS + 1;
  I2C1.setSDA(I2C1_SDA);
  I2C1.setSCL(I2C1_SCL);
  I2C1.setClock(WIRE_CLOCK);

  I2C1.begin(addr);
  I2C1.onReceive(core1_receiveEvent);
  I2C1.onRequest(core1_requestEvent);

  printMsgln("core1 I2C1 addr:"+String(addr));
}

void core1_receiveEvent(int howMany) {
  if (howMany == 0) return;
  core1_bufferReceive.write(I2C1.read());
  while (I2C1.available()) I2C1.read();
}

void core1_requestEvent() {
  char c = '\n';
  if (core1_bufferRequest.available() > 0 && core1_bufferRequest.indexOf('\n') != -1) {
    c = core1_bufferRequest.read();
  }
  I2C1.write(c);
}

void core1_abort_loop() {
    SerialPrintln("core1 detected crc8 hash mismatch. Re-request job..");
    while (core1_bufferReceive.available()) core1_bufferReceive.read();
    core1_bufferRequest.print("#\n");
    if (WDT_EN && wdt_pet) {
      watchdog_update();
    }
    Blink(BLINK_BAD_CRC, LED_PIN);
}

void core1_send(String data) {
  core1_bufferRequest.print(data + "\n");
}

bool core1_loop() {
    
    if (core1_bufferReceive.available() > 0 && core1_bufferReceive.indexOf("$") != -1) {
            
      String action = core1_bufferReceive.readStringUntil(',');
      String field;
      String response;

      if (action == "get") {
        response = String(false);
        core1_send(response + "$");
      }
    }

    if (core1_bufferReceive.available() > 0 && core1_bufferReceive.indexOf('\n') != -1) {
      
      if (core1_bufferReceive.available() > 0 && 
        core1_bufferReceive.indexOf("$") != -1) {
        core1_bufferReceive.readStringUntil('$');
      }

      //char core1_lastBlockHash[41], core1_newBlockHash[41];

      // Read last block hash
      String core1_lastblockhash = core1_bufferReceive.readStringUntil(',');
    
      // Read expected hash
      String core1_newblockhash = core1_bufferReceive.readStringUntil(',');
      
      char core1_lastBlockHash[core1_lastblockhash.length() + 2]; // +1 para el carácter nulo '\0'
      char core1_newBlockHash[core1_newblockhash.length() + 2]; // +1 para el carácter nulo '\0'
      core1_lastblockhash.toCharArray(core1_lastBlockHash, sizeof(core1_lastBlockHash));
      core1_newblockhash.toCharArray(core1_newBlockHash, sizeof(core1_newBlockHash));

      // Read difficulty
      uintDiff core1_difficulty = strtoul(core1_bufferReceive.readStringUntil('\n').c_str(), NULL, 10);

      Serial.println("core1_job: " + core1_lastblockhash + ", " + core1_newblockhash + ", " + String(core1_difficulty));

      // clear in case of excessive jobs
      while (core1_bufferReceive.available() > 0 && core1_bufferReceive.indexOf('\n') != -1) {
        core1_bufferReceive.readStringUntil('\n');
      }
        
      uint32_t startTime = millis();

      uintDiff core1_ducos1result = ducos1a(&core1_hashState, core1_lastBlockHash, core1_newBlockHash, core1_difficulty);
      uint32_t elapsedTime = millis() - startTime;

      // // handles less than a millisecond
      // if (elapsedTime == 0) elapsedTime = 1;

      // // handles outlier
      // float current_hr = (float)core1_ducos1result / elapsedTime;
      // // If fewer than 10 results have been processed, calculate a simple mean
      // if (core1_processedResults < 10) {
      //   core1_weightedMean = ((core1_weightedMean * core1_processedResults) + current_hr) / (core1_processedResults + 1);
      //   core1_processedResults++;
      // } else {
      //   // Calculate the weighted mean value
      //   core1_weightedMean = (core1_alpha * current_hr) + ((1 - core1_alpha) * core1_weightedMean);
    
      //   // Check if the new result deviates by more than 7% from the weighted mean
      //   if ((current_hr > core1_weightedMean) && (abs(current_hr - core1_weightedMean) > core1_weightedMean * 0.05)) {
      //     elapsedTime += 1;
      //   }
      // }

      while (core1_bufferRequest.available()) core1_bufferRequest.read();

      String core1_result = String(core1_ducos1result) + "," + String(elapsedTime) + "," + String(DUCOID);

      Serial.println("Core 1 result: " + core1_result);

      core1_send(core1_result);
      
      Blink(BLINK_SHARE_FOUND, LED_PIN);
            
      return true;
    }

    return false;

}

String core1_response() {
  return core1_bufferRequest;
}

#endif