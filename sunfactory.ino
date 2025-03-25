// Turns the 'PRG' button into the power button, long press is off
#define HELTEC_POWER_BUTTON // must be before "#include <heltec_unofficial.h>"

#include <heltec_unofficial.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>
// #include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#define LOG_INTERVAL_MS 60000 // 60000 milliseconds = 1 minute
#define WIFI_TIMEOUT_MS 10000 // 10000 milliseconds = 10 seconds

#define WIFI_SSID "sunfactory"
#define WIFI_PASS "sunfactory"

const char *filename = "/data.csv"; // File path in SPIFFS

WebServer server(80); // Create a web server on port 80

// NTP server settings
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;     // Adjust this according to your timezone
const int daylightOffset_sec = 0; // Adjust this according to your day light saving time

// Constants for the Thermistors
#define THERMISTOR_PIN_A 7          // Analog pin where the voltage divider is connected for Thermistor A
#define THERMISTOR_PIN_B 6          // Analog pin where the voltage divider is connected for Thermistor B
#define SERIES_RESISTOR 10000.0     // 10k resistor value (in ohms)
#define NOMINAL_RESISTANCE 100000.0 // 100k thermistor at 25C
#define NOMINAL_TEMPERATURE 25.0    // 25°C
#define B_COEFFICIENT 3950.0        // Beta coefficient of thermistor
#define SUPPLY_VOLTAGE 3.3          // ESP8266 supply voltage
#define ADC_RESOLUTION 4095.0       // 12-bit ADC on ESP8266

// Constants from the LDR datasheet
#define LDR_PIN 5      // Analog pin where the voltage divider is connected for LDR
#define R10_LUX 8000.0 // Resistance at 10 Lux (8kΩ - lower bound)
#define GAMMA 0.7      // Gamma value from datasheet

#define BUZZER_PIN 4 // Buzzer

float targetTemperatures[] = {180, 200, 210, 220, 250, 25, 70, 90, 100};
int targetIndex = 0;
float targetTemperature = targetTemperatures[targetIndex];

bool connected = false;

void setup()
{
  heltec_setup();

  display.resetOrientation();

  pinMode(BUZZER_PIN, OUTPUT); // Buzzer

  Serial.println("Serial works");

  if (!SPIFFS.begin(true))
  {
    return;
  }
  delay(1000);

  if (SPIFFS.exists(filename))
  {
    Serial.println("File exists");
    // File file = SPIFFS.open(filename, FILE_APPEND);
  }
  else
  {
    Serial.println("File does not exist, creating new file with headers");
    File file = SPIFFS.open(filename, FILE_WRITE);
    delay(1000);
    if (file)
    {
      file.println("Timestamp, Temperature A, Temperature B, Lux"); // CSV headers
      file.close();
    }
  }
  delay(1000);

  // Display
  display.println("Display works");

  // Radio
  display.print("Radio ");
  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE)
  {
    display.println("works");
  }
  else
  {
    display.printf("fail, code: %i\n", state);
  }

  RADIOLIB_OR_HALT(radio.setFrequency(866.3));
  heltec_delay(3000);
  yield();
  // Battery
  float vbat = heltec_vbat();
  display.printf("Vbat: %.2fV (%d%%)\n", vbat, heltec_battery_percent(vbat));

  yield();
  char buffer[64]; // Buffer to store formatted string
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawStringf(4, 4, buffer, "");
  display.drawStringf(50, 4, buffer, WIFI_SSID);
  display.drawStringf(4, 24, buffer, "");
  display.drawStringf(50, 24, buffer, WIFI_PASS);
  display.display();
  heltec_delay(5000);

  connected = handleWifi();

  delay(1000); // Allow time for serial monitor to open
}

void loop()
{
  yield();
  heltec_loop();
  yield();

  // Button
  targetTemperature = targetTemperatures[targetIndex];
  yield();

  if (button.isSingleClick())
  {
    targetIndex = (targetIndex + 1) % (sizeof(targetTemperatures) / sizeof(targetTemperatures[0])); // Cycle through targetTemperatures array

    // LED
    for (int n = 0; n <= 10; n++)
    {
      heltec_led(n);
      delay(5);
    }
    for (int n = 10; n >= 0; n--)
    {
      heltec_led(n);
      delay(5);
    }
    // display.println("LED works");
  }

  yield();
  float ldrValue = analogRead(LDR_PIN);

  // Convert to voltage
  float ldrVoltage = (ldrValue / ADC_RESOLUTION) * SUPPLY_VOLTAGE;

  // Avoid division by zero
  if (ldrVoltage == 0)
  {
    // Serial.println("LDR voltage is 0V, possibly no light detected.");
    return;
  }

  // Calculate LDR resistance
  float ldrResistance = (SUPPLY_VOLTAGE / ldrVoltage - 1) * SERIES_RESISTOR;

  // Convert Resistance to Lux using the gamma formula
  float lux = 10.0 * pow((R10_LUX / ldrResistance), (1.0 / GAMMA));
  yield();
  // Read and calculate temperature for Thermistor A
  int adcValueA = analogRead(THERMISTOR_PIN_A);
  float voltageA = (adcValueA / ADC_RESOLUTION) * SUPPLY_VOLTAGE;
  float resistanceA = SERIES_RESISTOR * ((SUPPLY_VOLTAGE / voltageA) - 1);
  float thermistorTemperatureA = resistanceA / NOMINAL_RESISTANCE; // (R/Ro)
  thermistorTemperatureA = log(thermistorTemperatureA);            // ln(R/Ro)
  thermistorTemperatureA /= B_COEFFICIENT;                         // 1/B * ln(R/Ro)
  thermistorTemperatureA += 1.0 / (NOMINAL_TEMPERATURE + 273.15);  // + (1/To)
  thermistorTemperatureA = 1.0 / thermistorTemperatureA;           // Invert
  thermistorTemperatureA -= 273.15;

  // Read and calculate temperature for Thermistor B
  int adcValueB = analogRead(THERMISTOR_PIN_B);
  float voltageB = (adcValueB / ADC_RESOLUTION) * SUPPLY_VOLTAGE;
  float resistanceB = SERIES_RESISTOR * ((SUPPLY_VOLTAGE / voltageB) - 1);
  float thermistorTemperatureB = resistanceB / NOMINAL_RESISTANCE; // (R/Ro)
  thermistorTemperatureB = log(thermistorTemperatureB);            // ln(R/Ro)
  thermistorTemperatureB /= B_COEFFICIENT;                         // 1/B * ln(R/Ro)
  thermistorTemperatureB += 1.0 / (NOMINAL_TEMPERATURE + 273.15);  // + (1/To)
  thermistorTemperatureB = 1.0 / thermistorTemperatureB;           // Invert
  thermistorTemperatureB -= 273.15;

  display.clear();
  char buffer[64]; // Buffer to store formatted string

  // print layout structures
  display.drawHorizontalLine(0, 32, 128);
  display.drawVerticalLine(84, 0, 64);
  display.drawRect(0, 0, 128, 64);

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawStringf(87, 7, buffer, "%.0fC", targetTemperature);

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  if (thermistorTemperatureA < -100 || thermistorTemperatureA > 400)
  {
    display.drawStringf(2, 4, buffer, "A:-------");
  }
  else
  {
    display.drawStringf(2, 4, buffer, "A:%.0fC", thermistorTemperatureA);
  }

  yield();
  // print temperature readings for sensor B
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  if (thermistorTemperatureB < -100 || thermistorTemperatureB > 400)
  {
    display.drawStringf(2, 34, buffer, "B:-------");
  }
  else
  {
    display.drawStringf(2, 34, buffer, "B:%.0fC", thermistorTemperatureB);
  }

  // print OK if temperature above target temperature
  if (thermistorTemperatureA >= targetTemperature || thermistorTemperatureB >= targetTemperature)
  {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_24);
    display.drawStringf(106, 34, buffer, "OK");
    heltec_led(100);

    digitalWrite(BUZZER_PIN, HIGH); // Turn on the buzzer
    heltec_delay(200);
    digitalWrite(BUZZER_PIN, LOW); // Turn off the buzzer
    heltec_delay(200);
  }
  else
  {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawStringf(106, 32, buffer, "%.0f", lux);
    display.drawStringf(106, 46, buffer, "lm");
    heltec_led(0);
  }

  yield();
  display.display();

  static unsigned long lastSaveTime = 0;
  unsigned long currentTime = millis();
  yield();

  if (currentTime - lastSaveTime >= LOG_INTERVAL_MS) // 60000 milliseconds = 1 minute
  {
    lastSaveTime = currentTime;
    yield();

    // Get current time from WiFi network
    char timestamp[20];
    // Generate a simple timestamp and dummy value
    snprintf(timestamp, sizeof(timestamp), "%lu", millis() / 1000);

    if (connected)
    {
      time_t now;
      struct tm timeinfo;
      if (!getLocalTime(&timeinfo))
      {
        Serial.println("Failed to obtain time");
      }
      else
      {
        time(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
      }
    }

    Serial.println(timestamp);

    yield();
    File file = SPIFFS.open(filename, FILE_APPEND);
    yield();

    if (!file)
    {
      return;
    }

    try
    {
      file.printf("%s, %d, %d, %d,\n", timestamp, (int)thermistorTemperatureA, (int)thermistorTemperatureB, (int)lux);
    }
    catch (const std::exception &e)
    {
      Serial.print("Error writing to file: ");
      Serial.println(e.what());
    }

    file.close();
    yield();
  }
  yield();

  server.handleClient(); // Handle client requests
  yield();

  heltec_delay(100);
}

bool handleWifi()
{

  // WiFiManager wm;
  // wm.setTimeout(30); // 30 secon
  // wm.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  // // heltec_delay(3000);

  // // Try connecting to saved WiFi, or start AP for setup
  // // if (!wm.autoConnect(WIFI_SSID, WIFI_PASS))
  // if (!wm.autoConnect("AVALON", "H1dr0f0b14"))
  // {
  //   ESP.restart();
  // }

  // Serial.println("Connected to WiFi!");
  // Serial.println(WiFi.localIP());
  // display.drawStringf(4, 44, buffer, "IP: ");
  // display.drawStringf(50, 44, buffer, WiFi.localIP().toString().c_str());

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  char buffer[64]; // Buffer to store formatted string
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "WiFi Connecting...");
  display.display();

  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED)
  {
    if (millis() - startAttemptTime >= WIFI_TIMEOUT_MS)
    {
      display.clear();
      display.drawString(0, 0, "WiFi Failed");
      display.display();

      return false;
    }
    delay(500);
    display.drawString(0, 16, ".");
    display.display();
  }

  display.clear();
  display.drawString(0, 0, "WiFi Works");
  display.drawStringf(0, 16, buffer, "IP: %s", WiFi.localIP().toString().c_str());
  display.display();
  Serial.println("Connected to WiFi!");
  Serial.println(WiFi.localIP());
  delay(5000);

  // Start the server
  server.on("/", HTTP_GET, handleRoot);             // Handle root request
  server.on("/download", HTTP_GET, handleDownload); // Handle download request
  yield();
  server.begin();

  // Initialize NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  return true;
}

// Handle the root URL
void handleRoot()
{
  String html = "<html><body><h1>Sunfactory Temperature Log:</h1>";
  html += "<p><a href='/download'>Download CSV</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Handle the download URL
void handleDownload()
{
  File file = SPIFFS.open(filename, FILE_READ);
  if (!file)
  {
    server.send(500, "text/plain", "Failed to open file for reading");
    return;
  }

  // Send the file content as a downloadable file
  server.streamFile(file, "text/csv");
  file.close();
}
