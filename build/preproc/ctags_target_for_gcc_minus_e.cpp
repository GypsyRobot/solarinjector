# 1 "/home/everton/sunfactory/sunfactory.ino"
// Turns the 'PRG' button into the power button, long press is off


# 5 "/home/everton/sunfactory/sunfactory.ino" 2
# 6 "/home/everton/sunfactory/sunfactory.ino" 2
# 7 "/home/everton/sunfactory/sunfactory.ino" 2
# 8 "/home/everton/sunfactory/sunfactory.ino" 2

// #include <WiFiManager.h> // https://github.com/tzapu/WiFiManager




// For wifi connection



const char *filename = "/data.csv"; // File path in SPIFFS

WebServer server(80); // Create a web server on port 80

// NTP server settings
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0; // Adjust this according to your timezone
const int daylightOffset_sec = 0; // Adjust this according to your day light saving time

// Constants for the Thermistors
# 37 "/home/everton/sunfactory/sunfactory.ino"
// Constants from the LDR datasheet






float targetTemperatures[] = {210, 240, 180, 15};
int targetIndex = 0;
float targetTemperature = targetTemperatures[targetIndex];

bool connected = false;

void setup()
{
  heltec_setup();

  display.resetOrientation();

  pinMode(4 /* Buzzer*/, 0x03); // Buzzer

  Serial0.println("Serial works");

  if (!SPIFFS.begin(true))
  {
    return;
  }
  delay(1000);

  if (SPIFFS.exists(filename))
  {
    Serial0.println("File exists");
    // File file = SPIFFS.open(filename, FILE_APPEND);
  }
  else
  {
    Serial0.println("File does not exist, creating new file with headers");
    File file = SPIFFS.open(filename, "w");
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
  if (state == (0))
  {
    display.println("works");
  }
  else
  {
    display.printf("fail, code: %i\n", state);
  }

  _radiolib_status = radio.setFrequency(866.3); Serial0.print("[RadioLib] "); Serial0.print("radio.setFrequency(866.3)"); Serial0.print(" returned "); Serial0.print(_radiolib_status); Serial0.print(" ("); Serial0.print(radiolib_result_string(_radiolib_status)); Serial0.println(")");; if (_radiolib_status != (0)) { Serial0.println("[RadioLib] Halted"); while (true) { heltec_delay(10); } };
  heltec_delay(3000);
  yield();
  // Battery
  float vbat = heltec_vbat();
  display.printf("Vbat: %.2fV (%d%%)\n", vbat, heltec_battery_percent(vbat));

  yield();
  char buffer[64]; // Buffer to store formatted string
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawStringf(4, 4, buffer, "ssid: ");
  display.drawStringf(50, 4, buffer, "sunfactory");
  display.drawStringf(4, 24, buffer, "pass: ");
  display.drawStringf(50, 24, buffer, "sunfactory");
  display.display();
  heltec_delay(3000);

  connected = handleWifi();

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
  float ldrValue = analogRead(5 /* Analog pin where the voltage divider is connected for LDR*/);

  // Convert to voltage
  float ldrVoltage = (ldrValue / 4095.0 /* 12-bit ADC on ESP8266*/) * 3.3 /* ESP8266 supply voltage*/;

  // Avoid division by zero
  if (ldrVoltage == 0)
  {
    // Serial.println("LDR voltage is 0V, possibly no light detected.");
    return;
  }

  // Calculate LDR resistance
  float ldrResistance = (3.3 /* ESP8266 supply voltage*/ / ldrVoltage - 1) * 10000.0 /* 10k resistor value (in ohms)*/;

  // Convert Resistance to Lux using the gamma formula
  float lux = 10.0 * pow((8000.0 /* Resistance at 10 Lux (8kΩ - lower bound)*/ / ldrResistance), (1.0 / 0.7 /* Gamma value from datasheet*/));
  yield();
  // Read and calculate temperature for Thermistor A
  int adcValueA = analogRead(7 /* Analog pin where the voltage divider is connected for Thermistor A*/);
  float voltageA = (adcValueA / 4095.0 /* 12-bit ADC on ESP8266*/) * 3.3 /* ESP8266 supply voltage*/;
  float resistanceA = 10000.0 /* 10k resistor value (in ohms)*/ * ((3.3 /* ESP8266 supply voltage*/ / voltageA) - 1);
  float thermistorTemperatureA = resistanceA / 100000.0 /* 100k thermistor at 25C*/; // (R/Ro)
  thermistorTemperatureA = log(thermistorTemperatureA); // ln(R/Ro)
  thermistorTemperatureA /= 3950.0 /* Beta coefficient of thermistor*/; // 1/B * ln(R/Ro)
  thermistorTemperatureA += 1.0 / (25.0 /* 25°C*/ + 273.15); // + (1/To)
  thermistorTemperatureA = 1.0 / thermistorTemperatureA; // Invert
  thermistorTemperatureA -= 273.15;

  // Read and calculate temperature for Thermistor B
  int adcValueB = analogRead(6 /* Analog pin where the voltage divider is connected for Thermistor B*/);
  float voltageB = (adcValueB / 4095.0 /* 12-bit ADC on ESP8266*/) * 3.3 /* ESP8266 supply voltage*/;
  float resistanceB = 10000.0 /* 10k resistor value (in ohms)*/ * ((3.3 /* ESP8266 supply voltage*/ / voltageB) - 1);
  float thermistorTemperatureB = resistanceB / 100000.0 /* 100k thermistor at 25C*/; // (R/Ro)
  thermistorTemperatureB = log(thermistorTemperatureB); // ln(R/Ro)
  thermistorTemperatureB /= 3950.0 /* Beta coefficient of thermistor*/; // 1/B * ln(R/Ro)
  thermistorTemperatureB += 1.0 / (25.0 /* 25°C*/ + 273.15); // + (1/To)
  thermistorTemperatureB = 1.0 / thermistorTemperatureB; // Invert
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

    digitalWrite(4 /* Buzzer*/, 0x1); // Turn on the buzzer
    heltec_delay(200);
    digitalWrite(4 /* Buzzer*/, 0x0); // Turn off the buzzer
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

  if (currentTime - lastSaveTime >= 60000 /* 60000 milliseconds = 1 minute*/) // 60000 milliseconds = 1 minute
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
        Serial0.println("Failed to obtain time");
      }
      else
      {
        time(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
      }
    }

    Serial0.println(timestamp);

    yield();
    File file = SPIFFS.open(filename, "a");
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
      Serial0.print("Error writing to file: ");
      Serial0.println(e.what());
    }

    file.close();
    yield();
  }
  yield();

  server.handleClient(); // Handle client requests
  yield();

  heltec_delay(1000);
}

bool handleWifi()
{
  // Connect to WiFi
  WiFi.begin("sunfactory", "sunfactory");

  char buffer[64]; // Buffer to store formatted string
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "WiFi Connecting...");
  display.display();

  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED)
  {
    if (millis() - startAttemptTime >= 10000 /* 10000 milliseconds = 10 seconds*/)
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
  Serial0.println("Connected to WiFi!");
  Serial0.println(WiFi.localIP());
  delay(5000);

  // Start the server
  server.on("/", HTTP_GET, handleRoot); // Handle root request
  server.on("/download", HTTP_GET, handleDownload); // Handle download request
  yield();
  server.begin();

  // Initialize NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  return true;
}

// Function to append data to the CSV file
void addData()
{
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
  File file = SPIFFS.open(filename, "r");
  if (!file)
  {
    server.send(500, "text/plain", "Failed to open file for reading");
    return;
  }

  // Send the file content as a downloadable file
  server.streamFile(file, "text/csv");
  file.close();
}
