# 1 "/home/everton/sunfactory/sunfactory.ino"
// Turns the 'PRG' button into the power button, long press is off


# 5 "/home/everton/sunfactory/sunfactory.ino" 2
# 6 "/home/everton/sunfactory/sunfactory.ino" 2
# 7 "/home/everton/sunfactory/sunfactory.ino" 2
# 8 "/home/everton/sunfactory/sunfactory.ino" 2

// #include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

// ###################### LORA ######################
// Pause between transmited packets in seconds.
// Set to zero to only transmit a packet when pressing the user button
// Will not exceed 1% duty cycle, even if you set a lower value.


// Frequency in MHz. Keep the decimal point to designate float.
// Check your own rules and regulations to see what is legal where you are.

// #define FREQUENCY           905.2       // for US

// LoRa bandwidth. Keep the decimal point to designate float.
// Allowed values are 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0, 250.0 and 500.0 kHz.


// Number from 5 to 12. Higher means slower but higher "processor gain",
// meaning (in nutshell) longer range and more robust against interference.


// Transmit power in dBm. 0 dBm = 1 mW, enough for tabletop-testing. This value can be
// set anywhere between -9 dBm (0.125 mW) to 22 dBm (158 mW). Note that the maximum ERP
// (which is what your antenna maximally radiates) on the EU ISM band is 25 mW, and that
// transmissting without an antenna can damage your hardware.


String txdata;
String rxdata;
volatile bool rxFlag = false;
long counter = 0;
uint64_t last_tx = 0;
uint64_t tx_time;
uint64_t minimum_pause;

// ###################################################



// For wifi connection




// #define WIFI_SSID "sunfactory"
// #define WIFI_PASS "sunfactory"

const char *filename = "/data.csv"; // File path in SPIFFS

WebServer server(80); // Create a web server on port 80

// NTP server settings
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0; // Adjust this according to your timezone
const int daylightOffset_sec = 0; // Adjust this according to your day light saving time

// Constants for the Thermistors
# 75 "/home/everton/sunfactory/sunfactory.ino"
// Constants from the LDR datasheet






float targetTemperatures[] = {180, 200, 210, 220, 250, 25, 70, 90, 100};
int targetIndex = 0;
float targetTemperature = targetTemperatures[targetIndex];

bool connected = false;

float thermistorTemperatureA = 0;
float thermistorTemperatureB = 0;
float lux = 0;
float fileSize = 0;
int availableMemory = 99999; // to make sure it's not 0
float availablePercentage = 100; // start with 100 to make sure
char timestamp[20];

bool enableBeep = true;

unsigned long lastSaveTime = 0;

void setup()
{
  heltec_setup();

  // ###################### LORA ######################
  both.println("Radio init");
  _radiolib_status = radio.begin(); Serial0.print("[RadioLib] "); Serial0.print("radio.begin()"); Serial0.print(" returned "); Serial0.print(_radiolib_status); Serial0.print(" ("); Serial0.print(radiolib_result_string(_radiolib_status)); Serial0.println(")");; if (_radiolib_status != (0)) { Serial0.println("[RadioLib] Halted"); while (true) { heltec_delay(10); } };
  // Set the callback function for received packets
  radio.setDio1Action(rx);
  // Set radio parameters
  both.printf("Frequency: %.2f MHz\n", 866.3 /* for Europe*/);
  _radiolib_status = radio.setFrequency(866.3 /* for Europe*/); Serial0.print("[RadioLib] "); Serial0.print("radio.setFrequency(866.3 /* for Europe*/)"); Serial0.print(" returned "); Serial0.print(_radiolib_status); Serial0.print(" ("); Serial0.print(radiolib_result_string(_radiolib_status)); Serial0.println(")");; if (_radiolib_status != (0)) { Serial0.println("[RadioLib] Halted"); while (true) { heltec_delay(10); } };
  both.printf("Bandwidth: %.1f kHz\n", 250.0);
  _radiolib_status = radio.setBandwidth(250.0); Serial0.print("[RadioLib] "); Serial0.print("radio.setBandwidth(250.0)"); Serial0.print(" returned "); Serial0.print(_radiolib_status); Serial0.print(" ("); Serial0.print(radiolib_result_string(_radiolib_status)); Serial0.println(")");; if (_radiolib_status != (0)) { Serial0.println("[RadioLib] Halted"); while (true) { heltec_delay(10); } };
  both.printf("Spreading Factor: %i\n", 9);
  _radiolib_status = radio.setSpreadingFactor(9); Serial0.print("[RadioLib] "); Serial0.print("radio.setSpreadingFactor(9)"); Serial0.print(" returned "); Serial0.print(_radiolib_status); Serial0.print(" ("); Serial0.print(radiolib_result_string(_radiolib_status)); Serial0.println(")");; if (_radiolib_status != (0)) { Serial0.println("[RadioLib] Halted"); while (true) { heltec_delay(10); } };
  both.printf("TX power: %i dBm\n", 14);
  _radiolib_status = radio.setOutputPower(14); Serial0.print("[RadioLib] "); Serial0.print("radio.setOutputPower(14)"); Serial0.print(" returned "); Serial0.print(_radiolib_status); Serial0.print(" ("); Serial0.print(radiolib_result_string(_radiolib_status)); Serial0.println(")");; if (_radiolib_status != (0)) { Serial0.println("[RadioLib] Halted"); while (true) { heltec_delay(10); } };
  // Start receiving
  _radiolib_status = radio.startReceive(0xFFFFFF /*  23    0                        infinite (Rx continuous mode)*/); Serial0.print("[RadioLib] "); Serial0.print("radio.startReceive(0xFFFFFF /*  23    0                        infinite (Rx continuous mode)*/)"); Serial0.print(" returned "); Serial0.print(_radiolib_status); Serial0.print(" ("); Serial0.print(radiolib_result_string(_radiolib_status)); Serial0.println(")");; if (_radiolib_status != (0)) { Serial0.println("[RadioLib] Halted"); while (true) { heltec_delay(10); } };

  // ###################################################

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

  // Battery
  float vbat = heltec_vbat();
  display.printf("Vbat: %.2fV (%d%%)\n", vbat, heltec_battery_percent(vbat));

  yield();
  char buffer[64]; // Buffer to store formatted string
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawStringf(4, 4, buffer, "");
  display.drawStringf(4, 4, buffer, "TP-Link_6178");
  display.drawStringf(4, 24, buffer, "");
  display.drawStringf(4, 24, buffer, "39511836");
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

  if (!SPIFFS.exists(filename))
  {
    Serial0.println("File does not exist, creating new file with headers");
    display.println("log wiped!");
    display.println("creating file");
    File file = SPIFFS.open(filename, "w");
    delay(1000);
    if (file)
    {
      file.println("Timestamp, Temperature A, Temperature B, Lux"); // CSV headers
      file.close();
    }
    delay(1000);
  }
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
  lux = pow((8000.0 /* Resistance at 10 Lux (8kΩ - lower bound)*/ / ldrResistance), (1.0 / 0.7 /* Gamma value from datasheet*/));
  yield();
  // Read and calculate temperature for Thermistor A
  int adcValueA = analogRead(7 /* Analog pin where the voltage divider is connected for Thermistor A*/);
  float voltageA = (adcValueA / 4095.0 /* 12-bit ADC on ESP8266*/) * 3.3 /* ESP8266 supply voltage*/;
  float resistanceA = 10000.0 /* 10k resistor value (in ohms)*/ * ((3.3 /* ESP8266 supply voltage*/ / voltageA) - 1);
  thermistorTemperatureA = resistanceA / 100000.0 /* 100k thermistor at 25C*/; // (R/Ro)
  thermistorTemperatureA = log(thermistorTemperatureA); // ln(R/Ro)
  thermistorTemperatureA /= 3950.0 /* Beta coefficient of thermistor*/; // 1/B * ln(R/Ro)
  thermistorTemperatureA += 1.0 / (25.0 /* 25°C*/ + 273.15); // + (1/To)
  thermistorTemperatureA = 1.0 / thermistorTemperatureA; // Invert
  thermistorTemperatureA -= 273.15;

  // Read and calculate temperature for Thermistor B
  int adcValueB = analogRead(6 /* Analog pin where the voltage divider is connected for Thermistor B*/);
  float voltageB = (adcValueB / 4095.0 /* 12-bit ADC on ESP8266*/) * 3.3 /* ESP8266 supply voltage*/;
  float resistanceB = 10000.0 /* 10k resistor value (in ohms)*/ * ((3.3 /* ESP8266 supply voltage*/ / voltageB) - 1);
  thermistorTemperatureB = resistanceB / 100000.0 /* 100k thermistor at 25C*/; // (R/Ro)
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
  if (enableBeep == true && (thermistorTemperatureA >= targetTemperature || thermistorTemperatureB >= targetTemperature))
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

  // Indicate that the memory is almost full
  if (availablePercentage < 20)
  {
    heltec_led(100);
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawStringf(3, 0, buffer, "BACKUP!");
    display.drawStringf(63, 0, buffer, "%.0f%%", availablePercentage);
  }
  else
  {
    heltec_led(0);
  }

  yield();
  display.display();

  unsigned long currentTime = millis();
  yield();

  if ((currentTime - lastSaveTime >= 1000 /* 60000 milliseconds = 1 minute*/) || lastSaveTime == 0) // 60000 milliseconds = 1 minute
  {
    lastSaveTime = currentTime;
    yield();

    // Get current time from WiFi network
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
      // Write data to file
      file.printf("%s, %d, %d, %d,\n", timestamp, (int)thermistorTemperatureA, (int)thermistorTemperatureB, (int)lux);
      fileSize = file.size();
      // Send data via Lora
      txdata = String(timestamp) + "," + String((int)thermistorTemperatureA) + "," + String((int)thermistorTemperatureB) + "," + String((int)lux);

      handleLoraTx();
    }
    catch (const std::exception &e)
    {
      Serial0.print("Error writing to file: ");
      Serial0.println(e.what());
    }
    file.close();

    availableMemory = SPIFFS.totalBytes() - SPIFFS.usedBytes();
    availablePercentage = (float)availableMemory / SPIFFS.totalBytes() * 100;
    Serial0.printf("File size: %.2f Kbytes\n", (int)fileSize / 1024);
    Serial0.printf("Available memory: %.2f Kbytes (%.2f%%)\n", availableMemory / 1024, availablePercentage);

    yield();
  }
  yield();

  server.handleClient(); // Handle client requests
  yield();

  // wipe file if memory is less than 10% to keep system running
  if (availablePercentage < 10)
  {
    SPIFFS.remove(filename);
    fileSize = 0;
  }

  handleLoraRx();

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
  WiFi.begin("TP-Link_6178", "39511836");

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
  server.on("/raw", HTTP_GET, handleGetValues); // Handle getValues request
  server.on("/wipe", HTTP_GET, handleWipeFile); // Handle wipeFile request
  server.on("/enable-beep", HTTP_GET, handleEnableBeep);
  server.on("/disable-beep", HTTP_GET, handleDisableBeep);
  yield();
  server.begin();

  // Initialize NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  return true;
}

// Handle the root URL
void handleRoot()
{
  String html = "<html><head><meta http-equiv='refresh' content='" + String(10) + "'></head>"
                                                                                  "<body style='background-color:#ffcc00; text-align:center;font-family:Arial;'><h1>SUNFACTORY</h1>"
                                                                                  "<table border='1' style='margin-left:auto; margin-right:auto; text-align:center;'>"
                                                                                  "<tr><th>Timestamp</th><th>Temperature A</th><th>Temperature B</th><th>Lux</th></tr>";
  if (timestamp == nullptr || strlen(timestamp) == 0)
  {
    html += "<tr><td><a href='/'wait</a></td><td>" + String((int)thermistorTemperatureA) + " C</td><td>" + String((int)thermistorTemperatureB) + " C</td><td>" + String((int)lux) + " lm</td></tr>";
  }
  else
  {
    html += "<tr><td>" + String(timestamp) + " </td><td>" + String((int)thermistorTemperatureA) + " C</td><td>" + String((int)thermistorTemperatureB) + " C</td><td>" + String((int)lux) + " lm</td></tr>";
  }
  html += "</table><p><a href='/download'>Download LOG</a></p>";
  html += "<p>File size: " + String(fileSize / 1024) + " Kbytes</p>";
  html += "<p>Available memory: " + String(availableMemory / 1024) + " Kbytes (" + String(availablePercentage, 2) + "%)</p>";
  html += "<p>Beep " + String(enableBeep ? "Enabled" : "Disabled") + "</p>";
  if (!enableBeep)
  {
    html += "<p><a href='/enable-beep'>Enable Beep</a></p>";
  }
  else
  {
    html += "<p><a href='/disable-beep'>Disable Beep</a></p>";
  }
  html += "<p style='text-align:right;'><a href='/wipe'>Wipe LOG</a></p>";
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

// Handle the getValues URL
void handleGetValues()
{
  String json = "{";
  json += "\"timestamp\":\"" + String(timestamp) + "\",";
  json += "\"temperatureA\":" + String((int)thermistorTemperatureA) + ",";
  json += "\"temperatureB\":" + String((int)thermistorTemperatureB) + ",";
  json += "\"lux\":" + String((int)lux);
  json += "}";

  server.send(200, "application/json", json);
}

void handleWipeFile()
{
  if (server.hasArg("confirm") && server.arg("confirm") == "yes")
  {
    SPIFFS.remove(filename);
    fileSize = 0;
    server.sendHeader("Location", "/");
    server.send(303);
  }
  else
  {
    String html = "<html><body style='background-color:#ffcc00;'><h1>Confirm Wipe</h1>";
    html += "<p>Are you sure you want to wipe the log file?</p>";
    html += "<a href='/wipe?confirm=yes'>Yes</a> | <a href='/'>No</a>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  }
}

void handleEnableBeep()
{
  enableBeep = true;
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleDisableBeep()
{
  enableBeep = false;
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLoraTx()
{
  radio.transmit(txdata.c_str());
  radio.clearDio1Action();
  heltec_led(50); // 50% brightness is plenty for this LED
  tx_time = millis();
  _radiolib_status = radio.transmit(String(counter++).c_str()); Serial0.print("[RadioLib] "); Serial0.print("radio.transmit(String(counter++).c_str())"); Serial0.print(" returned "); Serial0.print(_radiolib_status); Serial0.print(" ("); Serial0.print(radiolib_result_string(_radiolib_status)); Serial0.println(")");;
  tx_time = millis() - tx_time;
  heltec_led(0);
  if (_radiolib_status == (0))
  {
    yield();
    // both.printf("OK (%i ms)\n", (int)tx_time);
  }
  else
  {
    both.printf("fail (%i)\n", _radiolib_status);
  }
  // Maximum 1% duty cycle
  minimum_pause = tx_time * 100;
  last_tx = millis();
  radio.setDio1Action(rx);
  _radiolib_status = radio.startReceive(0xFFFFFF /*  23    0                        infinite (Rx continuous mode)*/); Serial0.print("[RadioLib] "); Serial0.print("radio.startReceive(0xFFFFFF /*  23    0                        infinite (Rx continuous mode)*/)"); Serial0.print(" returned "); Serial0.print(_radiolib_status); Serial0.print(" ("); Serial0.print(radiolib_result_string(_radiolib_status)); Serial0.println(")");; if (_radiolib_status != (0)) { Serial0.println("[RadioLib] Halted"); while (true) { heltec_delay(10); } };
}

void handleLoraRx()
{
  // If a packet was received, display it and the RSSI and SNR
  if (rxFlag)
  {
    rxFlag = false;
    radio.readData(rxdata);
    if (_radiolib_status == (0))
    {
      yield();
      // both.printf("RX [%s]\n", rxdata.c_str());
      // both.printf("  RSSI: %.2f dBm\n", radio.getRSSI());
      // both.printf("  SNR: %.2f dB\n", radio.getSNR());
    }
    _radiolib_status = radio.startReceive(0xFFFFFF /*  23    0                        infinite (Rx continuous mode)*/); Serial0.print("[RadioLib] "); Serial0.print("radio.startReceive(0xFFFFFF /*  23    0                        infinite (Rx continuous mode)*/)"); Serial0.print(" returned "); Serial0.print(_radiolib_status); Serial0.print(" ("); Serial0.print(radiolib_result_string(_radiolib_status)); Serial0.println(")");; if (_radiolib_status != (0)) { Serial0.println("[RadioLib] Halted"); while (true) { heltec_delay(10); } };
  }
}

// Can't do Serial or display things here, takes too much time for the interrupt
void rx()
{
  rxFlag = true;
}
