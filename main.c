  #include <stdio.h>
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "driver/spi_master.h"
  #include "driver/gpio.h"
  #include "esp_log.h"
  #include "stdbool.h"

  //defining the pins on ESP32 to be used
  #define SDA_PIN 5
  #define SCK_PIN 18
  #define MOSI_PIN 23
  #define MISO_PIN 19
  #define RST_PIN 21

  //defining MFRC522 addesses and codes
  //0x1 is memory address of the Command Register, the "brain"
  #define MFRC522_REG_COMMAND 0x1
  //0x14 is memory address of the Transmitter Control Register, controls internal transmitter antenna pins
  #define MFRC522_REG_TX_CONTROL 0x14
  //0x0F is Soft Reset, resets configurations, PCD is Proximity Coupling Device, refers to the RFIDreader module
  #define PCD_SOFT_RESET 0x0F

  //memory address of First in first out data buffer, used as I/O to write/read data from internal buffer
  #define MFRC522_REG_FIFO_DATA 0x09
  //memory address of reguster that shows how many bytes are stored in FIFO buffer
  #define MFRC522_REG_FIFO_LEVEL 0x0A
  //memory address of register that adjusts for bit-oriented frame data
  //bit-oriented framing treats a data frame a continuous stream of bits instead of a collection of bytes
  #define MFRC522_REG_BIT_FRAMING 0X0D
  //command code for MFRC552 chip to transmit data from FIFO buffer & activate the receiver after transmission
  #define PCD_TRANSCEIVE 0X0C
  //PICC is Proximity Integrated Circuit Card, this is Request Command Type A code that is sent into the air to let any Type A RFID cards to activate and answer  
  #define PICC_CMD_REQA 0X26

  #define MFRC522_FIFO_ADDR 0x09

  //assigning permanent laebl with pointer and a private handle for SPI device
  static const char *TAG = "RFID_TAG";
  static  spi_device_handle_t spi;

  //Now writing a byte to MFRC522 register
  //ESP32 sends 8 bit value to MFRC522 to control behavior, e.g. "Scan for cards" or "soft reset"
  void write_to_mfrc522_register(uint8_t reg, uint8_t value) {
    //initializing an array of 2 8bit integers, will use to write a value to a register
    uint8_t data[2] = { (reg << 1) & 0x7E, value};
    //initializing a structure configuration for the SPI transaction, using designated initializers
    //designated initializers let me initialize specific members of the structure by name
    spi_transaction_t t = {
      .length = 16,
      //tx, transmitter buffer
      .tx_buffer = &data,
    };
    //executing the SPI transaction defined by structure t, sent to device with handle spi
    //Chip Select line is pulled low, data is clocked out, and reads back incoming data from MFRC522, then pulls Chip Select back high
    spi_device_transmit(spi, &t);
  }

  //Now reading a byte from an MFRC522 register
  //provide the register address to ESP32 and the function will talk to MFRC522 via SPI to fetch and return the byte at that register
  uint8_t read_from_mfrc522_register(uint8_t reg) {
    //this is formatting an 8 bit register addres in MFRC522 for SPI protocol
    uint8_t addr = ((reg << 1) & 0x7E) | 0x80;
    //creates and initializes fixed array of 2 bytes to be used as a buffer for SPI data
    uint8_t rx_data[2] = {0};
    spi_transaction_t t = {
      .length = 16,
      .tx_buffer = &addr,
      //rx, receiver buffer
      .rx_buffer = rx_data,
    };
    spi_device_transmit(spi, &t);
    //retrieves the 2nd byte (C index array starts at 0) from the SPI slave (MFRC522) in the transactio
    return rx_data[1];
  }

  void mfrc522_write_fifo(uint8_t *data, uint8_t len) {
    if (len== 0) return;

    //buffer with address byte and then data payload
    //declare array of 64 8-bit integers, stores data before transmission
    uint8_t tx_buffer[64];
    tx_buffer[0] = (MFRC522_FIFO_ADDR << 1) & 0x7E; //write comand format

    for (int i = 0; i < len; i++) {
      tx_buffer[i + 1] = data[i];
    }

    spi_transaction_t t = {
      .length = (len + 1) * 8, //address byte + data byte
      .tx_buffer = tx_buffer,
      .rx_buffer = NULL
    };

    spi_device_transmit(spi, &t);

  }

  //read a stream of bytes from the MFRC522 FIFO buffer
  void mfrc522_read_fifo(uint8_t *data, uint8_t len) {
    if (len == 0) return;
    
    //send read address byte and clock in response byte
    uint8_t tx_buffer[64] = {0};
    uint8_t rx_buffer[64] = {0};

    tx_buffer[0] = ((MFRC522_FIFO_ADDR << 1) & 0x7E) | 0x80; //reads command format

    spi_transaction_t t = {
      .length = (len + 1) * 8, //address byte  + data bytes to clock in
      .tx_buffer = tx_buffer,
      .rx_buffer = rx_buffer
    };

    spi_device_transmit(spi, &t);

    //copy received bytes and shift past dummy response byte
    for (int i=0; i < len; i++) {
      data[i] = rx_buffer[i + 1];
    }
  }

  //initializing the SPI communication bus, using standard SPI 1 bit per cycle
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
      //max size of a single data transfer at a time in byte 
      .max_transfer_sz = 32,
    };

    //configuration for SPI slave device connected to SPI buses
    spi_device_interface_config_t devcfg = {
      //setting serial clock frequency
      .clock_speed_hz = 1000000,
      //defining timing mode, MFRC522 uses SPI Mode 0
      .mode = 0,
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

  //initializing the reader antenna for RFID
  void mfrc552_init_antenna() {
    //configuration to the standard timing setup
    write_to_mfrc522_register(0x2A, 0x8D); // TModeReg: TAuto=1, auto-restart, prescaler high
    write_to_mfrc522_register(0x2B, 0x3E); // TPrescalerReg: prescaler low
    write_to_mfrc522_register(0x2C, 0x30); // TReloadRegH: timer reload high
    write_to_mfrc522_register(0x2D, 0x00); // TReloadRegL: timer reload low
    write_to_mfrc522_register(0x15, 0x40); // TxASKReg: force 100% ASK modulation
    write_to_mfrc522_register(0x11, 0x3D); // RxModeReg: CRC enable, etc

    //turn the antenna on
    uint8_t current_value = read_from_mfrc522_register(MFRC522_REG_TX_CONTROL);
    if ((current_value & 0x03) != 0x03) {
      write_to_mfrc522_register(MFRC522_REG_TX_CONTROL, current_value | 0x03);
    }
    //verification
    uint8_t verify_value = read_from_mfrc522_register(MFRC522_REG_TX_CONTROL);
    printf("%s: MFRC522 Antenna Initialized. TxControlReg (0x14) = 0x%02X\n", TAG, verify_value);
  }

  bool mfrc522_card_detected() {
    uint8_t buffer[2];
    uint8_t valid_bits = 7; //REQA is 7 bits
    
    //clear interrupt requests to set bit framing for 7 bits
    write_to_mfrc522_register(0x04, 0x7F); //ComIrqReg
    write_to_mfrc522_register(MFRC522_REG_BIT_FRAMING, (0x00 | valid_bits));

    //load REQA into FIFO
    buffer[0] = PICC_CMD_REQA;
    write_to_mfrc522_register(MFRC522_REG_COMMAND, 0x00); //stops command
    mfrc522_write_fifo(buffer, 1);

    //issue transceive command
    write_to_mfrc522_register(MFRC522_REG_COMMAND, PCD_TRANSCEIVE);

    //start transmission, bit 7 of BitFramingReg
    uint8_t bit_framing = read_from_mfrc522_register(MFRC522_REG_BIT_FRAMING);
    write_to_mfrc522_register(MFRC522_REG_BIT_FRAMING, bit_framing | 0x80);

    //wait for completion
    vTaskDelay(pdMS_TO_TICKS(10));

    //check if data received in FIFO, ATQA is usually 2 bytes
    uint8_t fifo_level = read_from_mfrc522_register(MFRC522_REG_FIFO_LEVEL);

    //if fifo_level < 2, a card responded
    return (fifo_level >= 2);
    
  }


  void app_main() {
    printf("%s: Initializing SPI interface...\n", TAG);
    rfid_spi_init();
    mfrc552_init_antenna();


    //assigning GPIO pin to control hardware reset line of the SPI slave device
    gpio_set_direction(RST_PIN, GPIO_MODE_OUTPUT);
    //pull low
    gpio_set_level(RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    //pull high
    gpio_set_level(RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    //soft reset command, command register 0x01, soft reset 0x0F
    write_to_mfrc522_register(0x01, PCD_SOFT_RESET);

    //loop to constantly check and log version of MFRC522 to ensure that SPI connection is working
    while (true) {
      //reads 0x37 which is version register, verifying SPI connection
      uint8_t version = read_from_mfrc522_register(0x37);
      printf("%s: MFRC522 version: 0x%02X\n", TAG, version);

      if (mfrc522_card_detected()){
      printf("Card Detected!\n");
    }; 

      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }