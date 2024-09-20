
#include <Wire.h>

#define Example1 
#define EEPROM_ADDRESS  0x50

uint8_t Data1[8] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
uint8_t Data2[8] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
//uint8_t DataOut[8] ={0};

uint8_t x;
int button = 8;

int outState  = 1;
int oldState= 1;


void button_handler(uint32_t id, uint32_t event) {
    if (outState == 0) {
        // turn on LED
        outState = 1;
        //digitalWrite(led, outState);
       // Serial.println("test 1");
    } else {
        // turn off LED
        outState = 0;
        //digitalWrite(led, outState);
        //Serial.println("test 0");
    }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("EEPROM TEST");

  pinMode(button, INPUT_IRQ_FALL);
  digitalSetIrqHandler(button, button_handler);  

  Wire.begin();
     
}

void loop() {
  // put your main code here, to run repeatedly:
  if(oldState != outState)
  {
    oldState = outState;
    Serial.print("State= ");
    
    
    if(oldState == 1)
    {
      Serial.println(oldState);
      WriteBytes(0,Data1,8);
    }

    if(oldState == 0)
    {
      Serial.println(oldState);
      WriteBytes(0,Data2,8);
    }

  delay(10);

  uint8_t DataOut[8] ={0};
  ReadBytes(0,DataOut,8);

  for(x = 0; x < 8; x++)
  {
    Serial.println(DataOut[x],HEX);
  }    

  }
}

/*
void WriteByte(uint8_t data_addr, uint8_t data)
{
  
  Wire.beginTransmission(EEPROM_ADDRESS);
  Wire.write(data_addr);
  Wire.write(data);
  Wire.endTransmission();
}

uint8_t ReadByte(uint8_t data_addr)
{
  uint8_t data;
  Wire.beginTransmission(EEPROM_ADDRESS);
  Wire.write(data_addr);
  Wire.endTransmission();
  Wire.requestFrom(EEPROM_ADDRESS,1);

  if(Wire.available())
  {
    data = Wire.read();  
  }
  //Wire.endTransmission();
  
  return data;
}
*/

void ReadBytes(uint8_t data_addr,uint8_t *data, int len)
{
  uint8_t x;
  Wire.beginTransmission(EEPROM_ADDRESS);
  Wire.write(data_addr);
  Wire.endTransmission();
  Wire.requestFrom(EEPROM_ADDRESS,len);
  delay(1);
  if(Wire.available())
  {
    for(x = 0; x < len;x++)
    {
      data[x] = Wire.read();    
    }
    
  }
 
  
}


void WriteBytes(uint8_t data_addr, uint8_t *data,uint8_t len)
{

  uint8_t x;
  Wire.beginTransmission(EEPROM_ADDRESS);
  Wire.write(data_addr);
  for(x = 0; x < len; x++)
  {
    Wire.write(data[x]);  
  }
  
  Wire.endTransmission();
}
