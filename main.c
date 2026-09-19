#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "unist.h"
#include "SPI.h"
#include <MFRC522.h>

#define SS_PIN 5
#define RST_PIN 21
#define led_pin 22

MFRC522 rfid(SS_PIN, RST_PIN);

//simulate SPI 
void spi_message()uint8_t *data_out, uint8_t *data_in, uint8_t len) {

}

//send UID data to ESP32
void send_uid() {
  for (int i = 0; i <  10; i++) {
    uint8_t uid_data[4] {0x12, 0x34, 0x56,}
    //print UID data
    printf("UID %d: ", i + 1);
    for (int j = 0; j < 4; j++) {
      printf("%02X", uid_data[j]);
    }
    printf("\n");

    //send UID data via SPI
    spi_transfer(uid_data, NULL, 4):
  }
}

//chip and pin setup
void init() {

}

void app_main() {
  //init chip
  init()

  //send UID data to ESP32 in intervals
  while (true) {
    send_uid();
    sleep(5)
  }




  while (true) {
    gpio_set_level(LED_PIN, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
