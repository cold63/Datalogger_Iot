/*
執行這個範例前除了安裝 u8g2程式庫之外也要安裝以下 Modbus 程式庫

	" ModbusMaster"  by Doc Walker
	
	在 Librarary Manager 中安裝即可.


Before running this example, in addition to installing the u8g2 library, 
you also need to install the following Modbus library:

- "ModbusMaster" by Doc Walker

You can install it via the Library Manager.
*/

#include <ModbusMaster.h>
#include <U8g2lib.h>

#define S0  1
#define S1  0

ModbusMaster node;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
int fontWitdh;
int fontHigh;

unsigned long CurrentTime,preTime;
const int intervalSwitch = 1000;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial2.begin(9600);

    pinMode(S0, OUTPUT);
    pinMode(S1, OUTPUT);

    digitalWrite(S0, HIGH);
    digitalWrite(S1, LOW); 

    u8g2.begin();
    
    u8g2.setFont(u8g2_font_8x13_mf);
    u8g2.setFontPosTop();
    fontWitdh = u8g2.getMaxCharWidth();
    fontHigh = u8g2.getMaxCharHeight();

}

void loop() {
  // put your main code here, to run repeatedly:
    CurrentTime = millis();
   if(CurrentTime - preTime > intervalSwitch){
    preTime = CurrentTime;

	  uint8_t result;
    uint16_t temperature,humdidity;

    node.begin(1,Serial2);

           // node.readCoils();
           // node.readDiscreteInputs();
           // node.readHoldingRegisters();
           // node.readWriteMultipleRegisters();
           
           // node.writeMultipleCoils();
           // node.writeMultipleRegisters();
           // node.writeSingleCoil();
           // node.writeSingleRegister();

    result = node.readInputRegisters(1,2);

    if(result == node.ku8MBSuccess)
    {
        temperature = node.getResponseBuffer(0);
        Serial.print("temperature: ");
        Serial.println((float)temperature/10);

        humdidity = node.getResponseBuffer(1);
        Serial.print("humdidity: ");
        Serial.println((float)humdidity/10);        
    }

    u8g2.setFont(u8g2_font_8x13_mf);

    u8g2.clearBuffer();
    u8g2.setCursor(0,fontHigh);
    u8g2.print("temp:");
    u8g2.setCursor(0,fontHigh * 2);
    u8g2.print("humdi:");   

    u8g2.setCursor((fontWitdh * 4) + 8,fontHigh);
    u8g2.print((float)temperature/10);

    u8g2.setCursor(fontWitdh * 4 + 8,fontHigh * 2);
    u8g2.print((float)humdidity/10);

    u8g2.sendBuffer(); 
   } 
}
