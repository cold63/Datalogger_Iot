#include <SPI.h>
#include <LoRa.h>

int counter = 0;

void setup() {
  Serial.begin(115200);
 // while (!Serial);

  Serial.println("LoRa Sender");


  LoRa.setPins(10,9,5);

 

  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setSpreadingFactor(7);     // 設置擴頻因子
  LoRa.setSignalBandwidth(125E3); // 設置信號帶寬 
  LoRa.setCodingRate4(5);         // 設置編碼率  
  LoRa.setSyncWord(0x12);          // 同步訊號字，需與傳送端一致
  LoRa.setTxPower(10);    

}

void loop() {
  Serial.print("Sending packet: ");
  Serial.println(counter);

  // send packet
  LoRa.beginPacket();
  LoRa.print("hello ");
  LoRa.print(counter);
  LoRa.endPacket();

  counter++;

  delay(5000);
}
