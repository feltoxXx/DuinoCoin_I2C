/*
 * core0.ino
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
 
  This is a modified version of the core0.ino from JK Rolling to works both, via single core USB
  and dual core I2C. 
  Modified by feltoxxx
  01/03/2025
 */

#pragma GCC optimize ("-Ofast")

#ifdef I2C_2_CORES

StreamString core0_bufferReceive;
StreamString core0_bufferRequest;

duco_hash_state_t core0_hashState;

// float core0_weightedMean = 0.0;        // Variable to store the weighted mean value
// const float core0_alpha = 0.2;         // Weight factor (0 < alpha <= 1), higher value gives more weight to recent values
// int core0_processedResults = 0;        // Counter to track the number of processed results


void core0_setup_i2c() {
  byte addr = 2 * DEV_INDEX + I2CS_START_ADDRESS;
  I2C0.setSDA(I2C0_SDA);
  I2C0.setSCL(I2C0_SCL);
  I2C0.setClock(WIRE_CLOCK);
  
  I2C0.begin(addr);
  I2C0.onReceive(core0_receiveEvent);
  I2C0.onRequest(core0_requestEvent);

  printMsgln("core0 I2C0 addr:"+String(addr));
}

void core0_receiveEvent(int howMany) {
  if (howMany == 0) return;
  core0_bufferReceive.write(I2C0.read());
  while (I2C0.available()) I2C0.read();
}

void core0_requestEvent() {
  char c = '\n';
  if (core0_bufferRequest.available() > 0 && core0_bufferRequest.indexOf('\n') != -1) {
    c = core0_bufferRequest.read();
  }
  I2C0.write(c);
}

void core0_abort_loop() {
    SerialPrintln("core0 detected crc8 hash mismatch. Re-request job..");
    while (core0_bufferReceive.available()) core0_bufferReceive.read();
    core0_bufferRequest.print("#\n");
    if (WDT_EN && wdt_pet) {
      watchdog_update();
    }
    Blink(BLINK_BAD_CRC, LED_PIN);
}

void core0_send(String data) {
  core0_bufferRequest.print(data + "\n");
}


bool core0_loop() {
    
    if (core0_bufferReceive.available() > 0 && core0_bufferReceive.indexOf("$") != -1) {
            
      String action = core0_bufferReceive.readStringUntil(',');
      String field;
      String response;

      if (action == "get") {
        response = String(false);
        core0_send(response + "$");
      }
    }

    if (core0_bufferReceive.available() > 0 && core0_bufferReceive.indexOf('\n') != -1) {
      
      if (core0_bufferReceive.available() > 0 && 
        core0_bufferReceive.indexOf("$") != -1) {
        core0_bufferReceive.readStringUntil('$');
      }

      //char lastBlockHash[41], newBlockHash[41];

      // Read last block hash
      String core0_lastblockhash = core0_bufferReceive.readStringUntil(',');
      
      // Read expected hash
      String core0_newblockhash = core0_bufferReceive.readStringUntil(',');
      
      char core0_lastBlockHash[core0_lastblockhash.length() + 2]; // +1 para el carácter nulo '\0'
      char core0_newBlockHash[core0_newblockhash.length() + 2]; // +1 para el carácter nulo '\0'
      core0_lastblockhash.toCharArray(core0_lastBlockHash, sizeof(core0_lastBlockHash));
      core0_newblockhash.toCharArray(core0_newBlockHash, sizeof(core0_newBlockHash));        

      // Read difficulty
      uintDiff core0_difficulty = strtoul(core0_bufferReceive.readStringUntil('\n').c_str(), NULL, 10);
      
      Serial.println("core0_job: " + core0_lastblockhash + ", " + core0_newblockhash + ", " + String(core0_difficulty));
      
      // clear in case of excessive jobs
      while (core0_bufferReceive.available() > 0 && core0_bufferReceive.indexOf('\n') != -1) {
        core0_bufferReceive.readStringUntil('\n');
      }
        
      uint32_t startTime = millis();
      
      uintDiff core0_ducos1result = ducos1a(&core0_hashState, core0_lastBlockHash, core0_newBlockHash, core0_difficulty);
      uint32_t elapsedTime = millis() - startTime;    
      
      // // handles less than a millisecond
      // if (elapsedTime == 0) elapsedTime = 1;

      // // handles outlier
      // float current_hr = (float)core0_ducos1result / elapsedTime;
      // // If fewer than 10 results have been processed, calculate a simple mean
      // if (core0_processedResults < 10) {
      //   core0_weightedMean = ((core0_weightedMean * core0_processedResults) + current_hr) / (core0_processedResults + 1);
      //   core0_processedResults++;
      // } else {
      //   // Calculate the weighted mean value
      //   core0_weightedMean = (core0_alpha * current_hr) + ((1 - core0_alpha) * core0_weightedMean);
    
      //   // Check if the new result deviates by more than 6% from the weighted mean
      //   if ((current_hr > core0_weightedMean) && (abs(current_hr - core0_weightedMean) > core0_weightedMean * 0.06)) {
      //     elapsedTime += 1;
      //   }
      // }

      while (core0_bufferRequest.available()) core0_bufferRequest.read();

      String core0_result = String(core0_ducos1result) + "," + String(elapsedTime) + "," + String(DUCOID);

      Serial.println("Core 0 result: " + core0_result);

      core0_send(core0_result);
      
      Blink(BLINK_SHARE_FOUND, LED_PIN);
            
      return true;
    }

    return false;

}

String core0_response() {
  return core0_bufferRequest;
}

#endif