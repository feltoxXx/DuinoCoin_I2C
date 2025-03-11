/*
   ____  __  __  ____  _  _  _____       ___  _____  ____  _  _ 
  (  _ \(  )(  )(_  _)( \( )(  _  )___  / __)(  _  )(_  _)( \( )
   )(_) ))(__)(  _)(_  )  (  )(_)((___)( (__  )(_)(  _)(_  )  ( 
  (____/(______)(____)(_)\_)(_____)     \___)(_____)(____)(_)\_)
  Official code for Arduino boards (and relatives)   version 4.3
  
  Duino-Coin Team & Community 2019-2024 © MIT Licensed
  https://duinocoin.com
  https://github.com/revoxhere/duino-coin
  If you don't know where to start, visit official website and navigate to
  the Getting Started page. Have fun mining!


  This is a modified version of the Arduino Code to works both, via single core USB
  and dual core I2C.
  Modified by feltoxxx
  01/03/2025
*/


#pragma GCC optimize ("-Ofast")

#include <Wire.h>
#include <StreamString.h>     // https://github.com/ricaun/StreamJoin
#include "pico/mutex.h"
extern "C" {
  #include <hardware/watchdog.h>
}
#include "duco_hash.h"


#define I2C_2_CORES    // Uncomment this if you want to mine with 2 cores and I2C


/****************** USER MODIFICATION START ****************/
#ifdef I2C_2_CORES
  #define DEV_INDEX                   1
  #define I2CS_START_ADDRESS          8
  #define WIRE_CLOCK                  400000             // >>> see kdb before changing this I2C clock frequency <<<
  #define I2C0_SDA                    0
  #define I2C0_SCL                    1
  #define I2C1_SDA                    2
  #define I2C1_SCL                    3
  #define CRC8_EN                     false
  #define WDT_EN                      true
  #define CORE_BATON_EN               false  
  #define SINGLE_CORE_ONLY            false               // >>> see kdb before setting it to true <<<
  #define WORKER_NAME                 "rp01"
#else
  #define SINGLE_CORE_ONLY            true               // >>> see kdb before setting it to true <<<
  #define WDT_EN                      false
  #define DISABLE_2ND_CORE
#endif
/****************** USER MODIFICATION END ******************/
/*---------------------------------------------------------*/
/****************** FINE TUNING START **********************/
#define LED_PIN                     LED_BUILTIN
#define LED_EN                      false
#define SENSOR_EN                   false

#define LED_BRIGHTNESS              255                 // 1-255
#define BLINK_SHARE_FOUND           1
#define BLINK_SETUP_COMPLETE        2
#define BLINK_BAD_CRC               3
#define WDT_PERIOD                  8300
#define SERIAL_LOGGER               Serial1
#define I2C0                        Wire
#define I2C1                        Wire1
#define I2CS_MAX                    32
#define DIFF_MAX                    1000
#define MCORE_WDT_THRESHOLD         10

#define SEP_TOKEN ","
#define END_TOKEN "\n"

/****************** FINE TUNING END ************************/

#ifdef SERIAL_LOGGER
#define SerialBegin()               SERIAL_LOGGER.begin(115200);
#define SerialPrint(x)              SERIAL_LOGGER.print(x);
#define SerialPrintln(x)            SERIAL_LOGGER.println(x);
#else
#define SerialBegin()
#define SerialPrint(x)
#define SerialPrintln(x)
#endif

// ----------------------------------
// prototype
void Blink(uint8_t count, uint8_t pin);
void BlinkFade(uint8_t led_brightness);
bool repeating_timer_callback(struct repeating_timer *t);

static String DUCOID;
static mutex_t serial_mutex;
static mutex_t led_fade_mutex;
static bool core_baton;
static bool wdt_pet = true;
static uint16_t wdt_period_half = WDT_PERIOD/2;
bool core0_started = false, core1_started = false;
uint32_t core0_shares = 0, core0_shares_ss = 0, core0_shares_local = 0;
uint32_t core1_shares = 0, core1_shares_ss = 0, core1_shares_local = 0;
struct repeating_timer timer;
uint8_t core0_led_brightness = 255, core1_led_brightness = 255;

typedef uint32_t uintDiff;

// float weightedMean = 0.0;        // Variable to store the weighted mean value
// const float alpha = 0.2;         // Weight factor (0 < alpha <= 1), higher value gives more weight to recent values
// int processedResults = 0;        // Counter to track the number of processed results

// ----------------------------------

static duco_hash_state_t hash;


void single_core_usb_loop(){
  // Wait for serial data
  if (Serial.available() <= 0) {
    return;
  }

  // Reserve 1 extra byte for comma separator (and later zero)
  char lastBlockHash[40 + 1];
  char newBlockHash[40 + 1];

  // Read last block hash
  if (Serial.readBytesUntil(',', lastBlockHash, 41) != 40) {
    return;
  }
  lastBlockHash[40] = 0;

  // Read expected hash
  if (Serial.readBytesUntil(',', newBlockHash, 41) != 40) {
    return;
  }
  newBlockHash[40] = 0;

  // Read difficulty
  uintDiff difficulty = strtoul(Serial.readStringUntil(',').c_str(), NULL, 10);
  // Clearing the receive buffer reading one job.
  while (Serial.available()) Serial.read();
  // Turn off the built-in led
  #if defined(ARDUINO_ARCH_AVR)
      PORTB = PORTB | B00100000;
  #else
      digitalWrite(LED_BUILTIN, LOW);
  #endif

  // Start time measurement
  uint32_t startTime = micros();

  // Call DUCO-S1A hasher
  // uintDiff ducos1result = ducos1a(lastBlockHash, newBlockHash, difficulty);
  uintDiff ducos1result = ducos1a(&hash, lastBlockHash, newBlockHash, difficulty);

  // Calculate elapsed time
  uint32_t elapsedTime = micros() - startTime;

  // // handles less than a millisecond
  // if (elapsedTime == 0) elapsedTime = 1;

  // // handles outlier
  // float current_hr = (float)ducos1result / elapsedTime;
  // // If fewer than 10 results have been processed, calculate a simple mean
  // if (processedResults < 10) {
  //   weightedMean = ((weightedMean * processedResults) + current_hr) / (processedResults + 1);
  //   processedResults++;
  // } else {
  //   // Calculate the weighted mean value
  //   weightedMean = (alpha * current_hr) + ((1 - alpha) * weightedMean);

  //   // Check if the new result deviates by more than 6% from the weighted mean
  //   if ((current_hr > weightedMean) && (abs(current_hr - weightedMean) > weightedMean * 0.06)) {
  //     elapsedTime += 1000;
  //   }
  // }

  Blink(BLINK_SHARE_FOUND, LED_PIN);

  // Clearing the receive buffer before sending the result.
  while (Serial.available()) Serial.read();

  // Send result back to the program with share time
  Serial.print(String(ducos1result, 2) 
               + SEP_TOKEN
               + String(elapsedTime, 2) 
               + SEP_TOKEN
               + String(DUCOID) 
               + END_TOKEN);
}

// Core0
void setup() {
  Serial.begin(115200);

  // set_sys_clock_khz(50000, true);
  bool print_on_por = true;
  

  // core status indicator
  if (LED_EN) {
    add_repeating_timer_ms(10, repeating_timer_callback, NULL, &timer);
  }

  // initialize pico internal temperature sensor
  if (SENSOR_EN) {
    enable_internal_temperature_sensor();
  }

  // initialize mutex
  mutex_init(&serial_mutex);
  mutex_init(&led_fade_mutex);

  Serial.println("Veamos");
  
  // let core0 run first
  core_baton = true;
  
  DUCOID = get_DUCOID();

  #ifdef DISABLE_2ND_CORE
    Serial.setTimeout(10000);
    while (!Serial)
      ;  // For Arduino Leonardo or any board with the ATmega32U4
    //Serial.flush();
    Blink(BLINK_SETUP_COMPLETE, LED_PIN);
  #else
    // initialize watchdog
    if (WDT_EN) {
      if (watchdog_caused_reboot()) {
        printMsgln(F("\nRebooted by Watchdog!"));
        Blink(BLINK_SETUP_COMPLETE, LED_PIN);
        print_on_por = false;
      }
      // Enable the watchdog, requiring the watchdog to be updated every 8000ms or the chip will reboot
      // Maximum of 0x7fffff, which is approximately 8.3 seconds
      // second arg is pause on debug which means the watchdog will pause when stepping through code
      watchdog_enable(WDT_PERIOD, 1);
    }
    core0_setup_i2c();
    Blink(BLINK_SETUP_COMPLETE, LED_PIN);
  #endif
 
  printMsgln("core0 startup done!");

}

void loop() {

  #ifdef I2C_2_CORES
    dual_core_i2c();
  #else
    single_core_usb_loop();  
  #endif
  delay(1);
  
}

#ifndef DISABLE_2ND_CORE
// Core0 Worker
void dual_core_i2c() {
  if (core_baton || !CORE_BATON_EN) {
    if (core0_loop()) {
      core0_started = true;
      printMsg(F("core0 job done :"));
      printMsg(core0_response());
      BlinkFade(core0_led_brightness);
      if (WDT_EN && wdt_pet) {
        watchdog_update();
      }
      
      if (core0_started && core1_started && !SINGLE_CORE_ONLY) {
        core0_shares++;
        if (core1_shares != core1_shares_ss) {
          core1_shares_ss = core1_shares;
          core1_shares_local = 0;
        }
        else {
          printMsgln("core0: core1 " + String(MCORE_WDT_THRESHOLD - core1_shares_local) + " remaining count to WDT disable");
          
          if (core1_shares_local >= MCORE_WDT_THRESHOLD) {
            printMsgln("core0: Detected core1 hung. Disable WDT");
            wdt_pet = false;
          }
          if ((MCORE_WDT_THRESHOLD - core1_shares_local) != 0) {
            core1_shares_local++;
          }
        }
      }
    }
    if (!SINGLE_CORE_ONLY) core_baton = false;
  }
}
// Core1
void setup1() {
  sleep_ms(100);
  core1_setup_i2c();
  Blink(BLINK_SETUP_COMPLETE, LED_PIN);
  printMsgln("core1 startup done!");
}

void loop1() {
  if (!core_baton || !CORE_BATON_EN) {
    if (core1_loop()) {
      core1_started = true;
      printMsg(F("core1 job done :"));
      printMsg(core1_response());
      BlinkFade(core1_led_brightness);
      if (WDT_EN && wdt_pet) {
        watchdog_update();
      }

      if (core0_started && core1_started && !SINGLE_CORE_ONLY) {
        core1_shares++;
        if (core0_shares != core0_shares_ss) {
          core0_shares_ss = core0_shares;
          core0_shares_local = 0;
        }
        else {
          printMsgln("core1: core0 " + String(MCORE_WDT_THRESHOLD - core0_shares_local) + " remaining count to WDT disable");
          if (core0_shares_local >= MCORE_WDT_THRESHOLD) {
            printMsgln("core1: Detected core0 hung. Disable WDT");
            wdt_pet = false;
          }
          if ((MCORE_WDT_THRESHOLD - core0_shares_local) != 0) {
            core0_shares_local++;
          }
        }
      }
    }
    core_baton = true;
  }
}
#endif

// protect scarce resource
void printMsg(String msg) {
  uint32_t owner;
  if (!mutex_try_enter(&serial_mutex, &owner)) {
    if (owner == get_core_num()) return;
    mutex_enter_blocking(&serial_mutex);
  }
  SerialPrint(msg);
  mutex_exit(&serial_mutex);
}

void printMsgln(String msg) {
  printMsg(msg+"\n");
}