#include <BlynkSimpleEsp32.h>
#include <uTimerLib.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <FS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <LiquidCrystal.h>


//TEMP SMOOTHING  readDHT()

const int numReadings = 10;
int readings[numReadings];      // the readings from the analog input
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int tempaverage = 0;                // the average


//start multitasking timer based on millis()
//timer for DHT polling
unsigned long previousMillis = 0;
  int intervalButton = 10000;
unsigned long timerDHT = 0;
  int intervalDHT = 3000; //18.000 ms = 3 min
//DHT power factor
float factorDHT = 1;

//LCD
LiquidCrystal lcd(22, 23, 5, 18, 19, 21);       //esp32 pins
//LiquidCrystal lcd(8, 9 ,4, 5, 6, 7);           // Arduino PINS

//LCD KEYPAD
//int lcd_key     = 0;   //ADC0 on Arduino
#define keyPin 26
int lcd_key = 0;
int adc_key_in  = 0;
int heatdelay = 30000;  //will use this to delay heater restart - 30 secs

#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

//DHT PIN
#define DHTPIN 13

#define heatRelayPin 32
#define heatRelayLED 33

float setpoint = 16;
int defsetpoint = setpoint;
int heatRelay = 0; //default for LOW
const char* Relay1 = "UNCONFIGURED";


//Initialize Preferences
Preferences preferences;

//THINGSPEAK
const char* host = "api.thingspeak.com";
String api_key = "--THINGSPKAPITKN";

//Blynk
char auth[] = "--BLYNKAPITKN";
#define BLYNK_PRINT Serial
BlynkTimer timer;
float blynkPin = 0;
BLYNK_WRITE(V1)
{
  blynkPin = param.asInt(); // assigning incoming value from pin V1 to a variable
}

int buttonOne = 0;
BLYNK_WRITE(V6)
{
buttonOne = param.asInt(); // assigning incoming value from pin V1 to a variable    
}
//WIFI INFORMATION
const char* ssid = "HUAWEI-B310-F6D1";
const char* password =  "--WIFIPWDTKN";
//M513MEM16B0
//WIFI CLIENT SERVER ? FIXME
//WiFiServer server(80);
//DHT assign
DHTesp dht;
int t = 16;
int h = 30;
//Internal LED
#define internalLed 2

void setup() {
  Serial.begin(115200);
  //Reset smoothing array
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
//  lcd.begin(16,2);
//  lcd.clear();
//  delay(3000);
  configureHeat();
  configureWifi();
//  setupLCD();
  simpleTimer();
}

void loop() {
  Blynk.run();
  timer.run();
//  readDHT();
//  confHeaterRelay();
  setHeaterRelay();
  //  tempLCD();
  configureSetpoint();
  reset();
}


//##########FUNCTIONS

void configureHeat()
{
  //HEAT RELAY

  pinMode(heatRelayPin, OUTPUT);
  pinMode(heatRelayLED, OUTPUT);

  //TEMP
  pinMode(DHTPIN, INPUT);

  //Internal LED
  pinMode(internalLed, OUTPUT);
  //initialize DHT sensor
  dht.setup(DHTPIN);
}

void configureWifi()
{
  //Configure WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)   {
    Serial.println("Connecting to WiFi..");
    lcd.setCursor(1,0);
    lcd.print("Wifi start");
    lcd.setCursor(0,1);
    lcd.print("Connecting .");
    digitalWrite(internalLed, HIGH);
//    delay(500);
    lcd.setCursor(0,1);
    lcd.print("Connecting ..");
    digitalWrite(internalLed, LOW);
//    delay(500);
    lcd.setCursor(0,1);
    lcd.print("Connecting ...");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to the WiFi network");
    lcd.setCursor(0,1);
    lcd.print("Connected OK!");
//    delay(500);
    lcd.setCursor(1,0);
    lcd.print("Connected OK!");
    lcd.setCursor(0,1);
    lcd.print("IP ");
    lcd.print(WiFi.localIP());
    Serial.println(WiFi.localIP());
    digitalWrite(internalLed, HIGH);
//    delay(500);
  }
}


void simpleTimer()
{
  //BLYNK
  Blynk.begin(auth, ssid, password);
  timer.setInterval(1000L, sendBlynkData);
//  timer.setInterval(2000L, blynkSerial)
  timer.setInterval(5000L, confHeaterRelay);//1 minute
  timer.setInterval(2000L, readDHT);
}

void setupLCD()
{
  lcd.clear();
  lcd.setCursor(1,0);
  lcd.print("Start OK");

  // go to row 1 column 0, note that this is indexed at 0
  lcd.setCursor(0,1);
  lcd.print ("Termostat ESP32");
//  delay(100);

}

void tempLCD()
{
  if (isnan(t)) {
    //Serial.println("Error reading temperature!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error read t/h");
  }
  else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Tmp");
    lcd.setCursor(4, 0);
    lcd.print("|");
    lcd.setCursor(0, 1);            // move to the begining of the second line
    int temp = t;
    lcd.print(temp);
    lcd.setCursor(3, 1);
    lcd.print("C");
    lcd.setCursor(4, 1);
    lcd.print("|");
    lcd.setCursor(6, 0);
    lcd.print("Hum");
    lcd.setCursor(10, 0);
    lcd.print("|");
    lcd.setCursor(6, 1);
    int hum = h;
    lcd.print(hum);
    lcd.setCursor(9, 1);
    lcd.print("%");
    lcd.setCursor(10, 1);
    lcd.print("|");
    lcd.setCursor(12, 0);
    lcd.print("SET");
    lcd.setCursor(12, 1);
    int setpt = setpoint;
    lcd.print(setpt);
    lcd.setCursor(15, 1);
    lcd.print("C");
  }
}

void setHeaterRelay()
{
  //heatRelayPin
  if (heatRelay == 0) //HIGH means OFF
  {
//    delay(1000);
    digitalWrite(heatRelayPin, HIGH);
    digitalWrite(heatRelayLED, HIGH);
  }
  else if (heatRelay != 0 || isnan(heatRelay))
  {
//    delay(1000);
    digitalWrite(heatRelayPin, LOW);
    digitalWrite(heatRelayLED, LOW);
  }
}

void confHeaterRelay()
{
  if (isnan(t))
  {
    Relay1 = "ERROR DHT";
    heatRelay = 0;
  }
  else if (setpoint > t && t > 16)
  {
    Relay1 = "HEATER ON";
    heatRelay = 1;
  }
  else if (setpoint <= t) //change for setpoint from prefs.
  {
    Relay1 = "HEATER OFF";
    heatRelay = 0;
  }
}

void sendBlynkData()
{
//  if (blynkPin > defsetpoint)
//  {
//  Blynk.virtualWrite(V1, blynkPin);
//  }
  Blynk.virtualWrite(V2, setpoint);
  Blynk.virtualWrite(V3, t);
  Blynk.virtualWrite(V4, h);
  Blynk.virtualWrite(V5, Relay1);
  String content = "";
  char charcter;
  content = Serial.read();
//  content.concat(character);
  Blynk.virtualWrite(V7, content);

}

void readDHT()
{

  //while DHT is not working, signal internal LED.Don't SEND data.
  if (isnan(h) || isnan(t))
  {
    digitalWrite(internalLed, LOW);
    delay(800);
    digitalWrite(internalLed, HIGH);
    delay(50);
//    Serial.println("DHT Sensor Error!");
    Relay1 = "Error DHT!";
  }
  //Preferences
  preferences.begin("Heating", false);
  //DHT SENSOR
  t = dht.getTemperature();
  t -= factorDHT;//correct for DHT error - DHT is over so division
  h = dht.getHumidity();
  h -= factorDHT; 

//Smoothing
  // subtract the last reading:
  total = total - readings[readIndex];
  // read from the sensor:
  readings[readIndex] = t;
  // add the reading to the total:
  total = total + readings[readIndex];
  // advance to the next position in the array:
  readIndex = readIndex + 1;

  // if we're at the end of the array...
  if (readIndex >= numReadings) {
    // ...wrap around to the beginning:
    readIndex = 0;
  }

  // calculate the average:
  tempaverage = total / numReadings;
  t = tempaverage;

  
//   if (buttonOne != 0) 
//   {
  Serial.print("Tavg: ");
  Serial.print(tempaverage);
  Serial.print(" ");
  Serial.print("T: ");
  Serial.print(t);
  Serial.print("  ");
  Serial.print("H: ");
  Serial.print(h);
  Serial.print(" ");
  Serial.print("Set: ");
  Serial.print(setpoint);
  Serial.print(" ");
  Serial.print("Rs: ");
  Serial.print(heatRelay);
  Serial.print(" ");
  Serial.print("Rv: ");
  Serial.print(Relay1);
  Serial.print(" ");
  Serial.print("MemS: ");
  Serial.print(preferences.getInt("setpoint"));
  Serial.print(" ");
  Serial.print("Bp: ");
  Serial.println(blynkPin);
//   }
  Send_Data();
}



void Send_Data()
{

  // map the moist to 0 and 100% for a nice overview in thingspeak.

  //  value = constrain(value,0,5000);
  //  value = map(value,0,5000,100,0);
  //
  //  Serial.println(" Prepare to send data ");
  //  Serial.print(t);
  //  Serial.print(" ");
  //  Serial.print(h);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;

  const int httpPort = 80;

  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  else
  {
    String data_to_send = api_key;
    data_to_send += "&field1=";
    data_to_send += String(t);
    data_to_send += "&field2=";
    data_to_send += String(h);
    data_to_send += "&field3=";
    data_to_send += String(setpoint);
    data_to_send += "&field4=";
    data_to_send += String(heatRelay);
    data_to_send += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + api_key + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(data_to_send.length());
    client.print("\n\n");
    client.print(data_to_send);

//    delay(1000);
  }

  client.stop();
}

void configureSetpoint()
{
  preferences.begin("Heater",false);
  int storedSetpoint = preferences.getInt("setpoint");
//  if (blynkPin == 0 || isnan(blynkPin) && storedSetpoint == 0 || isnan(storedSetpoint))
//    { 
////      preferences.putInt("setpoint",setpoint);  
//      blynkPin = setpoint;
//    }
//  else
//  {
//    setpoint = storedSetpoint;
//  }
  if (blynkPin != 0)
  {
    preferences.putInt("setpoint",blynkPin);
    setpoint = blynkPin;
  }
  if (storedSetpoint != 0)
  {
    setpoint = storedSetpoint;
  }
  if (blynkPin == 0)
  {
    blynkPin = preferences.getInt("setpoint");
  }
//  Serial.print(setpoint);
//  Serial.print(" ");
//  Serial.print(blynkPin);
//  Serial.print(" ");
//  Serial.println(storedSetpoint);
  preferences.end();
}


void reset()
{
  if (buttonOne != 0) 
  {
//  Serial.println("REMOTE RESET REQUESTED!!");
//  Relay1 = "DEBUG";
  unsigned long currentMillis = millis();
//  if ((unsigned long)(currentMillis - previousMillis >= intervalButton))
//  {
      Serial.println("REMOTE RESET");
      Relay1 = "RESET 10s";
//      Serial.print(time1);
//      Serial.print(" ");
//      Serial.println(ButtonTimer);
        previousMillis = currentMillis;
      ESP.restart();
//   }
  }
//  ESP.restart();
  }

