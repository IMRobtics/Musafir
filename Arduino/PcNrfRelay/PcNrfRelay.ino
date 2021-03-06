#include <SPI.h>
#include <nRF24L01.h>
#include <printf.h>
#include <RF24.h>
#include <RF24_config.h>

// RF24 radio(CE,CSN);
RF24 radio(49,53);
const uint64_t pipes[2] = {0xDEDEDEDEE8LL, 0xDEDEDEDEE4LL};

boolean stringComplete = false;
String sendPayload = "";
String recvPayload = "";

void setup(void) {
  Serial.begin(115200);
  printf_begin();

  Serial.println("PcNrfRelay");
  radioInit();
  Serial.println();
  sendPayload.reserve(32);
  recvPayload.reserve(32);
}

void loop(void) {
  radioReceive();  // radio to serial
  serialReceive(); // serial to radio (AFTER PROCESS)
}

void radioInit(void){
  radio.begin();
  radio.setAutoAck(1);
  radio.enableAckPayload();
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(70);
  radio.enableDynamicPayloads();
  radio.setRetries(5,5);
  radio.setCRCLength(RF24_CRC_16);
  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1,pipes[1]);
  radio.startListening();
  radio.printDetails();
}

void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    sendPayload += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}

void radioReceive(void) {
  int len = 0;
  if ( radio.available() ) {
    len = radio.getDynamicPayloadSize();
    char buff[32]="";
    radio.read(buff,len);
    buff[len] = 0;
    buff[31] = 0;
    recvPayload += buff;
    Serial.print(recvPayload);
    Serial.println();
    recvPayload = "";
  }
}
void serialReceive(void) {
  if (stringComplete) {
    // swap TX & Rx addr for writing
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(0,pipes[0]);
    radio.stopListening();
    char buff[32]="";
    sendPayload.toCharArray(buff,32);
    radio.write(buff,strlen(buff));
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
    radio.startListening();
    sendPayload = "";
    stringComplete = false;
  }
}
