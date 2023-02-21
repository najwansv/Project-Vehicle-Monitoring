#include <LiquidCrystal_I2C.h> // include the LiquidCrystal_I2C library
LiquidCrystal_I2C lcd(0x27, 16, 2);

#include <SPI.h> // include the SPI library
#include <MFRC522.h> // include the MFRC522 library

// define the pins used
#define SS_PIN 2 // SS pin of the MFRC522
#define RST_PIN 16 // RST pin of the MFRC522
#define pinRelay 0 // Pin Relay
#define speedSensor 3 // Pin Speed Sensor
#define waterSensor A0 // Pin Water Sensor


#include <ArduinoMqttClient.h> // include the ArduinoMqttClient library
#include <ESP8266WiFi.h> // include the ESP8266WiFi library

char ssid[] = "cieciecie";    // your network SSID (name)
char pass[] = "17171717";    // your network password (use for WPA, or use as key for WEP)

WiFiClient wifiClient; // create an instance of the WiFiClient class
MqttClient mqttClient(wifiClient); // create an instance of the MqttClient class


const char broker[] = "18.141.140.63"; // MQTT broker address
int        port     = 1883; // MQTT broker port

// MQTT topics
const char topicFuel[]  = "vehicle/fuel"; 
const char topicSpeedRpm[]  = "vehicle/speed/rpm";
const char topicSpeedRps[]  = "vehicle/speed/rps";
const char topicStartMotorOn[]  = "vehicle/startmotor/On";
const char topicStartMotorOff[]  = "vehicle/startmotor/Off";
const char topicSwitchMotor[]  = "vehicle/switchmotor";

// initialize variables
const long interval = 1000;
unsigned long previousMillis = 0;

int count = 0;

int sensorValue = 0; 
float waterHeight = 0; 
float sensorVoltage = 0;

int maxVal = 1023; 
float sensorLength = 10.0;

unsigned long start_time = 0;
unsigned long end_time = 0;
unsigned long current_time = 0;
int steps=0;
float steps_old=0;
float temp=0;
float rps=0;
float rpm=0;

int condition;

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

String strID;

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  lcd.init();
  lcd.backlight();
  pinMode(speedSensor,INPUT_PULLUP);

  // attempt to connect to WiFi network:
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    lcd.setCursor(0,0);
    lcd.print("Connecting to");
    lcd.setCursor(0,1);
    lcd.print("the network");
    Serial.print(".");
    delay(5000);
  }
  lcd.clear();

  Serial.println("You're connected to the network");
  Serial.println();

  lcd.setCursor(0,0);
  lcd.print("Connected to");
  lcd.setCursor(0,1);
  lcd.print("the network");
  delay(2000);
  lcd.clear();

  // attempt to connect to the MQTT broker:
  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1);
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  // subscribe to the topic and set callback function
  mqttClient.onMessage(SwitchMotor);
  mqttClient.subscribe(topicSwitchMotor);

  lcd.setCursor(0,0);
  lcd.print("Connected to");
  lcd.setCursor(0,1);
  lcd.print("the MQTT broker");
  delay(2000);
  lcd.clear();
  
  lcd.setCursor(0,0);
  lcd.print(" Fuel :");
  lcd.setCursor(0,1);
  lcd.print(" RPM  :");

  SPI.begin();
  rfid.PCD_Init();
  Serial.println("I am waiting for card...");
  pinMode(pinRelay, OUTPUT);
  digitalWrite(pinRelay, HIGH);
}

void loop() {

  mqttClient.poll(); // check for incoming messages

  FuelSensor(); // Fuel Sensor
  SpeedMotor(); // Speed Sensor
  StartMotor(); // Start Motor with RFID
}

void SpeedMotor(){
  start_time=millis();
  end_time=start_time+1000;
  
  while(millis()<end_time){
    yield();
      if(digitalRead(speedSensor)){
          steps=steps+1; 
          while(digitalRead(speedSensor)){
            yield();
          }
      }
  }
  // calculate the speed
  temp=steps-steps_old;
  steps_old=steps;
  rps=(temp/20);
  rpm=(rps*60);
  lcd.setCursor(9,1);
  lcd.print(rpm);
  lcd.print("   ");

  // publish the message
  mqttClient.beginMessage(topicSpeedRpm);
  mqttClient.print(rpm);
  mqttClient.endMessage();

  mqttClient.beginMessage(topicSpeedRps);
  mqttClient.print(rps);
  mqttClient.endMessage();
}

void FuelSensor(){
  sensorValue = analogRead(waterSensor);
  waterHeight = sensorValue*sensorLength/maxVal;
  lcd.setCursor(9,0);
  lcd.print(waterHeight);
  lcd.print("L");

  // publish the message
  mqttClient.beginMessage(topicFuel);
  mqttClient.print(waterHeight);
  mqttClient.endMessage();
}

void StartMotor(){
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
      return;

  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  strID = "";
  for (byte i = 0; i < rfid.uid.size; i++){
      strID +=
      (rfid.uid.uidByte[i] < 0x10 ? "0" : "") +
      String(rfid.uid.uidByte[i], HEX) +
      (i != rfid.uid.size - 1 ? ":" : "");
  }

  strID.toUpperCase();
  
  if (strID == "B4:E9:AC:07") { // card ID
    if (condition == 0) {
    
      digitalWrite(pinRelay, LOW);
      Serial.println("relay nyala");

      mqttClient.beginMessage(topicStartMotorOn);
      mqttClient.print("Motorcycle Start");
      mqttClient.endMessage();

      condition = 1;
    } else if (condition == 1) {
      digitalWrite(pinRelay, HIGH);
      Serial.println("relay mati");

      mqttClient.beginMessage(topicStartMotorOff);
      mqttClient.print("Motorcycle Stop");
      mqttClient.endMessage();
      condition = 0;
    }
  } else {
    Serial.println("Card ID not found");
  }

  Serial.print("Your ID Card : ");
  Serial.println(strID);
  delay(1000);

}

void SwitchMotor(int payload) {
  // we received a message, print out the topic and contents
  Serial.print("Received a message with topic ");
  Serial.print(mqttClient.messageTopic());
  Serial.print(": ");
  Serial.print(payload);

  if (payload == true) {
    digitalWrite(pinRelay, LOW);
    Serial.println(" relay nyala");

    mqttClient.beginMessage(topicStartMotorOn);
    mqttClient.print("Motorcycle Start");
    mqttClient.endMessage();
    condition = 1;
  } 
}
