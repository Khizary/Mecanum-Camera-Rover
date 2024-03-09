
#include <SoftwareSerial.h>
SoftwareSerial espSerial(2,3);

bool appendflag;
char c;
String dataframe;
String finishedframe;

void setup() {
  appendflag = true;
  Serial.begin(9600);
  espSerial.begin(19200);
}

void loop() {
  if (espSerial.available()) {
    printf('data received');
     c = espSerial.read();
        // Serial.print(c);
    
  if (c == '*'){
    appendflag = true;
  }
  if(appendflag == true){
    dataframe += c;
  }
  if (c == '#') {
    appendflag = false;
    finishedframe = dataframe;
    dataframe = '\0';
  Serial.println(finishedframe);
  }
  if (dataframe.length() > 17){
    dataframe = '\0';
  }
  }
}
