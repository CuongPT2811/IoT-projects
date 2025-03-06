#include <Arduino.h>
#include <EspWiFi.h>
#include <DHT.h>
#include <Wire.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <DNSServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <map>
#include <LiquidCrystal_I2C.h>


#define API_KEY "AIzaSyB2KBwQgPd_XHpQKM5bg5lDg6HtGdZNnfo"
#define DATABASE_URL "https://smartplantcare-7f796-default-rtdb.firebaseio.com/"
#define USER_EMAIL "cuongtuan4198@gmail.com"
#define USER_PASSWORD "Cuong123!"

#define SOIL_MOISTURE_PIN_1 32
#define SOIL_MOISTURE_PIN_2 33

#define DHTPIN 14
#define DHTTYPE DHT22  // DHT 11

#define RELAY_WATER_PUMP_PIN_1 16
#define RELAY_WATER_PUMP_PIN_2 19
#define RELAY_LED_LIGHT_PIN 18
#define RELAY_FAN_PIN 17

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 20, 4);  // Initialize with the address and dimensions

static int counter = 0;
static int count = 0;

bool relayFanState = false;
bool relayWaterState = false;
bool relaySprayState = false;
bool relayLedState = false;  // Current state of the relay

unsigned long sendDataPrevMillis = 0;
const unsigned long timerDelay = 10 * 1000UL;

const int GMT_OFFSET_SECONDS = 25200;

String weekDays[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
String months[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November" };

FirebaseData firebaseData;
FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;
FirebaseJson json;

String databasePath = "/readings";
String temperaturePath = "/temperature";
String airHumidityPath = "/humidity";
String soilMoisturePath = "/soilMoisture";
String soilMoisture2Path = "/soilMoisture2";
String waterPumpStatePath = "/Pump/status";
String sprayPumpStatePath = "/Spray/status";
String fanStatePath = "/Fan/status";
String ledStatePath = "/Light/status";
String lastSwitchTimePath = "/LastSwitchTime";
String LedOffCyclePath = "/LedOffCycle";
String LedOnCyclePath = "/LedOnCycle";
FirebaseData firebaseDataHumDown;
FirebaseData firebaseDataHumUp;
FirebaseData firebaseDataSoilDown;
FirebaseData firebaseDataSoilUp;
FirebaseData firebaseDataTempDown;
FirebaseData firebaseDataTempUp;
FirebaseData firebaseDataLightOff;
FirebaseData firebaseDataLightOn;
FirebaseData firebaseDataLastTime;

bool isLastSwitchTimeReset = false;  // Biến trạng thái kiểm soát việc chạy
unsigned long lastLogTime = 0;       // Lưu thời gian (millisecond) của lần gửi log cuối cùng
bool ledOn = false;           // Trạng thái hiện tại của LED (bật hoặc tắt)
bool inOffCycle = true;       // Xác định LED đang ở chu kỳ tắt nào
unsigned long timeOffCycle, timeOnCycle;
unsigned long lastSwitchTime = 0;  // Thời gian bật/tắt của đèn
unsigned long lastSwitchTime1 = 0;
unsigned long currentMillis = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);
String timestamp;
void setup() {
  Serial.begin(115200);

  lcd.begin();
  lcd.backlight();
  dht.begin();
  initWiFi();
  timeClient.begin();
  timeClient.setTimeOffset(GMT_OFFSET_SECONDS);

  pinMode(SOIL_MOISTURE_PIN_1, INPUT);
  pinMode(SOIL_MOISTURE_PIN_2, INPUT);
  pinMode(RELAY_WATER_PUMP_PIN_1, OUTPUT);
  pinMode(RELAY_WATER_PUMP_PIN_2, OUTPUT);
  pinMode(RELAY_LED_LIGHT_PIN, OUTPUT);
  pinMode(RELAY_FAN_PIN, OUTPUT);

  digitalWrite(RELAY_WATER_PUMP_PIN_1, LOW);
  digitalWrite(RELAY_WATER_PUMP_PIN_2, LOW);
  digitalWrite(RELAY_LED_LIGHT_PIN, LOW);
  digitalWrite(RELAY_FAN_PIN, LOW);

  firebaseConfig.api_key = API_KEY;
  firebaseAuth.user.email = USER_EMAIL;
  firebaseAuth.user.password = USER_PASSWORD;
  firebaseConfig.database_url = DATABASE_URL;
  Firebase.reconnectWiFi(true);
  firebaseData.setResponseSize(4096);
  firebaseConfig.token_status_callback = tokenStatusCallback;
  firebaseConfig.max_token_generation_retry = 5;
  Firebase.begin(&firebaseConfig, &firebaseAuth);

  // Lấy thời gian bắt đầu từ Firebase
  lastSwitchTime1 = retrieveLastSwitchTimeFromFirebase();
}

void loop() {
  static unsigned long lastExecution = 0;
  currentMillis = millis();
  controlFan();
  controlWater();
  controlSpray();
  controlLed();
  processStopCommand();


  if (currentMillis - lastExecution >= 5000) {
    readTemperatureAndHumidity();
    readSoilMoisture();
    readSoilMoisture2();
    sendDataToFirebase();  // Gọi hàm gửi dữ liệu
    lastExecution = currentMillis;
  }
}

void initWiFi() {
  WiFi.begin("zero", "12345678");
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

String getRealTime() {
  timeClient.update();
  time_t epochTime = timeClient.getEpochTime();
  struct tm *timeInfo = gmtime((time_t *)&epochTime);

  // Lấy các phần của thời gian
  int currentYear = timeInfo->tm_year + 1900;
  int currentMonth = timeInfo->tm_mon + 1;
  int currentDay = timeInfo->tm_mday;
  int currentHour = timeInfo->tm_hour;
  int currentMinute = timeInfo->tm_min;
  int currentSecond = timeInfo->tm_sec;

  // Tạo chuỗi định dạng ngày giờ
  String formattedTime = String(weekDays[timeClient.getDay()]) + " " + (currentHour < 10 ? "0" : "") + String(currentHour) + ":" + (currentMinute < 10 ? "0" : "") + String(currentMinute) + ":" + (currentSecond < 10 ? "0" : "") + String(currentSecond) + " " + String(currentDay) + "-" + String(currentMonth) + "-" + String(currentYear);

  return formattedTime;
}

int getFirebaseIntData(FirebaseData &firebaseDataInstance, const String &path) {
  static std::map<String, int> cachedValues; // Bộ lưu trữ giá trị cũ (map)

  Serial.print("Fetching data from path: ");
  Serial.println(path);

  if (Firebase.RTDB.getInt(&firebaseDataInstance, path)) {
    // Nếu đọc thành công, cập nhật giá trị vào bộ nhớ đệm
    int value = firebaseDataInstance.intData();
    cachedValues[path] = value;
    Serial.print("Value at ");
    Serial.print(path);
    Serial.print(": ");
    Serial.println(value);
    return value;
  } else {
    // Nếu lỗi, trả về giá trị cũ từ bộ nhớ đệm
    Serial.print("Error reading ");
    Serial.print(path);
    Serial.print(": ");
    Serial.println(firebaseDataInstance.errorReason());

    if (cachedValues.find(path) != cachedValues.end()) {
      Serial.print("Using cached value for ");
      Serial.print(path);
      Serial.print(": ");
      Serial.println(cachedValues[path]);
      return cachedValues[path]; // Trả về giá trị cũ
    } else {
      Serial.print("No cached value available for ");
      Serial.println(path);
      return 0; // Trả về giá trị mặc định nếu không có giá trị cũ
    }
  }
}


int GetHumDown() {
  return getFirebaseIntData(firebaseDataHumDown, "/devices/HumidityDown/status");
}

int GetHumUp() {
  return getFirebaseIntData(firebaseDataHumUp, "/devices/HumidityUp/status");
}

int GetSoilDown() {
  return getFirebaseIntData(firebaseDataSoilDown, "/devices/SoilDown/status");
}

int GetSoilUp() {
  return getFirebaseIntData(firebaseDataSoilUp, "/devices/SoilUp/status");
}

int GetTempDown() {
  return getFirebaseIntData(firebaseDataTempDown, "/devices/TempDown/status");
}

int GetTempUp() {
  return getFirebaseIntData(firebaseDataTempUp, "/devices/TempUp/status");
}

int GetTimeLightOff() {
  return getFirebaseIntData(firebaseDataLightOff, "/devices/TimeLightOff/status");
}

int GetTimeLightOn() {
  return getFirebaseIntData(firebaseDataLightOn, "/devices/TimeLightOn/status");
}
unsigned long retrieveLastSwitchTimeFromFirebase() {
  return getFirebaseIntData(firebaseDataLastTime, "/readings/LastSwitchTime");
}


String retrieveData(const String &path) {
  // Thử lấy dữ liệu từ Firebase theo đường dẫn `path`
  if (Firebase.RTDB.getString(&firebaseData, path)) {
    String value = firebaseData.stringData();
    Serial.print("Value at ");
    Serial.print(path);
    Serial.print(": ");
    Serial.println(value);
    return value;
  } else {
    Serial.print("Error reading ");
    Serial.print(path);
    Serial.print(": ");
    Serial.println(firebaseData.errorReason());
    return "";  // Trả về chuỗi rỗng khi có lỗi
  }
}


void controlDevice(const String &status, int relayPin, bool &relayState) {
  if (status == "on") {
    digitalWrite(relayPin, HIGH);
    relayState = true;
  } else if (status == "off") {
    digitalWrite(relayPin, LOW);
    relayState = false;
  }
}

// Điều khiển quạt
void controlFan() {
  float currentHumidity = dht.readHumidity();
  unsigned long currentMillis = millis();
  static unsigned long previousFanMillis = 0;
  static const int delay = 2;
  static bool relayFanState = false;
  static String lastFanState = "";  // Trạng thái cuối cùng được gửi lên Firebase

  String fanStatus = retrieveData("/devices/Fan/status");
  String mode = retrieveData("/devices/Mode");

  if (mode == "Manual") {
    relayFanState = (fanStatus == "on");  // Cập nhật relayFanState dựa trên fanStatus
    controlDevice(fanStatus, RELAY_FAN_PIN, relayFanState);

  } else if (mode == "Auto") {
    if (!relayFanState && (currentHumidity >= GetHumUp())) {
      digitalWrite(RELAY_FAN_PIN, HIGH);
      relayFanState = true;
      previousFanMillis = currentMillis;
    }

    if (relayFanState && (currentHumidity < (GetHumUp() - delay))) {
      digitalWrite(RELAY_FAN_PIN, LOW);
      relayFanState = false;
    }
  }
  String currentFanState = relayFanState ? "on" : "off";
  // Chỉ gửi nếu trạng thái thay đổi
  if (currentFanState != lastFanState) {
    json.set(fanStatePath.c_str(), currentFanState);
    Firebase.RTDB.updateNode(&firebaseData, databasePath.c_str(), &json);
    lastFanState = currentFanState;
  }
}

// Điều khiển bơm nước
void controlWater() {
  unsigned long currentMillis = millis();
  static unsigned long previousWaterMillis = 0;
  static bool relayWaterState = false;
  static String lastWaterState = "";  // Trạng thái cuối cùng được gửi lên Firebase
  static const int soilHysteresis = 1;
  String waterStatus = retrieveData("/devices/Pump/status");
  String mode = retrieveData("/devices/Mode");

  if (mode == "Manual") {
    relayWaterState = (waterStatus == "on");
    controlDevice(waterStatus, RELAY_WATER_PUMP_PIN_1, relayWaterState);
  } else if (mode == "Auto") {
    if (!relayWaterState && ((readSoilMoisture() <= GetSoilDown() || readSoilMoisture2() <= GetSoilDown()))) {
      digitalWrite(RELAY_WATER_PUMP_PIN_1, HIGH);
      relayWaterState = true;
      previousWaterMillis = currentMillis;
    } else if (readSoilMoisture() > (GetSoilDown() + soilHysteresis) && readSoilMoisture2() > (GetSoilDown() + soilHysteresis)) {
      digitalWrite(RELAY_WATER_PUMP_PIN_1, LOW);
      relayWaterState = false;
    }
    // Tắt bơm sau 10s nếu đang chạy
    if (relayWaterState && (currentMillis - previousWaterMillis >= 10 * 1000UL)) {
      digitalWrite(RELAY_WATER_PUMP_PIN_1, LOW);
      relayWaterState = false;
    }
  }
  String currentWaterState = relayWaterState ? "on" : "off";
  // Chỉ gửi nếu trạng thái thay đổi
  if (currentWaterState != lastWaterState) {
    json.set(waterPumpStatePath.c_str(), currentWaterState);
    Firebase.RTDB.updateNode(&firebaseData, databasePath.c_str(), &json);
    lastWaterState = currentWaterState;
  }
}

// Điều khiển phun sương
void controlSpray() {
  float currentHumidity = dht.readHumidity();
  unsigned long currentMillis = millis();
  static unsigned long previousSprayMillis = 0;
  static bool relaySprayState = false;
  static String lastSprayState = "";  // Trạng thái cuối cùng được gửi lên Firebase

  String sprayStatus = retrieveData("/devices/Spray/status");
  String mode = retrieveData("/devices/Mode");

  if (mode == "Manual") {
    relaySprayState = (sprayStatus == "on");
    controlDevice(sprayStatus, RELAY_WATER_PUMP_PIN_2, relaySprayState);

  } else if (mode == "Auto") {
    if (!relaySprayState && (currentHumidity <= GetHumDown())) {
      digitalWrite(RELAY_WATER_PUMP_PIN_2, HIGH);
      relaySprayState = true;
      previousSprayMillis = currentMillis;
    } else if (currentHumidity > GetHumDown()) {
      digitalWrite(RELAY_WATER_PUMP_PIN_2, LOW);
      relaySprayState = false;
    }

    // Tắt bơm sau 1 phút nếu đang chạy
    if (relaySprayState && (currentMillis - previousSprayMillis >= 60 * 1000UL)) {
      digitalWrite(RELAY_WATER_PUMP_PIN_2, LOW);
      relaySprayState = false;
    }
  }

  String currentSprayState = relaySprayState ? "on" : "off";

  // Chỉ gửi nếu trạng thái thay đổi
  if (currentSprayState != lastSprayState) {
    json.set(sprayPumpStatePath.c_str(), currentSprayState);
    Firebase.RTDB.updateNode(&firebaseData, databasePath.c_str(), &json);
    lastSprayState = currentSprayState;
  }
}
// Điều khiển đèn LED
void controlLed() {
  timeOffCycle = GetTimeLightOff() * 1000 * 3600 * 24;  // Chuyển đổi ngày thành mili giây
  timeOnCycle = GetTimeLightOn() * 3600000;                // Chuyển đổi giờ thành mili giây
  currentMillis = lastSwitchTime1 + millis();
  static String lastLedState = "";
  String lightStatus = retrieveData("/devices/Light/status");
  String mode = retrieveData("/devices/Mode");
  if (mode == "Manual") {
    relayLedState = (lightStatus == "on");
    controlDevice(lightStatus, RELAY_LED_LIGHT_PIN, relayLedState);
  } else if (mode == "Auto") {
    Serial.println(currentMillis);
    if (inOffCycle) {
      // Nếu đang ở chu kỳ tắt
      if (currentMillis - lastSwitchTime >= timeOffCycle) {
        // Kết thúc chu kỳ tắt, chuyển sang chu kỳ bật
        digitalWrite(RELAY_LED_LIGHT_PIN, HIGH);  // Bật LED
        relayLedState = HIGH;                     // Cập nhật trạng thái LED
        ledOn = true;
        inOffCycle = false;              // Chuyển sang chu kỳ bật
        Serial.println(inOffCycle);
        lastSwitchTime = currentMillis;  // Đặt lại thời gian`        
        Serial.println("Chuyển sang chu kỳ bật 8 tiếng mỗi ngày");
      } else {
        // Đảm bảo LED tắt trong chu kỳ tắt
        digitalWrite(RELAY_LED_LIGHT_PIN, LOW);
        relayLedState = LOW;
      }
    } else {
      // Nếu đang ở chu kỳ bật 8 tiếng
      if (ledOn) {
        digitalWrite(RELAY_LED_LIGHT_PIN, HIGH);  // Bật LED
        relayLedState = HIGH;
        // LED đang bật, kiểm tra xem có cần tắt sau 8 tiếng không
        if (currentMillis - lastSwitchTime >= timeOnCycle) {
          digitalWrite(RELAY_LED_LIGHT_PIN, LOW);  // Tắt LED
          relayLedState = LOW;                     // Cập nhật trạng thái LED
          ledOn = false;                           // Cập nhật trạng thái ledOn
          lastSwitchTime = currentMillis;          // Đặt lại thời gian
          Serial.println("Đèn LED đã tắt sau 8 tiếng");
        }
      } else {
        // LED đang tắt, bật đèn cho chu kỳ 8 tiếng tiếp theo
        if (currentMillis - lastSwitchTime >= (24 * 3600 * 1000 - timeOnCycle)) {
          digitalWrite(RELAY_LED_LIGHT_PIN, HIGH);  // Bật LED
          relayLedState = HIGH;                     // Cập nhật trạng thái LED
          ledOn = true;                             // Cập nhật trạng thái ledOn
          lastSwitchTime = currentMillis;           // Đặt lại thời gian
          Serial.println("Đèn LED đã bật cho 8 tiếng tiếp theo");
        }
      }
    }
  }
  String currentLedState = relayLedState ? "on" : "off";
  // Chỉ gửi nếu trạng thái thay đổi
  if (currentLedState != lastLedState) {
    json.set(ledStatePath.c_str(), currentLedState);
    Firebase.RTDB.updateNode(&firebaseData, databasePath.c_str(), &json);
    lastLedState = currentLedState;
  }
}

//dừng khẩn cấp
void processStopCommand() {
  String stopCommand = retrieveData("/devices/emergency");

  if (stopCommand == "true") {
    digitalWrite(RELAY_FAN_PIN, LOW);
    digitalWrite(RELAY_WATER_PUMP_PIN_1, LOW);
    digitalWrite(RELAY_WATER_PUMP_PIN_2, LOW);
    digitalWrite(RELAY_LED_LIGHT_PIN, LOW);

    relayFanState = false;
    relayWaterState = false;
    relaySprayState = false;
    relayLedState = false;

    json.set(fanStatePath.c_str(), "off");
    json.set(waterPumpStatePath.c_str(), "off");
    json.set(sprayPumpStatePath.c_str(), "off");
    json.set(ledStatePath.c_str(), "off");

    if (Firebase.RTDB.updateNode(&firebaseData, databasePath.c_str(), &json)) {
      Serial.println("All devices turned off successfully.");
    } else {
      Serial.println("Failed to update device states.");
      Serial.println(firebaseData.errorReason());
    }
    // Đặt lại lệnh Stop trên Firebase để tránh lặp lại
    Firebase.RTDB.setString(&firebaseData, "/devices/emergency", "false");
    Serial.println("Stop command processed and reset.");
  }
}

//doam dat
int readSoilMoisture() {
  int soilMoisture = analogRead(SOIL_MOISTURE_PIN_1);
  int soilMoisturePercent = map(soilMoisture, 0, 4095, 100, 0);
  lcd.setCursor(0, 0);
  lcd.print("Soil moisture 1: ");
  lcd.print(soilMoisturePercent);
  lcd.print("%");
  return soilMoisturePercent;
}

//doam dat2
int readSoilMoisture2() {
  int soilMoisture2 = analogRead(SOIL_MOISTURE_PIN_2);
  int soilMoisturePercent2 = map(soilMoisture2, 0, 4095, 100, 0);
  lcd.setCursor(0, 1);
  lcd.print("Soil moisture 2: ");
  lcd.print(soilMoisturePercent2);
  lcd.print("%");
  return soilMoisturePercent2;
}

//ndo doam
void readTemperatureAndHumidity() {
  float h, t;
  int retryCount = 3;  // Số lần thử đọc lại dữ liệu

  while (retryCount-- > 0) {
    h = dht.readHumidity();
    t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      break;  // Nếu đọc thành công, thoát khỏi vòng lặp
    }
    delay(2000);  // Chờ trước khi đọc lại
  }

  lcd.setCursor(0, 2);
  lcd.print("Humidity: ");
  lcd.print(h);
  lcd.print("%");

  lcd.setCursor(0, 3);
  lcd.print("Temp: ");
  lcd.print(t);
  lcd.print(" *C");
}

//firebase
void sendDataToFirebase() {

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int soilMoisturePercent = readSoilMoisture();
  int soilMoisturePercent2 = readSoilMoisture2();

  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    timestamp = getRealTime();
    timeClient.update();
    int currentHour = timeClient.getHours();
    int currentMinute = timeClient.getMinutes();
    int currentSecond = timeClient.getSeconds();
    String logPath = "/logs/" + String(counter);  // Tạo đường dẫn duy nhất cho mỗi log
    currentMillis = lastSwitchTime1 + millis();
    Serial.println(currentMillis);

    if ((currentMinute == 0) && (currentHour == 6 || currentHour == 14 || currentHour == 22)) {
      // Kiểm tra nếu chưa gửi log trong phút này
      if (currentMillis - lastLogTime > 60000) {  // Hơn 60 giây kể từ lần gửi cuối
        lastLogTime = currentMillis;              // Cập nhật thời gian gửi log

        // Gửi dữ liệu log
        json.set("timestamp", String(timestamp));
        json.set("temperature", temperature);
        json.set("humidity", humidity);
        json.set("soilMoisture", soilMoisturePercent);
        json.set("soilMoisture2", soilMoisturePercent2);

        String logPath = "/logs/" + String(counter);
        if (Firebase.RTDB.setJSON(&firebaseData, logPath.c_str(), &json)) {
          Serial.println("Log data sent successfully at path: " + logPath);
        } else {
          Serial.println("Failed to send log data");
          Serial.println(firebaseData.errorReason());
        }

        counter++;
        if (counter > 21) {
          counter = 0;
        }
      }
    }
    // Kiểm tra lệnh xóa từ Firebase
    String deleteCommand = retrieveData("/devices/delete");
    if (deleteCommand == "true") {
      // Xóa đường dẫn logs
      if (Firebase.RTDB.deleteNode(&firebaseData, "/logs")) {
        Serial.println("Xóa logs thành công");
        // Đặt lại counter về 0
        counter = 0;
        // Đặt lại chu trình LED về trạng thái ban đầu
        inOffCycle = true;
        ledOn = false;
        lastSwitchTime = millis();  // Cập nhật thời gian bắt đầu chu kỳ bật
        lastSwitchTime1 = 0;        // Cập nhật thời gian bắt đầu chu kỳ bật
        // Xóa lệnh xóa để tránh xóa lặp lại
        Firebase.RTDB.setBool(&firebaseData, "/devices/delete", false);
      } else {
        Serial.println("Xóa logs thất bại");
        Serial.println(firebaseData.errorReason());
      }
    }
    json.set(temperaturePath.c_str(), String(temperature));
    json.set(airHumidityPath.c_str(), String(humidity));
    json.set(soilMoisturePath.c_str(), String(soilMoisturePercent));
    json.set(soilMoisture2Path.c_str(), String(soilMoisturePercent2));
    json.set(lastSwitchTimePath.c_str(), currentMillis);
    if (Firebase.RTDB.setJSON(&firebaseData, databasePath.c_str(), &json)) {
      Serial.println("Data sent successfully");
    } else {
      Serial.println("Failed to send data");
      Serial.println(firebaseData.errorReason());
    }
  }
}
