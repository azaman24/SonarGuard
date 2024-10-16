#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"

#define greenLed 2
#define yellowLed 16
#define redLed 17
#define buzzer 15
#define trigPin 0
#define echoPin 4

long duration;
float distance;
int greenCounter, yellowCounter, redCounter;
int buzzerState = HIGH;
 
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
uint8_t txValue = 0;
long lastMsg = 0;
String rxload="\n";
 
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" 
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define SYSTEM_NAME "SonarGuard Security"
 
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Device connected");

    delay(500);
    pCharacteristic->setValue("Meet SonarGuard your mini security system");
    pCharacteristic->notify();
    delay(500);
  };
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Device disconnected");
  }
};
 
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0) {
      rxload="";
      for (int i = 0; i < rxValue.length(); i++){
        rxload +=(char)rxValue[i];
      }
      if (rxValue == "alarm on") {
        buzzerState = HIGH;
      } else if (rxValue == "alarm off") {
        buzzerState = LOW;
      }
    }
  }
};
 
void setupBLE(String BLEName){
  const char *ble_name=BLEName.c_str();
  BLEDevice::init(ble_name);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID); 
  pCharacteristic= pService->createCharacteristic(CHARACTERISTIC_UUID_TX,BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->addDescriptor(new BLE2902());
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX,BLECharacteristic::PROPERTY_WRITE);
  pCharacteristic->setCallbacks(new MyCallbacks()); 
  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void setup() {
  Serial.begin(9600);
  setupBLE("ESP32_Bluetooth");
  pinMode(greenLed, OUTPUT);
  pinMode(yellowLed, OUTPUT);
  pinMode(redLed, OUTPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, buzzerState);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void notifyAndPrint(const String& alert, int ledPin) {
  pCharacteristic->setValue(alert.c_str());
  pCharacteristic->notify();
  Serial.println(alert);
  digitalWrite(ledPin, HIGH);
  Serial.println("Distance: " + String(distance) + " cm");
}

void loop() {
  digitalWrite(buzzer, LOW);
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long now = millis();
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.0344 / 2;

  if (now - lastMsg > 100) {
    if(distance < 10 && distance > 0){
      notifyAndPrint("Close Object Detected!", redLed);
      redCounter++;

      if (redCounter > 5 || redCounter == 10) {
        digitalWrite(buzzer, buzzerState);
        delay(redCounter == 5 ? 3000 : 7000);
        digitalWrite(buzzer, LOW);
      }

      delay(3000);
    } 
    else if (distance > 10 && distance < 30) {
      notifyAndPrint("Nearby Object Detected!", yellowLed);
      yellowCounter++;
      delay(3000);
    } 
    else if (distance > 30 && distance < 50) {
      notifyAndPrint("Object Detected", greenLed);
      greenCounter++;
      delay(3000);
    } else {
      digitalWrite(greenLed, LOW);
      digitalWrite(yellowLed, LOW);
      digitalWrite(redLed, LOW);
    }

    if (deviceConnected&&rxload.length()>0) {
      Serial.println(rxload);
      if (rxload == "check activity") {
        char activity[100];
        sprintf(activity, "Close Detections: %d, Nearby Detections: %d, Far Detections: %d", redCounter, yellowCounter, greenCounter);

        pCharacteristic->setValue(activity);
        pCharacteristic->notify();
      } else if (rxload == "alarm off") {
        pCharacteristic->setValue("Alarm Turned OFF");
        pCharacteristic->notify();
      } else if (rxload == "alarm on") {
        pCharacteristic->setValue("Alarm Turned ON");
        pCharacteristic->notify();
      }
      rxload="";
    }

    lastMsg = now;
  }
}
