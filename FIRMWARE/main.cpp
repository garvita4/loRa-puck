#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

#define LORA_SCK   18
#define LORA_MOSI  19
#define LORA_MISO  20
#define LORA_NSS   21
#define LORA_DIO1  17
#define LORA_BUSY  22
#define LORA_NRST  23
#define LED_PIN    25 
#define LORA_FREQ_MHZ   915.0   
#define LORA_BW_KHZ     125.0
#define LORA_SF         9
#define LORA_CR         7       
#define LORA_SYNCWORD   0x12  
#define LORA_POWER_DBM  14
#define LORA_PREAMBLE   8

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);

volatile bool packetFlag = false;
bool wasTransmitting = false;
uint32_t counter = 0;

void setFlag() {
  packetFlag = true;
}

void blinkLed(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(60);
    digitalWrite(LED_PIN, LOW);
    delay(60);
  }
}

void halt(const char *msg, int16_t code) {
  Serial.printf("[ERROR] %s (code %d)\n", msg, code);
  while (true) {
    blinkLed(1);
    delay(500);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  uint32_t start = millis();
  while (!Serial && millis() - start < 3000) {
    delay(10);
  }
  Serial.println("LoRaPuck starting...");

  SPI.setSCK(LORA_SCK);
  SPI.setTX(LORA_MOSI);
  SPI.setRX(LORA_MISO);
  SPI.begin();

  int16_t state = radio.begin(
      LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
      LORA_SYNCWORD, LORA_POWER_DBM, LORA_PREAMBLE,
      0.0,   
      false); 
  if (state != RADIOLIB_ERR_NONE) {
    halt("radio.begin failed", state);
  }

  state = radio.setDio2AsRfSwitch(true);
  if (state != RADIOLIB_ERR_NONE) {
    halt("setDio2AsRfSwitch failed", state);
  }

  radio.setDio1Action(setFlag);

  state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    halt("startReceive failed", state);
  }

  Serial.println("Radio ready! Press BOOTSEL to send a ping.");
  blinkLed(3);
}

void loop() {
  static bool lastButton = false;
  bool button = BOOTSEL; 
  if (button && !lastButton) {
    String msg = "ping #" + String(counter++);
    Serial.printf("Sending: %s\n", msg.c_str());

    wasTransmitting = true;
    int16_t state = radio.startTransmit(msg);
    if (state != RADIOLIB_ERR_NONE) {
      Serial.printf("Transmit failed, code %d\n", state);
      wasTransmitting = false;
      radio.startReceive();
    }
  }
  lastButton = button;

  if (packetFlag) {
    packetFlag = false;

    if (wasTransmitting) {
      wasTransmitting = false;
      radio.finishTransmit();
      Serial.println("Sent!");
      radio.startReceive();
    } else {
      String data;
      int16_t state = radio.readData(data);
      if (state == RADIOLIB_ERR_NONE) {
        Serial.printf("Received: %s\n", data.c_str());
        Serial.printf("  RSSI: %.1f dBm | SNR: %.1f dB\n",
                      radio.getRSSI(), radio.getSNR());
        blinkLed(2);
      } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println("Received a packet but the CRC was bad");
      } else {
        Serial.printf("Read failed, code %d\n", state);
      }
      radio.startReceive();
    }
  }
}