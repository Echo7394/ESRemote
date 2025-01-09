#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Adafruit_GFX.h"
#include <WiFi.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <base64.h>
#include <CST816D.h>

#define I2C_SDA 4
#define I2C_SCL 5
#define TP_INT 0
#define TP_RST 1
#define off_pin 35

TFT_eSPI tft = TFT_eSPI(); 
CST816D touch(I2C_SDA, I2C_SCL, TP_RST, TP_INT);

unsigned long activityTime = 0;
String lastTemperature = ""; 
String auth = ":199312";
String encodedAuth = base64::encode(auth);
String plus = "+";
String minus = "-";
bool initialFetchDone = false;

void drawBorder() {
  int outerRadius = 120;
  int borderThickness = 10;
  int innerRadius = outerRadius - borderThickness;
  int startR = 0x10;
  int startG = 0xFF;
  int startB = 0xE0;
  for (int radius = outerRadius; radius > innerRadius; radius--) {
    float fadeFactor = float(outerRadius - radius) / borderThickness;
    uint16_t fadedColor = tft.color565(
      int(startR * (1 - fadeFactor)), 
      int(startG * (1 - fadeFactor)), 
      int(startB * (1 - fadeFactor))
    );
    tft.drawCircle(120, 120, radius, fadedColor);
  }
}

void displayError(String message) {
  tft.fillScreen(TFT_BLACK);
  drawBorder();
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(30, 100);
  tft.print(message);
  Serial.println("Error: " + message);
}

void handleHTTPError(int httpCode) {
    Serial.println("Error on HTTP request");
    tft.fillScreen(TFT_BLACK);
    drawBorder();
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(3);
    tft.setCursor(50, 90);
    tft.print(httpCode);
}

void updateDisplay(String temperature) {
  tft.fillScreen(TFT_BLACK);
  drawBorder();
  tft.setTextSize(4);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(110, 20);
  tft.print(plus);
  tft.setCursor(110, 200);
  tft.print(minus);
  tft.setTextSize(3);
  tft.setCursor(41, 90);
  tft.print("Temp Set:\n\n     " + temperature + " F");
}

void fetchAndDisplayTemperature() {
  HTTPClient http;
  int maxRetries = 5;
  int retries = 0;

  while (retries < maxRetries) {
    http.begin("http://192.168.0.23");
    http.addHeader("Authorization", "Basic " + encodedAuth);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      String identifier = "id='tempSet'>";
      int startIndex = payload.indexOf(identifier);

      if (startIndex != -1) {
        startIndex += identifier.length();
        int endIndex = payload.indexOf("</span>", startIndex);

        if (endIndex != -1) {
          String temperature = payload.substring(startIndex, endIndex);

          if (!initialFetchDone) {
            // Run this block only on the initial fetch
            updateDisplay(temperature);
            lastTemperature = temperature;
            initialFetchDone = true; // Set the flag to true
          }

          if (temperature != lastTemperature) {
            updateDisplay(temperature);
            lastTemperature = temperature;
          }
          http.end(); // Close the HTTP connection
          return; // Successful response, exit the loop
        } else {
          displayError("End tag not found");
        }
      } else {
        displayError("Temperature data not found");
      }
    } else {
      displayError("HTTP GET failed, retrying...");
      delay(2000); // Wait for a while before retrying
    }

    http.end(); // Close the HTTP connection
    retries++;
  }

  // If it fails more than 5 times, restart the ESP
  displayError("HTTP GET failed more than 5 times, restarting ESP...");
  ESP.restart();
}

void checkForInactivity() {
  if (millis() - activityTime > 10000) {
    analogWrite(TFT_BL, 5);
  }
}

void increaseTemp() {
    HTTPClient http;
    http.begin("http://192.168.0.23/increase");
    http.addHeader("Authorization", "Basic " + encodedAuth);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        fetchAndDisplayTemperature();
    } else {
        handleHTTPError(httpCode);
    }
    http.end();
}

void decreaseTemp() {
    HTTPClient http;
    http.begin("http://192.168.0.23/decrease");
    http.addHeader("Authorization", "Basic " + encodedAuth);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        fetchAndDisplayTemperature();
    } else {
        handleHTTPError(httpCode);
    }
    http.end();
}

void handleTouch(uint16_t touchX, uint16_t touchY) {
  uint16_t touchColorInactive = tft.color565(0x10, 0xE60, 0xE0);
  uint16_t touchColorActive = tft.color565(0xFF, 0xFF, 0xFF);

  // Calculate the touch zones for increaseTemp and decreaseTemp
  int screenHeight = tft.height(); // Get the screen height (240)
  int touchZoneHeight = screenHeight / 4; // Divide the screen into 4 equal parts
  int upperTouchZoneY = 0; // Upper 1/4 of the screen
  int lowerTouchZoneY = 3 * touchZoneHeight; // Lower 1/4 of the screen

  if (touchY >= upperTouchZoneY && touchY <= (upperTouchZoneY + touchZoneHeight)) {
    // Touch zone for increaseTemp (upper 1/4 of the screen)
    tft.fillRoundRect(90, upperTouchZoneY, 60, touchZoneHeight, 5, touchColorActive);
    tft.setTextColor(TFT_BLACK); // Set text color
    tft.setTextSize(3);
    tft.setCursor(110, 20);
    tft.print(plus);
    increaseTemp();
  } else if (touchY >= lowerTouchZoneY && touchY <= (lowerTouchZoneY + touchZoneHeight)) {
    // Touch zone for decreaseTemp (lower 1/4 of the screen)
    tft.fillRoundRect(90, lowerTouchZoneY, 60, touchZoneHeight, 5, touchColorActive);
    tft.setTextColor(TFT_BLACK); // Set text color
    tft.setTextSize(3);
    tft.setCursor(110, 200);
    tft.print(minus);
    decreaseTemp();
  } else {
    // No touch on the zones, set them to inactive
    tft.fillRoundRect(90, upperTouchZoneY, 60, touchZoneHeight, 5, touchColorInactive);
    tft.fillRoundRect(90, lowerTouchZoneY, 60, touchZoneHeight, 5, touchColorInactive);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(110, 20);
    tft.print(plus);
    tft.setCursor(110, 200);
    tft.print(minus);
  }
}

void touch_read() {
  bool touched;
  uint8_t gesture;
  uint16_t touchX, touchY;
  touched = touch.getTouch(&touchX, &touchY, &gesture);
  if (touched) {
    fetchAndDisplayTemperature();
    activityTime = millis();
    analogWrite(TFT_BL, 255);
    handleTouch(touchX, touchY);
  }
}

void setup() {
  Wire.begin(I2C_SDA, I2C_SCL);
  analogWrite(TFT_BL, 255);
  WiFi.begin("Spires", "Spires54646");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  tft.init();
  tft.setRotation(0);
}

void loop() {
  if (!initialFetchDone) {
    fetchAndDisplayTemperature();
  }

  checkForInactivity();
  touch_read();
  Serial.println("looping");
}