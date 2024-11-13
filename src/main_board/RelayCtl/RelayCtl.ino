#include <ModbusRtu.h>
#include <SoftwareSerial.h>

#define S0  1
#define S1  0

SoftwareSerial mySerial(2, 3); // RX, TX

// data array for modbus network sharing
uint16_t au16data[16];
uint8_t u8state;
uint8_t u8Query;

/**
 *  Modbus object declaration
 *  u8id : node id = 0 for master, = 1..247 for slave
 *  port : serial port
 *  u8txenpin : 0 for RS-232 and USB-FTDI 
 *               or any pin number > 1 for RS-485
 */
Modbus master(0,mySerial); // this is master and RS-232 or USB-FTDI via software serial
/**
 * This is an structe which contains a query to an slave device
 */
modbus_t telegram[2];

unsigned long u32wait;

unsigned long CurrentTime,preTime;
const int intervalSwitch = 1000 *5;

void setup()
{
	Serial.begin(115200);
    mySerial.begin(9600); 
    pinMode(S0, OUTPUT);
    pinMode(S1, OUTPUT);

    digitalWrite(S0, LOW);
    digitalWrite(S1, LOW);  

    //Relay 光偶合
    telegram[0].u8id = 3;
    telegram[0].u8fct = 2;
    telegram[0].u16RegAdd = 0;
    telegram[0].u16CoilsNo =0x08;
    telegram[0].au16reg = au16data;    

    //Relay 繼電器
    telegram[1].u8id = 3;
    telegram[1].u8fct = 5;
    telegram[1].u16RegAdd = 0;
    telegram[1].u16CoilsNo = 0;
    telegram[1].au16reg = au16data+2;    
  

    master.start(); // start the ModBus object.
    master.setTimeOut( 2000 ); // if there is no answer in 2000 ms, roll over    
    u32wait = millis() + 1000;
}

void loop()
{
	uint8_t c = 0;

    switch( u8state )
    {
        case 0: 
            
            digitalWrite(S0, HIGH);
            digitalWrite(S1, LOW);          
            if (millis() > u32wait) u8state++; // wait state           
        break;

        case 1:
            Serial.print("u8Query: ");
            Serial.println(u8Query);        
            master.query(telegram[u8Query]);
            u8Query++;
            u8state++;

            if(u8Query == 1) (telegram[1].au16reg[0] == 1) ? (telegram[1].au16reg[0] = 0):(telegram[1].au16reg[0] = 1);  
            if(u8Query > 1) u8Query = 0;

        break;

        case 2:
            master.poll();
           
            if(master.getState() == COM_IDLE){
                u8state++;
                u32wait = millis() + 5000; 
            }        
        break;

        case 3:
            
                 

               for(int x = 0; x < 16; x++){
                    Serial.print(x);
                    Serial.print(" : ");
                    Serial.println(au16data[x]);
                }

             u8state++;  
        break;

        case 4:
            if (millis() > u32wait) u8state = 0; // wait state   
        break;




    }
}
