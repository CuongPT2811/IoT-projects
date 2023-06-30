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
// Replace with your network credentials
// const char* ssid = "ASUS";
// const char* password = "123123123";

const char* ssid = "Realmegt";
const char* password = "12345678";


//TDS
#define TdsSensorPin 36
#define VREF 3.3  // analog reference voltage(Volt) of the ADC
#define SCOUNT 30
GravityTDS tds;

//DS18B20 temperature sensor
#define ONE_WIRE_BUS 23
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float tempC;

// //DHT11
// #define DHTPIN 22
// #define DHTTYPE DHT11
// DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

//pH
#define SensorPin 39  //pH meter Analog output to Arduino Analog Input 0
#define Offset 0.4    //deviation compensate
// #define LED 13
#define samplingInterval 20
#define printInterval 800
#define ArrayLenth 40     //times of collection
int pHArray[ArrayLenth];  //Store the average value of the sensor feedback
int pHArrayIndex = 0;

static float pHValue;
void setup() {
  lcd.init();
  lcd.backlight();
  // pinMode(LED,OUTPUT);
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
  // //DHT11
  // dht.begin();
  // timer.setInterval(1000L, sendDHT);
  timer.setInterval(1000L, sendTDS);
  timer.setInterval(1000L, sendDS18);
  timer.setInterval(1000L, sendpH);
}

//
// sum of sample point

int analogBuffer[SCOUNT];  // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;

float averageVoltage = 0;
float tdsValue = 0;
float temperature = 25;  // current temperature for compensation

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

// void sendDHT()
// {
//   float h = dht.readHumidity();
//   float t = dht.readTemperature(); // or dht.readTemperature(true) for Fahrenheit

//   if (isnan(h) || isnan(t)) {
//     Serial.println("Failed to read from DHT sensor!");
//     return;
//   }
//   Serial.println("Temp: ");
//   Serial.println(t);
//   Serial.print("Hum: ");
//   Serial.println(h);
//   // You can send any value at any time.
//   // Please don't send more that 10 values per second.
//   Blynk.virtualWrite(V3, h);
//   Blynk.virtualWrite(V2, t);
// }
void sendDS18() {
  // Read DS18B20 temperature sensor value
  sensors.requestTemperatures();
  tempC = sensors.getTempCByIndex(0);

  // Print sensor values to serial monitor
  Serial.print("Water Temperature: ");
  Serial.println(tempC);

  // Send sensor values to Blynk

  Blynk.virtualWrite(V4, tempC);
}
void sendTDS() {
  // Read TDS sensor value
  static unsigned long analogSampleTimepoint = millis();
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
    Blynk.virtualWrite(V0, tdsValue);
  }
}

void sendpH() {
  static unsigned long samplingTime = millis();
  static unsigned long printTime = millis();
  static float voltage;
  if (millis() - samplingTime > samplingInterval) {
    pHArray[pHArrayIndex++] = analogRead(SensorPin);
    if (pHArrayIndex == ArrayLenth) pHArrayIndex = 0;
    voltage = avergearray(pHArray, ArrayLenth) * 3.5 / 4096;
    pHValue = 3.5 * voltage + Offset;
    samplingTime = millis();
  }
  if (millis() - printTime > printInterval)  //Every 800 milliseconds, print a numerical, convert the state of the LED indicator
  {
    Serial.print("pH value: ");
    Serial.println(pHValue, 2);
    // digitalWrite(LED,digitalRead(LED)^1);
    printTime = millis();
  }
  // Send sensor values to Blynk
  Blynk.virtualWrite(V1, pHValue);
}
void displayValue(){
 lcd.setCursor(0, 0);
  lcd.print("TDS: ");
  lcd.print(tdsValue, 0);
  lcd.print(" PPM");

  lcd.setCursor(0, 1);
  lcd.print("Temp: ");
  lcd.print(tempC, 0);
  lcd.print(" C");

  lcd.setCursor(0, 2);
  lcd.print("pH: ");
  lcd.print(pHValue);

}
void loop() {
 


  Blynk.run();
  timer.run();
  displayValue();
  // Wait for 1 second
  delay(1000);
  
}
