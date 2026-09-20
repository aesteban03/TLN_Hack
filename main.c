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

  //specifies command byte used by RFID reader to start anti-collision process at cascade lvl 1 with a card/tag
  #define PICC_CMD_SEL_CL1 0x93

  //For cloned-card simulation
  #define CLONE_TOGGLE_PIN 4

  //For card registration & UID and encrypted key storage
  #define MAX_CARDS 3
  #define UID_LEN 4
  #define KEY_LEN 8
  uint8_t registered_cards[MAX_CARDS][UID_LEN]; //stores 3 cards of 4 byte UID's, for simplicity.
  uint8_t blue_uid[UID_LEN] = {0x01, 0x02, 0x03, 0x04};
  int blue_card_index = 0; 
  int registered_card_count = 1;
  void register_uid(const uint8_t *uid) {
    for (int i = 0; i < UID_LEN; i++) {
      registered_cards[blue_card_index][i] = uid[i];
    }
  }

   //compares 2 uid's to see if they are identical, to be used when verifyingf card registration
  bool compare_uid(const uint8_t *id1, const uint8_t *id2) {
    for (int i = 0; i < UID_LEN; i++) {
      if (id1[i] != id2[i]) return false;
    }
    return true;
  } 

  bool check_card_registration(const uint8_t *uid) {  
    for (int i = 0; i < registered_card_count; i++) {
      if (compare_uid(registered_cards[blue_card_index], uid)) {
        printf("Access Granted! UID: %02X:%02X:%02X:%02X\n", uid[0], uid[1], uid[2], uid[3]);
        return true;
      }
    }
    printf("Access Denied! UID: %02X:%02X:%02X:%02X\n", uid[0], uid[1], uid[2], uid[3]);
    return false;
  }

// Secondary secret/encrypted key array matching each registered UID
uint8_t registered_keys[MAX_CARDS][KEY_LEN];

// Sample secret key for blue card
uint8_t blue_card_key[KEY_LEN] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};

// Function to register secondary key along with card UID
void register_card_key(int card_idx, const uint8_t *key) {
    for (int i = 0; i < KEY_LEN; i++) {
        registered_keys[card_idx][i] = key[i];
    }
}

// Function to compare secret keys
bool compare_key(const uint8_t *key1, const uint8_t *key2) {
    for (int i = 0; i < KEY_LEN; i++) {
        if (key1[i] != key2[i]) return false;
    }
    return true;
}

// Simulated function to read the secondary encrypted key from card storage/EEPROM block
// In actual hardware, this would perform a MIFARE Read command on a specific block address
// (e.g., using PICC_CMD_MF_READ after authenticating with Key A/B).
// For Wokwi simulation, we mock retrieving the expected key for the simulated blue card.
bool mfrc522_read_card_key(uint8_t *key_out) {
    for (int i = 0; i < KEY_LEN; i++) {
        key_out[i] = blue_card_key[i];
    }

    return true;
}

// Function to verify both UID and Encrypted Key to detect clone cards
bool check_card_authenticity(const uint8_t *uid, const uint8_t *scanned_key) {
    for (int i = 0; i < registered_card_count; i++) {
        if (compare_uid(registered_cards[i], uid)) {
            // UID matches registered user; now verify secondary key
            if (compare_key(registered_keys[i], scanned_key)) {
                printf("Access Granted! Authentic Card Verified.\n");
                return true;
            } else {
                printf("ALERT: CLONE DETECTED! UID matched but secondary secret key failed verification.\n");
                return false;
            }
        }
    }
    printf("Access Denied! Unregistered UID.\n");
    return false;
}

void simulate_clone_card() {
    uint8_t clone_uid[UID_LEN] = {
        0x01, 0x02, 0x03, 0x04
    };

    uint8_t fake_key[KEY_LEN] = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    printf("\n========================================\n");
    printf("[SIMULATION] Clone card presented!\n");
    printf("Simulated UID: %02X:%02X:%02X:%02X\n",
           clone_uid[0],
           clone_uid[1],
           clone_uid[2],
           clone_uid[3]);

    printf("[SIMULATION] Forged secret key: ");

    for (int i = 0; i < KEY_LEN; i++) {
        printf("%02X", fake_key[i]);
    }

    printf("\n");

    check_card_authenticity(clone_uid, fake_key);

    printf("========================================\n\n");
}



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

  //for the ESP32 to see the UID of the card being scanned, UID is Unique Identifier
  bool mfrc522_read_uid(uint8_t *uid_out) {
    uint8_t buffer[5];

    //flushes the internal buffer FIFO buffer, ensures that no leftover or corrupted data skews the new data
    write_to_mfrc522_register(MFRC522_REG_FIFO_LEVEL, 0x80);

    //anticollision level 1 command 0x93 and NVB (number of valid bits)
    buffer[0] = PICC_CMD_SEL_CL1;
    buffer[1] = 0x20;

    //clear interrupt requests and reset bit framing
    write_to_mfrc522_register(0x04, 0x7F);
    write_to_mfrc522_register(MFRC522_REG_BIT_FRAMING, 0x00);

    //load command into FIFO and transceive
    write_to_mfrc522_register(MFRC522_REG_COMMAND, 0x00); //stop command
    mfrc522_write_fifo(buffer, 2);

    write_to_mfrc522_register(MFRC522_REG_COMMAND, PCD_TRANSCEIVE);

    //start transmission
    uint8_t bit_framing = read_from_mfrc522_register(MFRC522_REG_BIT_FRAMING);
    write_to_mfrc522_register(MFRC522_REG_BIT_FRAMING, bit_framing | 0x80);

    //wait for card response
    vTaskDelay(pdMS_TO_TICKS(10));

    //check if 5 bytes were received back, 4 uid bytes + block check character
    uint8_t fifo_level = read_from_mfrc522_register(MFRC522_REG_FIFO_LEVEL);
    if (fifo_level < 5) {
      return false;
    }

    //read 5 bytes from fifo
    mfrc522_read_fifo(buffer, 5);

    //copy first 4 bytes into output array
    for (int i = 0; i < 4; i++) {
      uid_out[i] = buffer[i];
    }

    return true;

      
  }


  void app_main() {
    printf("%s: Initializing SPI interface...\n", TAG);
    rfid_spi_init();
    mfrc552_init_antenna();
    // Force registered card slot 0 to match the Wokwi default tag if UID array is empty
    for (int i = 0; i < UID_LEN; i++) {
      registered_cards[0][i] = blue_uid[i];
    }
    register_card_key(0, blue_card_key);

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

    //initial version check of MFRC522
    uint8_t init_version = read_from_mfrc522_register(0x37);
    printf("%s: MFRC522 version: 0x%02X\n", TAG, init_version);

    //initializing gpio4 for clone simulation
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CLONE_TOGGLE_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    //loop to constantly check and log version of MFRC522 to ensure that SPI connection is working
    while (true) {

    /*
     * Check the clone simulation button first.
     *
     * GPIO 4 uses an internal pull-up:
     * HIGH = not pressed
     * LOW  = pressed
     */
    if (gpio_get_level(CLONE_TOGGLE_PIN) == 0) {

        simulate_clone_card();

        // Wait until button is released.
        while (gpio_get_level(CLONE_TOGGLE_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        // Small debounce delay.
        vTaskDelay(pdMS_TO_TICKS(100));
    }


    /*
     * Normal MFRC522 card scanning.
     */
    if (mfrc522_card_detected()) {

        uint8_t uid[UID_LEN];

        if (mfrc522_read_uid(uid)) {

            printf("Card Detected! UID: %02X:%02X:%02X:%02X\n",
                   uid[0],
                   uid[1],
                   uid[2],
                   uid[3]);

            //uint8_t scanned_key[KEY_LEN];

            if (compare_uid(registered_cards[0], uid)) {

    uint8_t scanned_key[KEY_LEN];

    if (mfrc522_read_card_key(scanned_key)) {

        printf("Secret Key: ");

        for (int i = 0; i < KEY_LEN; i++) {
            printf("%02X", scanned_key[i]);

            if (i < KEY_LEN - 1) {
                printf(":");
            }
        }

        printf("\n");

        check_card_authenticity(uid, scanned_key);

    } else {

        printf("Error: Could not read secure block from card.\n");
    }

} else {

    printf("Access Denied! Unregistered UID.\n");
}

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelay(pdMS_TO_TICKS(10));
}
    }
  }