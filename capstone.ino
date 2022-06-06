#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#define refRMS 200
#define refSPL 80
#define wifiPin 14
#define bluetoothPin 12
#define httpPin 13
#define soundPin A0

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int sampleWindow = 50;
unsigned int sample;
unsigned int counting;
unsigned int accSound = 0;
unsigned int temp = 0;
int LOC = 101;
int wifiTry = 0;
int MODE = 0; // 0: measuring, 1: bluetooth
String readStr;

void clearLine(int line) {
  lcd.setCursor(0, line);
  lcd.print("                ");
  lcd.setCursor(0, line);
}

void httpPOST(int sound, int vibration) {
  StaticJsonDocument<JSON_OBJECT_SIZE(4)> doc;

  doc["auth"] = "a729b64d390a0d3b4c39445419cc9ab2";
  doc["location"] = LOC;
  doc["sound"] = sound;
  doc["vibration"] = vibration;

  String body = "";
  serializeJson(doc, body);
  
  HTTPClient http;

  http.begin("http://capstone-design-api-server.herokuapp.com/api/data");
  http.addHeader("Content-Type", "application/json");

  int breakPoint = true;

  digitalWrite(httpPin, HIGH);
  clearLine(0);
  lcd.print("HTTP Protocol");
  clearLine(1);
  lcd.print("Sound: ");
  lcd.print(sound);

  while (breakPoint) {
  int httpResponseCode = http.POST(body);

    if (httpResponseCode > 0){
        String response = http.getString();
        Serial.println(httpResponseCode);
        Serial.println(response);
        breakPoint = false;
        clearLine(1);
        lcd.print("HTTP SUCCESS");
        delay(1000);
      } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
        clearLine(1);
        lcd.print("HTTP ERROR");
      }
    }
    http.end();
    digitalWrite(httpPin, LOW);

    // LCD
    clearLine(0);
    lcd.print("Measuring(");
    lcd.print(LOC);
    lcd.print(")");
    clearLine(1);
    lcd.print("Sound: ");
  }

void bluetoothMode() {
  while(Serial.available()) {
    delay(10);
    char temp = Serial.read();
    if (temp == ';') {
      break;
    }
    readStr += temp;
  }

  if (readStr.length() > 0) {
    // change mode
    if (readStr == "start") {
      MODE = 0;
      Serial.println("MODE: measuring");
      digitalWrite(bluetoothPin, LOW);
      clearLine(0);
      lcd.print("Measuring(");
      lcd.print(LOC);
      lcd.print(")");
      clearLine(1);
      lcd.print("Sound: ");
    }
    // set ssid
    if (readStr.substring(0, 5) == "ssid:") {
      Serial.print("Set ssid: ");
      String ssid = readStr.substring(5, readStr.length());
      Serial.println(ssid);
      for (int i = 0; i < ssid.length(); i++) {
        EEPROM.write(i, (char)ssid.charAt(i));
      }
      EEPROM.write(ssid.length(), ';');
      EEPROM.commit();
      clearLine(1);
      lcd.print("SSID changed");
    }
    // set pw
    if (readStr.substring(0, 3) == "pw:") {
      Serial.print("Set pw: ");
      String password = readStr.substring(3, readStr.length());
      Serial.println(password);
      for (int j = 0; j < password.length(); j++) {
        EEPROM.write(50 + j, (char)password.charAt(j));
      }
      EEPROM.write(50 + password.length(), ';');
      EEPROM.commit();
      clearLine(1);
      lcd.print("PW changed");
    }
    // set pw
    if (readStr.substring(0, 4) == "loc:") {
      Serial.print("Set LOC: ");
      String tempLoc = readStr.substring(4, readStr.length());
      Serial.println(tempLoc);
      for (int j = 0; j < tempLoc.length(); j++) {
        EEPROM.write(100 + j, (char)tempLoc.charAt(j));
      }
      EEPROM.write(100 + tempLoc.length(), ';');
      EEPROM.commit();
      clearLine(1);
      lcd.print("location changed");
      LOC = tempLoc.toInt();
    }
    // reset Wifi
    if (readStr.substring(0, 9) == "resetwifi") {
      String tempSSID;
      String tempPW;
    
      for(int i = 0; i < 50; i++) {
        if (EEPROM.read(i) == ';') break;
        tempSSID += (char)EEPROM.read(i);
      }
      tempSSID.trim();
      const char* ssid = tempSSID.c_str();
      
      for(int j = 0; j < 50; j++) {
        if (EEPROM.read(j + 50) == ';') break;
        tempPW += (char)EEPROM.read(j + 50);
      }
      tempPW.trim();
      const char* password = tempPW.c_str();
      resetWifi(ssid, password);
    }
    readStr = "";
  }
}

void measuringMode() {
  unsigned long startMillis = millis();
  unsigned int peakToPeak = 0;
  
  unsigned int signalMax = 0;
  unsigned int signalMin = 1024;

  // Bluetooth Mode
  if (Serial.available()) {
    char temp = Serial.read();
    if (temp == 'b') {
      MODE = 1;
      clearLine(1);
      clearLine(0);
      lcd.print("MODE: Bluetooth");
      Serial.println("MODE: Bluetooth");
      digitalWrite(bluetoothPin, HIGH);
    }
  }
   
  while (millis() - startMillis < sampleWindow)
  {
     sample = analogRead(soundPin);
     if (sample < 1024)
     {
        if (sample > signalMax)
        {
           signalMax = sample;
        }
        else if (sample < signalMin)
        {
           signalMin = sample;
        }
     }
  }
  if (signalMax < signalMin) {
    peakToPeak = temp;
  } else {
    peakToPeak = signalMax - signalMin;
    temp = peakToPeak;
  }
  accSound += peakToPeak;

  Serial.print("peakToPeak: ");
  Serial.println(peakToPeak);
  lcd.setCursor(7, 1);
  lcd.print("     ");
  lcd.setCursor(7, 1);
  lcd.print(peakToPeak);

  // HTTP
  if(WiFi.status() == WL_CONNECTED && counting > 30) {
    httpPOST(accSound / counting, 0);
    counting = 0;
    accSound = 0;
  }

  counting += 1;
  delay(1000);
}

void resetWifi(const char* ssid, const char* password) {
  wifiTry = 0;
  digitalWrite(wifiPin, LOW);
  // turn OFF
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.mode(WIFI_OFF);
  }
  // turn ON
  clearLine(0);
  lcd.print("MODE: WiFi");
  clearLine(1);
  lcd.print("WiFi connecting");
  Serial.print("WiFi connecting to ");
  Serial.print(ssid);
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    wifiTry += 1;
    if (wifiTry > 20) {
      Serial.println("WiFi Error");
      clearLine(1);
      lcd.print("Wifi Error");
      digitalWrite(wifiPin, LOW);
      break;
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected");
    clearLine(1);
    lcd.print("Wifi Connnected");
    digitalWrite(wifiPin, HIGH);
  }
}

void setup() {

  // LCD
  Wire.begin(D2, D1);
  lcd.begin(16, 2);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Starting");
  
  // LED
  pinMode(wifiPin, OUTPUT); // WIFI
  pinMode(bluetoothPin, OUTPUT); // BLUETOOTH
  pinMode(httpPin, OUTPUT); // HTTP
  digitalWrite(wifiPin, LOW);
  digitalWrite(bluetoothPin, LOW);
  digitalWrite(httpPin, LOW);

  // Serial
  Serial.begin(9600);
  Serial.println("");

  // EEPROM
  EEPROM.begin(1000);
  String tempSSID;
  String tempPW;
  String tempLOC;

  // SSID
  for(int i = 0; i < 50; i++) {
    if (EEPROM.read(i) == ';') break;
    tempSSID += (char)EEPROM.read(i);
  }
  tempSSID.trim();
  const char* ssid = tempSSID.c_str();
  Serial.println(ssid);

  // PW
  for(int j = 0; j < 50; j++) {
    if (EEPROM.read(j + 50) == ';') break;
    tempPW += (char)EEPROM.read(j + 50);
  }
  tempPW.trim();
  const char* password = tempPW.c_str();
  Serial.println(password);

  // LOC
  for(int j = 0; j < 50; j++) {
    if (EEPROM.read(j + 100) == ';') break;
    tempLOC += (char)EEPROM.read(j + 100);
  }
  tempLOC.trim();
  LOC = tempLOC.toInt();
  Serial.println(LOC);

  // WiFi
  resetWifi(ssid, password);

  // LCD
  clearLine(0);
  lcd.print("Measuring(");
  lcd.print(LOC);
  lcd.print(")");
  clearLine(1);
  lcd.print("Sound: ");
}

void loop() {
  switch(MODE){
    case 0: {
      measuringMode();
      break;
    }
    case 1: {
      bluetoothMode();
      break;
    }
  }
}
