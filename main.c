#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

//defining the pins on ESP32 to be used
#define SDA_PIN 5
#define SCK_PIN 18
#define MOSI_PIN 23
#define MISO_PIN 19
#define RST_PIN 21

//assigning permanent laebl with pointer and a private handle for SPI device
static const char *TAG - "RFID_TAG";
static  spi_device_handle_t spi;

//initializing the SPI communication bus
void rfid_spi_init() {
  //configuring GPIO pins for SPI architecture
  spi_bus_config_t buscfg = {
    //Master In Slave Out, receives data from MFRC522 and to the ESP32
    .miso_io_num = MISO_PIN,
    //Master Out Slave In, sends data from the ESP32 to the MFRC522
    .mosi_io_num = MOSI_PIN, 
    //Serial Clock, used to sync transfer of data bits between master and slave devices
    .sclk_io_num = SCK_PIN,
    //quad write protect used in 4-bit transmission, quad SPI, which I'm not using
    .quadwp_io_num = -1,
    //quad hold used in 4-bit transmission, quad SPI, which I'm not using
    .quadhd_io_num = -1,
    //max size of a single data transfer at a time in bytes
    .max_transfer_sz = 32,
  };

  //configuration for SPI slave device connected to SPI buses
  spi_device_config_t devcfg = {
    //setting serial clock frequency
    .clock_speed_hz = 1000000;
    //defining timing mode, MFRC522 uses SPI Mode 0
    .mode = 0
    //defining the chip select pin, allows ESP32 to communicatee specifically with this device
    .spics_io_num = SDA_PIN,
    //dicates how many SPI transactions can be running at a time
    .queue_size = 7,
  };

  //Initializing SPI bus
  //specifies which ESP32 internal peripherals to use, uses pointer to assign GPIO pins for bus
  //Error check will print an error and stop the program if initialization fails
  ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
  //attaching MFRC522 device to SPI bus, pointer to define settings for this device, pointer to private handle 
  ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfg, &spi));

}

