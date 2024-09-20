
#define S0  1
#define S1  0

int chr;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial2.begin(9600);
  
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);

  digitalWrite(S0, LOW);
  digitalWrite(S1, HIGH);

  delay(500);

  
  Serial2.println("TEST Serial");


}

void loop() {
  // put your main code here, to run repeatedly:
  
  if(Serial2.available()){
    while((chr = Serial2.read()) > 0){
      Serial2.println(char(chr));
    }
  }
  delay(100);
  
  //Serial2.println("TEST Serial");
  delay(500);
}
