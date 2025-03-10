/*
 * pico_utils.ino
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

  This is a modified version of the pico_utils.ino from JK Rolling to get the RP2040 RGB led working properly. 
  Modified by feltoxxx
  01/03/2025
 */

#pragma GCC optimize ("-Ofast")

#include "pico/unique_id.h"
#include "hardware/structs/rosc.h"
#include "hardware/adc.h"

#ifndef LED_EN
#define LED_EN false
#endif

#include <NeoPixelConnect.h>

NeoPixelConnect p(16, 1, pio0, 0);


String get_DUCOID() {
  int len = 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1;
  uint8_t buff[len] = "";
  pico_get_unique_board_id_string((char *)buff, len);
  String uniqueID = String ((char *)buff, strlen((char *)buff));
  return "DUCOID"+uniqueID;
}

void enable_internal_temperature_sensor() {
  adc_init();
  adc_set_temp_sensor_enabled(true);
  adc_select_input(0x4);
}

double read_temperature() {
  uint16_t adcValue = adc_read();
  double temperature;
  temperature = 3.3f / 0x1000;
  temperature *= adcValue;
  // celcius degree
  temperature = 27.0 - ((temperature - 0.706)/ 0.001721);
  // fahrenheit degree
  // temperature = temperature * 9 / 5 + 32;
  return temperature;
}

/* Von Neumann extractor: 
From the input stream, this extractor took bits, 
two at a time (first and second, then third and fourth, and so on). 
If the two bits matched, no output was generated. 
If the bits differed, the value of the first bit was output. 
*/
uint32_t rnd_whitened(void){
    uint32_t k, random = 0;
    uint32_t random_bit1, random_bit2;
    volatile uint32_t *rnd_reg=(uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);
    
    for (k = 0; k < 32; k++) {
        while(1) {
            random_bit1=0x00000001 & (*rnd_reg);
            random_bit2=0x00000001 & (*rnd_reg);
            if (random_bit1 != random_bit2) break;
        }
        random = random + random_bit1;
        random = random << 1;    
    }
    return random;
}

struct LEDState {
  float fade;
  uint8_t green, red, blue;
};
 
volatile LEDState ledState[2] = {
  {0, 255, 0, 0},   // Core 0 (green) GRB
  {0, 0, 0, 255}    // Core 1 (blue)  GRB
};
 
void Blink(uint8_t count, uint8_t pin = LED_BUILTIN) {
  if (!LED_EN) return;
  uint8_t core = get_core_num();
  sleep_ms(50);

  for (int x = 0; x < (count); ++x) {

    p.neoPixelFill(ledState[core].green, ledState[core].red, ledState[core].blue, true);
    sleep_ms(50);
    p.neoPixelClear(true);
    sleep_ms(50);

  }
}

bool repeating_timer_callback(struct repeating_timer *t) {
  uint8_t core = get_core_num();

  if (ledState[core].fade > 0) {
    ledState[core].fade = (ledState[core].fade * 0.9) - 0.1;
    if (ledState[core].fade < 1) ledState[core].fade = 0;

    p.neoPixelFill( ledState[core].green * ledState[core].fade / 255,
                    ledState[core].red * ledState[core].fade / 255,
                    ledState[core].blue * ledState[core].fade / 255, true
                  );
     
    if (ledState[core].fade == 0) {
      p.neoPixelClear(true);
    }
  }
  return true;
}
 
void BlinkFade(uint8_t led_brightness) {
  if (!LED_EN) return;
  mutex_enter_blocking(&led_fade_mutex);
  uint8_t core = get_core_num();
  ledState[core].fade = led_brightness;
  mutex_exit(&led_fade_mutex);
}
 