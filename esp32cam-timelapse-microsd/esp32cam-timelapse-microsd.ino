
#include <SD_MMC.h>
#include "esp_camera.h"

#define MINISECONDS_BETWEEN_PHOTOS 1

// Pins for ESP32-CAM
#define FLASH_PIN 4

// Camera pins
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

bool startMicroSD() {
  Serial.print("Starting microSD... ");
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  if (SD_MMC.begin("/sdcard", true)) {
    Serial.println("OKAY");
    return true;
  } else {
    Serial.println("FAILED");
    return false;
  }
}

bool startCamera() {
  // Turn off the flash
  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(FLASH_PIN, HIGH);
  // Initialize the camera hardware
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Set resolution based on whether we have extra memory
  if (psramFound()) {
    Serial.println("PSRAM found. Maximum XGA resolution supported.");
    config.frame_size = FRAMESIZE_XGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    Serial.println("PSRAM not found. Maximum SVGA resolution supported.");
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Start the camera
  Serial.print("Starting camera... ");
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.println("FAILED");
    return false;
  } else {
    Serial.println("OKAY");
    return true;
  }
}
void delayBetweenPhoto() {
  delay(MINISECONDS_BETWEEN_PHOTOS * 60 * 3000);
}

void takePhoto(String filename) {
  // Take a photo and get the data
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Unable to take a photo");
    return;
  }

  // Make sure it is a JPEG
  if (fb->format != PIXFORMAT_JPEG) {
    Serial.println("Capture format not JPEG");
    esp_camera_fb_return(fb);  // Return the photo data
    return;
  }

  // Save the picture to the SD card
  File file = SD_MMC.open(filename.c_str(), "w");
  if (file) {
    Serial.println("Saving " + filename);
    file.write(fb->buf, fb->len);
    file.close();
  } else {
    Serial.println("Unable to write " + filename);
  }

  // Return the picture data
  esp_camera_fb_return(fb);
}

void setup() {
  // Initialize the peripherals
  Serial.begin(115200);
  while (!Serial) delay(100);
  startMicroSD();
  startCamera();
  delay(5000);  // Delay 5 seconds before first photo
}

void loop() {
  // Keep a count of the number of photos we have taken
  static int number = 0;
  number++;

  // Construct a filename that looks like "/photo_0001.jpg"
  String filename = "/d1_g4_photo_";
  if (number < 1000) filename += "0";
  if (number < 100) filename += "0";
  if (number < 10) filename += "0";
  filename += number;
  filename += ".jpg";

  takePhoto(filename);
  // Delay until the next photo
  delayBetweenPhoto();
}
