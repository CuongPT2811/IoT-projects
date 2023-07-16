#define BLYNK_TEMPLATE_ID "TMPL6pgAKvDxM"
#define BLYNK_TEMPLATE_NAME "test"
#define BLYNK_AUTH_TOKEN "VyIK9cPsMwVGqr1wH9zMMhfF3zT1epnb"
#define BLYNK_PRINT Serial
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <GravityTDS.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

const char* ssid = "";
const char* password = "";
//TDS
#define TdsSensorPin 36
#define VREF 3.5  // analog reference voltage(Volt) of the ADC
#define SCOUNT 10
GravityTDS tds;

// DS18B20 temperature sensor
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float tempC;

BlynkTimer timer;

//pH
#include "DFRobot_ESP_PH.h"
#include "EEPROM.h"

DFRobot_ESP_PH ph;
#define ESPADC 4096.0    //the esp Analog Digital Convertion value
#define ESPVOLTAGE 3300  //the esp voltage supply value
#define PH_PIN 35        //the e#define samplingInterval 20

float voltage, pHValue;
float pHmin, pHmax;
int TDSmin, TDSmax;
//Relay
const int RELAY_ph2 = 19;
const int RELAY_h2o = 18;
// const int RELAY_dd2 = 5;
const int RELAY_dd1 = 17;
const int RELAY_ph1 = 16;

void setup() {
  //LCD
  lcd.begin();
  lcd.backlight();
  //PUMP
  pinMode(RELAY_ph2, OUTPUT);
  pinMode(RELAY_h2o, OUTPUT);
  // pinMode(RELAY_dd2, OUTPUT);
  pinMode(RELAY_dd1, OUTPUT);
  pinMode(RELAY_ph1, OUTPUT);
  digitalWrite(RELAY_ph2, LOW);
  digitalWrite(RELAY_h2o, HIGH);
  // digitalWrite(RELAY_dd2, HIGH);
  digitalWrite(RELAY_dd1, LOW);
  digitalWrite(RELAY_ph1, LOW);
  // pinMode(LED,OUTPUT);
  //TDS
  pinMode(TdsSensorPin, INPUT);
  // Initialize serial communication
  Serial.begin(115200);
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Initialize Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
  //TDS sensor
  tds.begin();
  //DS18B20 temperature sensor
  sensors.begin();
  EEPROM.begin(32);  //needed to permit storage of calibration value in eeprom
  ph.begin();

  timer.setInterval(1000L, sendTDS);
  timer.setInterval(1000L, sendDS18);
  timer.setInterval(1000L, sendpH);
  timer.setInterval(1000L, displayValue);
}

int analogBuffer[SCOUNT];  // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;
int count = 1;
float averageVoltage = 0;
float tdsValue = 0;

//TDS
// median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0) {
    bTemp = bTab[(iFilterLen - 1) / 2];
  } else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}
//pH
double avergearray(int* arr, int number) {
  int i;
  int max, min;
  double avg;
  long amount = 0;
  if (number <= 0) {
    Serial.println("Error number for the array to avraging!/n");
    return 0;
  }
  if (number < 5) {  //less than 5, calculated directly statistics
    for (i = 0; i < number; i++) {
      amount += arr[i];
    }
    avg = amount / number;
    return avg;
  } else {
    if (arr[0] < arr[1]) {
      min = arr[0];
      max = arr[1];
    } else {
      min = arr[1];
      max = arr[0];
    }
    for (i = 2; i < number; i++) {
      if (arr[i] < min) {
        amount += min;  //arr<min
        min = arr[i];
      } else {
        if (arr[i] > max) {
          amount += max;  //arr>max
          max = arr[i];
        } else {
          amount += arr[i];  //min<=arr<=max
        }
      }  //if
    }    //for
    avg = (double)amount / (number - 2);
  }  //if
  return avg;
}

//sendTemperature
void sendDS18() {
  // Read DS18B20 temperature sensor value
  sensors.requestTemperatures();
  tempC = sensors.getTempCByIndex(0);
  delay(1000);
  // Print sensor values to serial monitor
  Serial.print("Water Temperature: ");
  Serial.println(tempC);

  // Send sensor values to Blynk
  if (tempC > 0) {
    Blynk.virtualWrite(V4, tempC);
  }
}

//sendTDS
void sendTDS() {
  // Read TDS sensor value
  static unsigned long analogSampleTimepoint = millis();
  // int temp=25;
  if (millis() - analogSampleTimepoint > 40U)  //every 40 milliseconds,read the analog value from the ADC
  {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);  //read the analog value and store into the buffer
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT)
      analogBufferIndex = 0;
  }
  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 800U) {
    printTimepoint = millis();
    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++)
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
    averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 4096.0;                                                                                                   // read the analog value more stable by the median filtering algorithm, and convert to voltage value
    float compensationCoefficient = 1.0 + 0.02 * (tempC - 25.0);                                                                                                                      //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
    float compensationVolatge = averageVoltage / compensationCoefficient;                                                                                                             //temperature compensation
    tdsValue = (133.42 * compensationVolatge * compensationVolatge * compensationVolatge - 255.86 * compensationVolatge * compensationVolatge + 857.39 * compensationVolatge) * 0.5;  //convert voltage value to tds value

    Serial.print("TDS Value:");
    Serial.print(tdsValue, 0);
    Serial.println("ppm");
    if (tempC > 0) {
      Blynk.virtualWrite(V0, tdsValue);
    }
  }
}

//sendpH
void sendpH() {
  count++;
  static unsigned long timepoint = millis();
  if (millis() - timepoint > 1000U)  //time interval: 1s
  {
    timepoint = millis();
    //voltage = rawPinValue / esp32ADC * esp32Vin
    voltage = analogRead(PH_PIN) / ESPADC * ESPVOLTAGE;  // read the voltage
    // Serial.print("voltage:");
    // Serial.println(voltage, 4);
    pHValue = ph.readPH(voltage, temperature);  // convert voltage to pH with temperature compensation
    Serial.print("pH:");
    Serial.println(pHValue, 4);
  }
  ph.calibration(voltage, tempC);  // calibration process by Serail CMD
  // Send sensor values to Blynk
  if (tempC > 0) {
    Blynk.virtualWrite(V1, pHValue);
  }
}

void displayValue() {
  if (tempC > 0) {
    lcd.setCursor(0, 0);
    lcd.print("TDS: ");
    lcd.print(tdsValue, 0, " PPM");

    lcd.setCursor(0, 2);
    lcd.print("Temp: ");
    lcd.print(tempC, 0, " C");
    // lcd.print(" C");}

    lcd.setCursor(0, 1);
    lcd.print("pH: ");
    lcd.print(pHValue);
  }
}

void Pump_pH() {
  if (pHValue < 6) {
    digitalWrite(RELAY_ph2, HIGH);
    delay(8000);
    digitalWrite(RELAY_ph2, LOW);
  } else if (pHValue > 6.8) {
    digitalWrite(RELAY_ph1, HIGH);
    delay(8000);
    digitalWrite(RELAY_ph1, LOW);
  } else {
    digitalWrite(RELAY_ph2, LOW);
    digitalWrite(RELAY_ph1, LOW);
  }
}

void Pump_H2o() {
  digitalWrite(RELAY_h2o, HIGH);
  delay(8000);
  digitalWrite(RELAY_h2o, LOW);
}

void Pump_TDS() {
  if (tdsValue < 800) {
    digitalWrite(RELAY_dd1, HIGH);
    delay(8000);
    digitalWrite(RELAY_dd1, LOW);
  } else if (tdsValue > 1000) {
    digitalWrite(RELAY_ph2, HIGH);
    delay(8000);
    digitalWrite(RELAY_ph2, LOW);
  } else {
    digitalWrite(RELAY_dd1, LOW);
    digitalWrite(RELAY_ph2, LOW);
  }
}

void loop() {
  Blynk.run();
  timer.run();
  pHmin = Blynk.virtualRead(V2);
  pHmax = Blynk.virtualRead(V3);
  TDSmin = Blynk.virtualRead(V5);
  TDSmax = Blynk.virtualRead(V6);

  Pump_H2o();
  sendDS18();
  displayValue();
  if (count % 10 == 0) {
    Pump_H2o();
  }
  if (count % 60 == 0) {
    Pump_pH();
    Pump_TDS();
    count = 1;
  }
}
