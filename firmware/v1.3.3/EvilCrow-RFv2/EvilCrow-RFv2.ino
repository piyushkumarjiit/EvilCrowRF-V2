#include "ELECHOUSE_CC1101_SRC_DRV.h"
#include <WiFiClient.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include "esp_task_wdt.h"
#define DEST_FS_USES_SD
#include <ESP32-targz.h>
//#include <SPIFFSEditor.h>
#include <EEPROM.h>
#include "SPIFFS.h"
#include "SPI.h"
#include <WiFiAP.h>
#include "FS.h"
#include "SD.h"

#define eepromsize 4096
#define samplesize 2000

#define SD_SCLK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_SS 22

SPIClass sdspi(VSPI);

#if defined(ESP8266)
#define RECEIVE_ATTR ICACHE_RAM_ATTR
#elif defined(ESP32)
#define RECEIVE_ATTR IRAM_ATTR
#else
#define RECEIVE_ATTR
#endif

// Variables to hold parsed parameters from serial commands.
int count_binconvert = 0;         // Declare count_binconvert globally and initialize to 0
bool serialDebugEnabled = false;  // Flag to enable/disable serial debug logging
// --- WiFi Control Variable ---
bool wifiEnabled = false;  // Flag to control if WiFi should be active

// Config SSID, password and channel
const char *ssid = "Evil Crow RF v2";      // Enter your SSID here
const char *password = "123456789ECRFv2";  //Enter your Password here
const int wifi_channel = 1;                //Enter your preferred Wi-Fi Channel

// HTML and CSS style
//const String MENU = "<body><p>Evil Crow RF v1.0</p><div id=\"header\"><body><nav id='menu'><input type='checkbox' id='responsive-menu' onclick='updatemenu()'><label></label><ul><li><a href='/'>Home</a></li><li><a class='dropdown-arrow'>Config</a><ul class='sub-menus'><li><a href='/txconfig'>RAW TX Config</a></li><li><a href='/txbinary'>Binary TX Config</a></li><li><a href='/rxconfig'>RAW RX Config</a></li><li><a href='/btnconfig'>Button TX Config</a></li></ul></li><li><a class='dropdown-arrow'>RX Log</a><ul class='sub-menus'><li><a href='/viewlog'>RX Logs</a></li><li><a href='/delete'>Delete Logs</a></li><li><a href='/downloadlog'>Download Logs</a></li><li><a href='/cleanspiffs'>Clean SPIFFS</a></li></ul></li><li><a class='dropdown-arrow'>URH Protocol</a><ul class='sub-menus'><li><a href='/txprotocol'>TX Protocol</a></li><li><a href='/listxmlfiles'>List Protocol</a></li><li><a href='/uploadxmlfiles'>Upload Protocol</a></li><li><a href='/cleanspiffs'>Clean SPIFFS</a></li></ul></li><li><a href='/jammer'>Simple Jammer</a></li><li><a href='/update'>OTA Update</a></li></ul></nav><br></div>";
const String HTML_CSS_STYLING = "<html><head><meta charset=\"utf-8\"><title>Evil Crow RF</title><link rel=\"stylesheet\" href=\"style.css\"><script src=\"lib.js\"></script></head>";

//Pushbutton Pins
int push1 = 34;
int push2 = 35;

int led = 32;
static unsigned long Blinktime = 0;

int error_toleranz = 200;

int RXPin = 26;
int RXPin0 = 4;
int TXPin0 = 2;
int Gdo0 = 25;
const int minsample = 30;
unsigned long sample[samplesize];
unsigned long samplesmooth[samplesize];
int samplecount;
static unsigned long lastTime = 0;
String transmit = "";
long data_to_send[2000];
long data_button1[2000];
long data_button2[2000];
long data_button3[2000];
long transmit_push[2000];
String tmp_module;
String tmp_frequency;
String tmp_xmlname;
String tmp_codelen;
String tmp_setrxbw;
String tmp_mod;
int mod;
String tmp_deviation;
float deviation;
String tmp_datarate;
String tmp_powerjammer;
int power_jammer;
int datarate;
float frequency;
float setrxbw;
String raw_rx = "0";
String jammer_tx = "0";
const bool formatOnFail = true;
String webString;
String bindata;
int samplepulse;
String tmp_samplepulse;
String tmp_transmissions;
int counter = 0;
int pos = 0;
int transmissions;
int pushbutton1 = 0;
int pushbutton2 = 0;
byte jammer[11] = {
  0xff,
  0xff,
};

//BTN Sending Config
int btn_set_int;
String btn_set;
String btn1_frequency;
String btn1_mod;
String btn1_rawdata;
String btn1_deviation;
String btn1_transmission;
String btn2_frequency;
String btn2_mod;
String btn2_rawdata;
String btn2_deviation;
String btn2_transmission;
float tmp_btn1_deviation;
float tmp_btn2_deviation;
float tmp_btn1_frequency;
float tmp_btn2_frequency;
int tmp_btn1_mod;
int tmp_btn2_mod;
int tmp_btn1_transmission;
int tmp_btn2_transmission;
String bindataprotocol;
String bindata_protocol;
String btn1tesla = "0";
String btn2tesla = "0";
float tmp_btn1_tesla_frequency;
float tmp_btn2_tesla_frequency;

// Wi-Fi config storage
int storage_status;
String tmp_config1;
String tmp_config2;
String config_wifi;
String ssid_new;
String password_new;
String tmp_channel_new;
String tmp_mode_new;
int channel_new;
int mode_new;

// Tesla
const uint16_t pulseWidth = 400;
const uint16_t messageDistance = 23;
const uint8_t transmtesla = 5;
const uint8_t messageLength = 43;

// ElegantOTA
unsigned long ota_progress_millis = 0;

const uint8_t sequence[messageLength] = {
  0x02, 0xAA, 0xAA, 0xAA,  // Preamble of 26 bits by repeating 1010
  0x2B,                    // Sync byte
  0x2C, 0xCB, 0x33, 0x33, 0x2D, 0x34, 0xB5, 0x2B, 0x4D, 0x32, 0xAD, 0x2C, 0x56, 0x59, 0x96, 0x66,
  0x66, 0x5A, 0x69, 0x6A, 0x56, 0x9A, 0x65, 0x5A, 0x58, 0xAC, 0xB3, 0x2C, 0xCC, 0xCC, 0xB4, 0xD2,
  0xD4, 0xAD, 0x34, 0xCA, 0xB4, 0xA0
};

// Jammer
int jammer_pin;

// File
File logs;
File file;

AsyncWebServer controlserver(80);

esp_task_wdt_config_t wdt_config = {
  .timeout_ms = 1000,
  .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
  .trigger_panic = true
};

void onOTAStart() {
  Serial.println("OTA update started!");
}

void onOTAProgress(size_t current, size_t final) {
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
}

void readConfigWiFi(fs::FS &fs, String path) {
  File file = fs.open(path);

  if (!file || file.isDirectory()) {
    storage_status = 0;
    return;
  }

  while (file.available()) {
    config_wifi = file.readString();
    int file_len = config_wifi.length() + 1;
    int index_config = config_wifi.indexOf('\n');
    tmp_config1 = config_wifi.substring(0, index_config - 1);

    tmp_config2 = config_wifi.substring(index_config + 1, file_len - 3);
    storage_status = 1;
  }
  file.close();
}

void writeConfigWiFi(fs::FS &fs, const char *path, String message) {
  File file = fs.open(path, FILE_APPEND);

  if (!file) {
    return;
  }

  if (file.println(message)) {
  } else {
  }
  file.close();
}

// handles uploads
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();

  if (!index) {
    logmessage = "Upload Start: " + String(filename);
    request->_tempFile = SD.open("/URH/" + filename, "w");
  }

  if (len) {
    request->_tempFile.write(data, len);
    logmessage = "Writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len);
  }

  if (final) {
    logmessage = "Upload Complete: " + String(filename) + ",size: " + String(index + len);
    request->_tempFile.close();
    request->redirect("/");
  }
}

// handles uploads SD Files
void handleUploadSD(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  removeDir(SD, "/HTML");
  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();

  if (!index) {
    logmessage = "Upload Start: " + String(filename);
    request->_tempFile = SD.open("/" + filename, "w");
  }

  if (len) {
    request->_tempFile.write(data, len);
    logmessage = "Writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len);
  }

  if (final) {
    logmessage = "Upload Complete: " + String(filename) + ",size: " + String(index + len);
    request->_tempFile.close();
    tarGzFS.begin();

    TarUnpacker *TARUnpacker = new TarUnpacker();

    TARUnpacker->haltOnError(true);                                                             // stop on fail (manual restart/reset required)
    TARUnpacker->setTarVerify(true);                                                            // true = enables health checks but slows down the overall process
    TARUnpacker->setupFSCallbacks(targzTotalBytesFn, targzFreeBytesFn);                         // prevent the partition from exploding, recommended
    TARUnpacker->setTarProgressCallback(BaseUnpacker::defaultProgressCallback);                 // prints the untarring progress for each individual file
    TARUnpacker->setTarStatusProgressCallback(BaseUnpacker::defaultTarStatusProgressCallback);  // print the filenames as they're expanded
    TARUnpacker->setTarMessageCallback(BaseUnpacker::targzPrintLoggerCallback);                 // tar log verbosity

    if (!TARUnpacker->tarExpander(tarGzFS, "/HTML.tar", tarGzFS, "/")) {
      Serial.printf("tarExpander failed with return code #%d\n", TARUnpacker->tarGzGetError());
    }
    deleteFile(SD, "/HTML.tar");
    request->redirect("/");
  }
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  deleteFile(SD, "/dir.txt");

  File root = fs.open(dirname);
  if (!root) {
    return;
  }
  if (!root.isDirectory()) {
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      appendFile(SD, "/dir.txt", "  DIR : ", file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      appendFile(SD, "/dir.txt", "", "<br>");
      appendFile(SD, "/dir.txt", "", file.name());
      appendFile(SD, "/dir.txt", "  SIZE: ", "");
      appendFileLong(SD, "/dir.txt", file.size());
    }
    file = root.openNextFile();
  }
}

void appendFile(fs::FS &fs, const char *path, const char *message, String messagestring) {

  logs = fs.open(path, FILE_APPEND);
  if (!logs) {
    //Serial.println("Failed to open file for appending");
    return;
  }
  if (logs.print(message) | logs.print(messagestring)) {
    //Serial.println("Message appended");
  } else {
    //Serial.println("Append failed");
  }
  logs.close();
}

void appendFileLong(fs::FS &fs, const char *path, unsigned long messagechar) {
  //Serial.printf("Appending to file: %s\n", path);

  logs = fs.open(path, FILE_APPEND);
  if (!logs) {
    //Serial.println("Failed to open file for appending");
    return;
  }
  if (logs.print(messagechar)) {
    //Serial.println("Message appended");
  } else {
    //Serial.println("Append failed");
  }
  logs.close();
}

void deleteFile(fs::FS &fs, const char *path) {
  //Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    //Serial.println("File deleted");
  } else {
    //Serial.println("Delete failed");
  }
}


void readFile(fs::FS &fs, String path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    if (serialDebugEnabled) {
      Serial.print("ERROR: Failed to open file: ");  // Added debug
      Serial.println(path);                          // Added debug
    }
    return;
  }
  if (serialDebugEnabled) {
    Serial.print("DEBUG: File opened successfully: ");  // Added debug
    Serial.println(path);                               // Added debug
  }

  //Serial.print("Read from file: ");
  while (file.available()) {
    bindataprotocol = file.readString();
  }
  // Debug statement to print bindataprotocol value
  if (serialDebugEnabled) {
    Serial.println(bindataprotocol);
  }
  // Close the file
  file.close();
}

void removeDir(fs::FS &fs, const char *dirname) {

  File root = fs.open(dirname);
  if (!root) {
    if (serialDebugEnabled) {
      Serial.println("Failed to open directory");
    }
    return;
  }
  if (!root.isDirectory()) {
    if (serialDebugEnabled) {
      Serial.println("Not a directory");
    }
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (serialDebugEnabled) {
      Serial.print("Next file: ");
      Serial.print(file.name());
      Serial.println("");
    }

    if (file.isDirectory()) {
      deleteFile(SD, file.name());
    } else {
      deleteFile(SD, file.name());
    }
    file = root.openNextFile();
  }
}

bool checkReceived(void) {

  delay(1);
  if (samplecount >= minsample && micros() - lastTime > 100000) {
    detachInterrupt(RXPin0);
    detachInterrupt(RXPin);
    return 1;
  } else {
    return 0;
  }
}

void printReceived() {

  Serial.print("Count=");
  Serial.println(samplecount);
  appendFile(SD, "/logs.txt", NULL, "<br>\n");
  appendFile(SD, "/logs.txt", NULL, "Count=");
  appendFileLong(SD, "/logs.txt", samplecount);
  appendFile(SD, "/logs.txt", NULL, "<br>");

  for (int i = 1; i < samplecount; i++) {
    Serial.print(sample[i]);
    Serial.print(",");
    appendFileLong(SD, "/logs.txt", sample[i]);
    appendFile(SD, "/logs.txt", NULL, ",");
  }
  Serial.println();
  Serial.println();
  appendFile(SD, "/logs.txt", "<br>\n", "<br>\n");
  appendFile(SD, "/logs.txt", "\n", "\n");
}

void RECEIVE_ATTR receiver() {
  const long time = micros();
  const unsigned int duration = time - lastTime;

  if (duration > 100000) {
    samplecount = 0;
  }

  if (duration >= 100) {
    sample[samplecount++] = duration;
  }

  if (samplecount >= samplesize) {
    detachInterrupt(RXPin0);
    detachInterrupt(RXPin);
    checkReceived();
  }

  if (mod == 0 && tmp_module == "2") {
    if (samplecount == 1 and digitalRead(RXPin) != HIGH) {
      samplecount = 0;
    }
  }

  lastTime = time;
}

void enableReceive() {
  pinMode(RXPin0, INPUT);
  RXPin0 = digitalPinToInterrupt(RXPin0);
  ELECHOUSE_cc1101.SetRx();
  samplecount = 0;
  attachInterrupt(RXPin0, receiver, CHANGE);
  pinMode(RXPin, INPUT);
  RXPin = digitalPinToInterrupt(RXPin);
  ELECHOUSE_cc1101.SetRx();
  samplecount = 0;
  attachInterrupt(RXPin, receiver, CHANGE);
}

void parse_data() {

  bindata_protocol = "";
  int data_begin_bits = 0;
  int data_end_bits = 0;
  int data_begin_pause = 0;
  int data_end_pause = 0;
  int data_count = 0;

  for (int c = 0; c < bindataprotocol.length(); c++) {
    if (bindataprotocol.substring(c, c + 4) == "bits") {
      data_count++;
    }
  }
  Serial.print("Data Count:");
  Serial.println(data_count);

  for (int d = 0; d < data_count; d++) {
    data_begin_bits = bindataprotocol.indexOf("<message bits=", data_end_bits);
    data_end_bits = bindataprotocol.indexOf("decoding_index=", data_begin_bits + 1);
    bindata_protocol += bindataprotocol.substring(data_begin_bits + 15, data_end_bits - 2);

    data_begin_pause = bindataprotocol.indexOf("pause=", data_end_pause);
    data_end_pause = bindataprotocol.indexOf(" timestamp=", data_begin_pause + 1);
    bindata_protocol += "[Pause: ";
    bindata_protocol += bindataprotocol.substring(data_begin_pause + 7, data_end_pause - 1);
    bindata_protocol += " samples]\n";
  }
  bindata_protocol.replace(" ", "");
  bindata_protocol.replace("\n", "");
  bindata_protocol.replace("Pause:", "");
  if (serialDebugEnabled) {
    Serial.println("DEBUG: Parsed Data--->");
    Serial.println(bindata_protocol);
  }
}


void sendSignals() {
  pinMode(TXPin0, OUTPUT);
  ELECHOUSE_cc1101.setModul(0);
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setModulation(2);
  ELECHOUSE_cc1101.setMHZ(frequency);
  ELECHOUSE_cc1101.setDeviation(0);
  ELECHOUSE_cc1101.SetTx();

  for (uint8_t t = 0; t < transmtesla; t++) {
    for (uint8_t i = 0; i < messageLength; i++) sendByte(sequence[i]);
    digitalWrite(TXPin0, LOW);
    delay(messageDistance);
  }
}

void sendSignalsBT1() {
  pinMode(TXPin0, OUTPUT);
  ELECHOUSE_cc1101.setModul(0);
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setModulation(2);
  ELECHOUSE_cc1101.setMHZ(tmp_btn1_tesla_frequency);
  ELECHOUSE_cc1101.setDeviation(0);
  ELECHOUSE_cc1101.SetTx();

  for (uint8_t t = 0; t < transmtesla; t++) {
    for (uint8_t i = 0; i < messageLength; i++) sendByte(sequence[i]);
    digitalWrite(TXPin0, LOW);
    delay(messageDistance);
  }
}

void sendSignalsBT2() {
  pinMode(TXPin0, OUTPUT);
  ELECHOUSE_cc1101.setModul(0);
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setModulation(2);
  ELECHOUSE_cc1101.setMHZ(tmp_btn2_tesla_frequency);
  ELECHOUSE_cc1101.setDeviation(0);
  ELECHOUSE_cc1101.SetTx();

  for (uint8_t t = 0; t < transmtesla; t++) {
    for (uint8_t i = 0; i < messageLength; i++) sendByte(sequence[i]);
    digitalWrite(TXPin0, LOW);
    delay(messageDistance);
  }
}

void sendByte(uint8_t dataByte) {
  for (int8_t bit = 7; bit >= 0; bit--) {  // MSB
    digitalWrite(TXPin0, (dataByte & (1 << bit)) != 0 ? HIGH : LOW);
    delayMicroseconds(pulseWidth);
  }
}

void power_management() {
  EEPROM.begin(eepromsize);

  pinMode(push2, INPUT);
  pinMode(led, OUTPUT);

  byte z = EEPROM.read(eepromsize - 2);
  if (digitalRead(push2) != LOW) {
    if (z == 1) {
      go_deep_sleep();
    }
  } else {
    if (z == 0) {
      EEPROM.write(eepromsize - 2, 1);
      EEPROM.commit();
      go_deep_sleep();
    } else {
      EEPROM.write(eepromsize - 2, 0);
      EEPROM.commit();
    }
  }
}

void go_deep_sleep() {
  Serial.println("Going to sleep now");
  ELECHOUSE_cc1101.setModul(0);
  ELECHOUSE_cc1101.goSleep();
  ELECHOUSE_cc1101.setModul(1);
  ELECHOUSE_cc1101.goSleep();
  led_blink(5, 250);
  esp_deep_sleep_start();
}

void led_blink(int blinkrep, int blinktimer) {
  for (int i = 0; i < blinkrep; i++) {
    digitalWrite(led, HIGH);
    delay(blinktimer);
    digitalWrite(led, LOW);
    delay(blinktimer);
  }
}

void poweron_blink() {
  if (millis() - Blinktime > 10000) {
    digitalWrite(led, LOW);
  }
  if (millis() - Blinktime > 10100) {
    digitalWrite(led, HIGH);
    Blinktime = millis();
  }
}

void force_reset() {
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  while (true)
    ;
}

// --- Added WiFi Control Functions ---
// --- WiFi Control Functions ---
void startWiFi() {
  if (serialDebugEnabled) {
    Serial.println("DEBUG: startWiFi() called.");
  }

  // Add the WiFi startup logic here
  // This logic is adapted from your original setup() function

  if (storage_status == 0) {  // No saved config, default to AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password, wifi_channel, 8);
    if (serialDebugEnabled) {
      Serial.print("WiFi configured as AP. SSID: ");
      Serial.print(ssid);
      Serial.print(" Password: ");
      Serial.println(password);
    }
  } else if (storage_status == 1) {  // Saved config found
    if (mode_new == 1) {             // AP mode
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ssid_new.c_str(), password_new.c_str(), channel_new, 8);
      if (serialDebugEnabled) {
        Serial.print("WiFi configured as AP. SSID: ");
        Serial.print(ssid_new);
        Serial.print(" Password: ");
        Serial.println(password_new);
      }
    } else if (mode_new == 2) {  // STA mode
      WiFi.mode(WIFI_STA);
      Serial.print("Attempting to connect to WiFi SSID: ");
      Serial.println(ssid_new);  // User feedback for STA connection
      WiFi.begin(ssid_new.c_str(), password_new.c_str());
      // In STA mode, you might want to wait for connection or add connection status checks in the loop
    }
  }
  // Start the web server
  controlserver.begin();
  if (serialDebugEnabled) {
    Serial.println("DEBUG: Web server started.");
  }
  Serial.println("WiFi startup initiated.");  // User feedback
  // Note: For STA mode, connection might take time. You might need to check connection status in the loop.
}

void stopWiFi() {
  if (serialDebugEnabled) {
    Serial.println("DEBUG: stopWiFi() called.");
  }

  Serial.println("Attempting to stop WiFi...");  // User feedback

  // Stop the web server gracefully first, if possible.
  // AsyncWebServer doesn't have a simple stop() method,
  // but setting the mode to WIFI_OFF should make it inactive.
  // If you were using a blocking server, you might need server.stop().

  // Set the WiFi mode to WIFI_OFF to disable the interface
  WiFi.mode(WIFI_OFF);

  if (serialDebugEnabled) {
    Serial.println("DEBUG: WiFi mode set to WIFI_OFF.");
  }

  Serial.println("WiFi stopped.");  // User feedback
}

void setup() {

  Serial.begin(115200);
  // Handle initial power state based on button/EEPROM
  power_management();
  // Initialize file systems
  SPIFFS.begin(formatOnFail);
  // Read WiFi configuration
  readConfigWiFi(SPIFFS, "/configwifi.txt");
  ssid_new = tmp_config1;
  password_new = tmp_config2;
  readConfigWiFi(SPIFFS, "/configmode.txt");
  tmp_channel_new = tmp_config1;
  tmp_mode_new = tmp_config2;
  channel_new = tmp_channel_new.toInt();
  mode_new = tmp_mode_new.toInt();

  // --- Conditional WiFi Initialization ---
  // Initialize the wifiEnabled flag based on whether a config was found
  if (storage_status == 1) {
    // If a configuration was loaded from SPIFFS, enable the WiFi flag initially
    wifiEnabled = true;
    if (serialDebugEnabled) {
      Serial.println("DEBUG: WiFi configuration loaded. WiFi flag set to true for potential startup.");
    }
  } else {
    // If no configuration was loaded, WiFi flag remains false
    wifiEnabled = false;
    if (serialDebugEnabled) {
      Serial.println("DEBUG: No WiFi configuration loaded. WiFi flag remains false. WiFi will not start automatically.");
    }
  }

  // If the wifiEnabled flag is true after reading config, call startWiFi()
  if (wifiEnabled) {
    Serial.println("WiFi flag is true. Calling startWiFi()...");  // Optional debug print
    startWiFi();                                                  // Call the start WiFi function
  } else {
    Serial.println("WiFi flag is false. WiFi will not start automatically on boot.");  // Optional debug print
  }
  // --- End Conditional WiFi Initialization ---

  delay(2000);
  // Initial welcome and help message
  Serial.println("Evil Crow RF v2 - Serial Interface");

  sdspi.begin(18, 19, 23, 22);
  SD.begin(22, sdspi);
  // Added to debug SD card initialization issue via Serial
  if (!SD.begin(22, sdspi)) {
    Serial.println("ERROR: SD Card Initialization failed!");
    return;  // Stop setup if SD fails
  }
  if (serialDebugEnabled) {
    Serial.println("DEBUG: SD Card initialized successfully.");
  }

  pinMode(push1, INPUT);
  pinMode(push2, INPUT);

  controlserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/index.html", "text/html");
  });

  controlserver.on("/rxconfig", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/rxconfig.html", "text/html");
  });

  controlserver.on("/txconfig", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/txconfig.html", "text/html");
  });

  controlserver.on("/txprotocol", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/txprotocol.html", "text/html");
  });

  controlserver.on("/txbinary", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/txbinary.html", "text/html");
  });

  controlserver.on("/btnconfig", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/btn3.html", "text/html");
  });

  controlserver.on("/wificonfig", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/wificonfig.html", "text/html");
  });

  controlserver.on("/btnconfigtesla", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/btnconfigtesla.html", "text/html");
  });

  controlserver.on("/txprotocol", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/txprotocol.html", "text/html");
  });

  controlserver.on("/updatesd", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/updatesd.html", "text/html");
  });

  controlserver.on("/listxmlfiles", HTTP_GET, [](AsyncWebServerRequest *request) {
    listDir(SD, "/URH", 0);
    request->send(SD, "/dir.txt", "text/html");
  });

  controlserver.on("/uploadxmlfiles", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/uploadxmlfiles.html", "text/html");
  });

  controlserver.on(
    "/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
      request->send(200);
    },
    handleUpload);

  controlserver.on(
    "/uploadsd", HTTP_POST, [](AsyncWebServerRequest *request) {
      request->send(200);
    },
    handleUploadSD);

  controlserver.on("/jammer", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/jammer.html", "text/html");
  });

  controlserver.on("/txtesla", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/txtesla.html", "text/html");
  });

  controlserver.on("/stopjammer", HTTP_POST, [](AsyncWebServerRequest *request) {
    jammer_tx = "0";
    request->send(200, "text/html", HTML_CSS_STYLING + "<script>alert(\"Stop OK\")</script>");
    ELECHOUSE_cc1101.setModul(0);
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setModul(1);
    ELECHOUSE_cc1101.setSidle();
  });

  controlserver.on("/stopbtntesla", HTTP_POST, [](AsyncWebServerRequest *request) {
    btn1tesla = "0";
    btn2tesla = "0";
    request->send(200, "text/html", HTML_CSS_STYLING + "<script>alert(\"Stop OK\")</script>");
    ELECHOUSE_cc1101.setSidle();
  });

  controlserver.on("/setjammer", HTTP_POST, [](AsyncWebServerRequest *request) {
    raw_rx = "0";
    tmp_module = request->arg("module");
    tmp_frequency = request->arg("frequency");
    tmp_powerjammer = request->arg("powerjammer");

    if (request->hasArg("configmodule")) {
      frequency = tmp_frequency.toFloat();
      power_jammer = tmp_powerjammer.toInt();

      if (serialDebugEnabled) {
        Serial.println("Start");
      }

      if (tmp_module == "1") {
        pinMode(2, OUTPUT);
        ELECHOUSE_cc1101.setModul(0);
        ELECHOUSE_cc1101.Init();
        ELECHOUSE_cc1101.setModulation(2);
        ELECHOUSE_cc1101.setMHZ(frequency);
        ELECHOUSE_cc1101.setPA(power_jammer);
        ELECHOUSE_cc1101.SetTx();
        if (serialDebugEnabled) {
          Serial.println("Module 1");
        }
      }

      if (tmp_module == "2") {
        pinMode(25, OUTPUT);
        ELECHOUSE_cc1101.setModul(1);
        ELECHOUSE_cc1101.Init();
        ELECHOUSE_cc1101.setModulation(2);
        ELECHOUSE_cc1101.setMHZ(frequency);
        ELECHOUSE_cc1101.setPA(power_jammer);
        ELECHOUSE_cc1101.SetTx();
        if (serialDebugEnabled) {
          Serial.println("Module 2");
        }
      }
      //sdspi.end();
      //sdspi.begin(18, 19, 23, 22);
      //SD.begin(22, sdspi);
      jammer_tx = "1";
      request->send(200, "text/html", HTML_CSS_STYLING + "<script>alert(\"Jammer OK\")</script>");
    }
  });

  controlserver.on("/settx", HTTP_POST, [](AsyncWebServerRequest *request) {
    raw_rx = "0";
    tmp_module = request->arg("module");
    tmp_frequency = request->arg("frequency");
    transmit = request->arg("rawdata");
    tmp_deviation = request->arg("deviation");
    tmp_mod = request->arg("mod");
    tmp_transmissions = request->arg("transmissions");

    if (request->hasArg("configmodule")) {
      int counter = 0;
      int pos = 0;
      frequency = tmp_frequency.toFloat();
      deviation = tmp_deviation.toFloat();
      mod = tmp_mod.toInt();
      transmissions = tmp_transmissions.toInt();

      for (int i = 0; i < transmit.length(); i++) {
        if (transmit.substring(i, i + 1) == ",") {
          data_to_send[counter] = transmit.substring(pos, i).toInt();
          pos = i + 1;
          counter++;
        }
      }

      if (tmp_module == "1" || tmp_module == "2") {
        /* Commented to use configureCC1101 for initiatlization
        pinMode(2, OUTPUT);
        ELECHOUSE_cc1101.setModul(0);
        ELECHOUSE_cc1101.Init();
        ELECHOUSE_cc1101.setModulation(mod);
        ELECHOUSE_cc1101.setMHZ(frequency);
        ELECHOUSE_cc1101.setDeviation(deviation);
        //delay(400);
        ELECHOUSE_cc1101.SetTx();
        */
        // Convert to 0-based index
        int module_index = tmp_module.toInt() - 1;
        // Call the shared configuration function. true for TX mode
        configureCC1101(module_index, frequency, deviation, mod, true);
        // The pinMode, setModul, Init, setModulation, setMHZ, setDeviation, and SetTx calls are now handled inside configureCC1101.

        for (int r = 0; r < transmissions; r++) {
          for (int i = 0; i < counter; i += 2) {
            digitalWrite((module_index == 0) ? 2 : 25, HIGH);
            delayMicroseconds(data_to_send[i]);
            digitalWrite((module_index == 0) ? 2 : 25, LOW);
            delayMicroseconds(data_to_send[i + 1]);
            Serial.print(data_to_send[i]);
            Serial.print(",");
          }
          delay(2000);  //Set this for the delay between retransmissions
        }
      }
      Serial.println();
      request->send(200, "text/html", HTML_CSS_STYLING + "<script>alert(\"Signal has been transmitted\")</script>");
      ELECHOUSE_cc1101.setSidle();
    }
  });

  controlserver.on("/settxtesla", HTTP_POST, [](AsyncWebServerRequest *request) {
    raw_rx = "0";
    tmp_frequency = request->arg("frequency");

    if (request->hasArg("configmodule")) {
      frequency = tmp_frequency.toFloat();
      sendSignals();
      request->send(200, "text/html", HTML_CSS_STYLING + "<script>alert(\"Signal has been transmitted\")</script>");
      ELECHOUSE_cc1101.setSidle();
    }
  });

  controlserver.on("/settxbinary", HTTP_POST, [](AsyncWebServerRequest *request) {
    raw_rx = "0";
    tmp_module = request->arg("module");
    tmp_frequency = request->arg("frequency");
    bindata = request->arg("binarydata");
    tmp_deviation = request->arg("deviation");
    tmp_mod = request->arg("mod");
    tmp_samplepulse = request->arg("samplepulse");
    tmp_transmissions = request->arg("transmissions");

    if (request->hasArg("configmodule")) {
      int counter = 0;
      int pos = 0;
      frequency = tmp_frequency.toFloat();
      deviation = tmp_deviation.toFloat();
      mod = tmp_mod.toInt();
      samplepulse = tmp_samplepulse.toInt();
      transmissions = tmp_transmissions.toInt();

      for (int i = 0; i < 1000; i++) {
        data_to_send[i] = 0;
      }

      bindata.replace(" ", "");
      bindata.replace("\n", "");
      bindata.replace("Pause:", "");
      int count_binconvert = 0;
      String lastbit_convert = "1";
      Serial.println("");
      Serial.println(bindata);

      for (int i = 0; i < bindata.length() + 1; i++) {
        if (lastbit_convert != bindata.substring(i, i + 1)) {
          if (lastbit_convert == "1") {
            lastbit_convert = "0";
          } else if (lastbit_convert == "0") {
            lastbit_convert = "1";
          }
          count_binconvert++;
        }

        if (bindata.substring(i, i + 1) == "[") {
          data_to_send[count_binconvert] = bindata.substring(i + 1, bindata.indexOf("]", i)).toInt();
          lastbit_convert = "0";
          i += bindata.substring(i, bindata.indexOf("]", i)).length();
        } else {
          data_to_send[count_binconvert] += samplepulse;
        }
      }

      for (int i = 0; i < count_binconvert; i++) {
        Serial.print(data_to_send[i]);
        Serial.print(",");
      }

      if (tmp_module == "1") {
        pinMode(2, OUTPUT);
        ELECHOUSE_cc1101.setModul(0);
        ELECHOUSE_cc1101.Init();
        ELECHOUSE_cc1101.setModulation(mod);
        ELECHOUSE_cc1101.setMHZ(frequency);
        ELECHOUSE_cc1101.setDeviation(deviation);
        //delay(400);
        ELECHOUSE_cc1101.SetTx();

        delay(1000);

        for (int r = 0; r < transmissions; r++) {
          for (int i = 0; i < count_binconvert; i += 2) {
            digitalWrite(2, HIGH);
            delayMicroseconds(data_to_send[i]);
            digitalWrite(2, LOW);
            delayMicroseconds(data_to_send[i + 1]);
          }
          delay(2000);  //Set this for the delay between retransmissions
        }
      }

      else if (tmp_module == "2") {
        pinMode(25, OUTPUT);
        ELECHOUSE_cc1101.setModul(1);
        ELECHOUSE_cc1101.Init();
        ELECHOUSE_cc1101.setModulation(mod);
        ELECHOUSE_cc1101.setMHZ(frequency);
        ELECHOUSE_cc1101.setDeviation(deviation);
        //delay(400);
        ELECHOUSE_cc1101.SetTx();

        delay(1000);

        for (int r = 0; r < transmissions; r++) {
          for (int i = 0; i < count_binconvert; i += 2) {
            digitalWrite(25, HIGH);
            delayMicroseconds(data_to_send[i]);
            digitalWrite(25, LOW);
            delayMicroseconds(data_to_send[i + 1]);
          }
          delay(2000);  //Set this for the delay between retransmissions
        }
      }
      request->send(200, "text/html", HTML_CSS_STYLING + "<script>alert(\"Signal has been transmitted\")</script>");
      ELECHOUSE_cc1101.setSidle();
      //sdspi.end();
      //sdspi.begin(18, 19, 23, 22);
      //SD.begin(22, sdspi);
    }
  });

  controlserver.on("/settxprotocol", HTTP_POST, [](AsyncWebServerRequest *request) {
    raw_rx = "0";
    tmp_frequency = request->arg("frequency");
    tmp_deviation = request->arg("deviation");
    tmp_xmlname = request->arg("xmlname");
    tmp_mod = request->arg("mod");
    tmp_samplepulse = request->arg("samplepulse");

    bindata_protocol = "";

    if (request->hasArg("configmodule")) {

      int counter = 0;
      int pos = 0;
      frequency = tmp_frequency.toFloat();
      deviation = tmp_deviation.toFloat();
      mod = tmp_mod.toInt();
      samplepulse = tmp_samplepulse.toInt();

      for (int i = 0; i < 1000; i++) {
        data_to_send[i] = 0;
      }

      readFile(SD, tmp_xmlname);
      //readFile(SD, "/URH/protocol.proto.xml");
      // Debug statement added to check if the code crashed before this
      if (serialDebugEnabled) {
        Serial.println("readFile completed with BinData:");
        Serial.println(bindata_protocol);
      }

      // Call parse data on bindataprotocol and in turn populate bindata_protocol
      parse_data();
      if (serialDebugEnabled) {
        Serial.println("parse_data completed.");
      }

      int count_binconvert = 0;
      String lastbit_convert = "1";

      for (int i = 0; i < bindata_protocol.length() + 1; i++) {
        if (lastbit_convert != bindata_protocol.substring(i, i + 1)) {
          if (lastbit_convert == "1") {
            lastbit_convert = "0";
          } else if (lastbit_convert == "0") {
            lastbit_convert = "1";
          }
          count_binconvert++;
        }

        if (bindata_protocol.substring(i, i + 1) == "[") {
          data_to_send[count_binconvert] = bindata_protocol.substring(i + 1, bindata_protocol.indexOf("]", i)).toInt();
          lastbit_convert = "0";
          i += bindata_protocol.substring(i, bindata_protocol.indexOf("]", i)).length();
        } else {
          data_to_send[count_binconvert] += samplepulse;
        }
      }

      Serial.println("Data to send ready.");
      if (serialDebugEnabled) {
        for (int i = 0; i < count_binconvert; i++) {
          Serial.print(data_to_send[i]);
          Serial.print(",");
        }
        Serial.println();
      }


      pinMode(2, OUTPUT);
      ELECHOUSE_cc1101.setModul(0);
      ELECHOUSE_cc1101.Init();
      ELECHOUSE_cc1101.setModulation(mod);
      ELECHOUSE_cc1101.setMHZ(frequency);
      ELECHOUSE_cc1101.setDeviation(deviation);
      //delay(400);
      ELECHOUSE_cc1101.SetTx();

      delay(1000);

      for (int i = 0; i < count_binconvert; i += 2) {
        digitalWrite(2, HIGH);
        delayMicroseconds(data_to_send[i]);
        digitalWrite(2, LOW);
        delayMicroseconds(data_to_send[i + 1]);
      }

      request->send(200, "text/html", HTML_CSS_STYLING + "<script>alert(\"Signal has been transmitted\")</script>");
      ELECHOUSE_cc1101.setSidle();
    }
  });

  controlserver.on("/setrx", HTTP_POST, [](AsyncWebServerRequest *request) {
    tmp_module = request->arg("module");
    //Serial.print("Module: ");
    //Serial.println(tmp_module);
    tmp_frequency = request->arg("frequency");
    tmp_setrxbw = request->arg("setrxbw");
    tmp_mod = request->arg("mod");
    tmp_deviation = request->arg("deviation");
    tmp_datarate = request->arg("datarate");
    if (request->hasArg("configmodule")) {
      frequency = tmp_frequency.toFloat();
      setrxbw = tmp_setrxbw.toFloat();
      mod = tmp_mod.toInt();
      if (serialDebugEnabled) {
        Serial.print("Modulation: ");
        Serial.println(mod);
      }
      deviation = tmp_deviation.toFloat();
      datarate = tmp_datarate.toInt();

      if (tmp_module == "1") {
        ELECHOUSE_cc1101.setModul(0);
        ELECHOUSE_cc1101.Init();
      }

      else if (tmp_module == "2") {
        ELECHOUSE_cc1101.setModul(1);
        ELECHOUSE_cc1101.Init();

        if (mod == 2) {
          ELECHOUSE_cc1101.setDcFilterOff(0);
        }

        if (mod == 0) {
          ELECHOUSE_cc1101.setDcFilterOff(1);
        }
        if (serialDebugEnabled) {
          Serial.println("Module 2");
        }
      }

      ELECHOUSE_cc1101.setSyncMode(0);   // Combined sync-word qualifier mode. 0 = No preamble/sync. 1 = 16 sync word bits detected. 2 = 16/16 sync word bits detected. 3 = 30/32 sync word bits detected. 4 = No preamble/sync, carrier-sense above threshold. 5 = 15/16 + carrier-sense above threshold. 6 = 16/16 + carrier-sense above threshold. 7 = 30/32 + carrier-sense above threshold.
      ELECHOUSE_cc1101.setPktFormat(3);  // Format of RX and TX data. 0 = Normal mode, use FIFOs for RX and TX. 1 = Synchronous serial mode, Data in on GDO0 and data out on either of the GDOx pins. 2 = Random TX mode; sends random data using PN9 generator. Used for test. Works as normal mode, setting 0 (00), in RX. 3 = Asynchronous serial mode, Data in on GDO0 and data out on either of the GDOx pins.

      ELECHOUSE_cc1101.setModulation(mod);  // set modulation mode. 0 = 2-FSK, 1 = GFSK, 2 = ASK/OOK, 3 = 4-FSK, 4 = MSK.
      ELECHOUSE_cc1101.setRxBW(setrxbw);
      ELECHOUSE_cc1101.setMHZ(frequency);
      ELECHOUSE_cc1101.setDeviation(deviation);  // Set the Frequency deviation in kHz. Value from 1.58 to 380.85. Default is 47.60 kHz.
      ELECHOUSE_cc1101.setDRate(datarate);       // Set the Data Rate in kBaud. Value from 0.02 to 1621.83. Default is 99.97 kBaud!

      enableReceive();
      raw_rx = "1";
      //sdspi.end();
      //sdspi.begin(18, 19, 23, 22);
      //SD.begin(22, sdspi);
      request->send(200, "text/html", HTML_CSS_STYLING + "<script>alert(\"RX Config OK\")</script>");
    }
  });

  controlserver.on("/setbtn", HTTP_POST, [](AsyncWebServerRequest *request) {
    btn_set = request->arg("button");
    btn_set_int = btn_set.toInt();
    raw_rx = "0";
    btn1tesla = "0";
    btn2tesla = "0";

    if (btn_set_int == 1) {
      btn1_rawdata = request->arg("rawdata");
      btn1_deviation = request->arg("deviation");
      btn1_frequency = request->arg("frequency");
      btn1_mod = request->arg("mod");
      btn1_transmission = request->arg("transmissions");
      counter = 0;
      int pos = 0;
      for (int i = 0; i < btn1_rawdata.length(); i++) {
        if (btn1_rawdata.substring(i, i + 1) == ",") {
          data_button1[counter] = btn1_rawdata.substring(pos, i).toInt();
          pos = i + 1;
          counter++;
        }
      }
    }

    if (btn_set_int == 2) {
      btn2_rawdata = request->arg("rawdata");
      btn2_deviation = request->arg("deviation");
      btn2_frequency = request->arg("frequency");
      btn2_mod = request->arg("mod");
      btn2_transmission = request->arg("transmissions");
      counter = 0;
      int pos = 0;
      for (int i = 0; i < btn2_rawdata.length(); i++) {
        if (btn2_rawdata.substring(i, i + 1) == ",") {
          data_button2[counter] = btn2_rawdata.substring(pos, i).toInt();
          pos = i + 1;
          counter++;
        }
      }
    }
    request->send(200, "text/html", HTML_CSS_STYLING + "<script>alert(\"Button Config OK\")</script>");
  });

  controlserver.on("/setbtntesla", HTTP_POST, [](AsyncWebServerRequest *request) {
    btn_set = request->arg("button");
    btn_set_int = btn_set.toInt();
    raw_rx = "0";

    if (btn_set_int == 1) {
      btn1_frequency = request->arg("frequency");
      tmp_btn1_tesla_frequency = btn1_frequency.toFloat();
      if (serialDebugEnabled) {
        Serial.print("Freq: ");
        Serial.println(btn1_frequency);
      }
      btn1tesla = "1";
    }

    if (btn_set_int == 2) {
      btn2_frequency = request->arg("frequency");
      tmp_btn2_tesla_frequency = btn2_frequency.toFloat();
      btn2tesla = "1";
    }
    request->send(200, "text/html", HTML_CSS_STYLING + "<script>alert(\"Button Config OK\")</script>");
  });

  controlserver.on("/viewlog", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/logs.txt", "text/html");
  });

  controlserver.on("/cleanspiffs", HTTP_GET, [](AsyncWebServerRequest *request) {
    SPIFFS.remove("/");
    request->send(200, "text/html", HTML_CSS_STYLING + "<body onload=\"JavaScript:AutoRedirect()\">"
                                                       "<br><h2>SPIFFS cleared!<br>You will be redirected in 5 seconds.</h2></body>");
  });

  controlserver.on("/downloadlog", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/logs.txt", String(), true);
  });

  controlserver.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request) {
    deleteFile(SD, "/logs.txt");
    request->send(200, "text/html", HTML_CSS_STYLING + "<body onload=\"JavaScript:AutoRedirect()\">"
                                                       "<br><h2>File cleared!<br>You will be redirected in 5 seconds.</h2></body>");
    webString = "";
    appendFile(SD, "/logs.txt", "Viewlog:\n", "<br>\n");
  });

  controlserver.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/style.css", "text/css");
  });

  controlserver.on("/lib.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/HTML/javascript.js", "text/javascript");
  });

  controlserver.on("/setwificonfig", HTTP_POST, [](AsyncWebServerRequest *request) {
    String ssid_value = request->arg("ssid");
    String password_value = request->arg("password");
    String channel_value = request->arg("channel");
    String mode_value = request->arg("mode");

    if (request->hasArg("configmodule")) {
      deleteFile(SPIFFS, "/configwifi.txt");
      deleteFile(SPIFFS, "/configmode.txt");
      writeConfigWiFi(SPIFFS, "/configwifi.txt", ssid_value);
      writeConfigWiFi(SPIFFS, "/configwifi.txt", password_value);
      writeConfigWiFi(SPIFFS, "/configmode.txt", channel_value);
      writeConfigWiFi(SPIFFS, "/configmode.txt", mode_value);
      WiFi.disconnect();
      force_reset();
    }
  });

  controlserver.on("/deletewificonfig", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasArg("configmodule")) {
      deleteFile(SPIFFS, "/configwifi.txt");
      deleteFile(SPIFFS, "/configmode.txt");
      force_reset();
    }
  });

  ElegantOTA.begin(&controlserver);
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
  // Commented this as this is started in setup section if WiFi flag is enabled
  //controlserver.begin();

  ELECHOUSE_cc1101.addSpiPin(14, 12, 13, 5, 0);
  ELECHOUSE_cc1101.addSpiPin(14, 12, 13, 27, 1);
  appendFile(SD, "/logs.txt", "Viewlog:\n", "<br>\n");
}

void signalanalyse() {
#define signalstorage 10

  int signalanz = 0;
  int timingdelay[signalstorage];
  float pulse[signalstorage];
  long signaltimings[signalstorage * 2];
  int signaltimingscount[signalstorage];
  long signaltimingssum[signalstorage];
  long signalsum = 0;

  for (int i = 0; i < signalstorage; i++) {
    signaltimings[i * 2] = 100000;
    signaltimings[i * 2 + 1] = 0;
    signaltimingscount[i] = 0;
    signaltimingssum[i] = 0;
  }
  for (int i = 1; i < samplecount; i++) {
    signalsum += sample[i];
  }

  for (int p = 0; p < signalstorage; p++) {

    for (int i = 1; i < samplecount; i++) {
      if (p == 0) {
        if (sample[i] < signaltimings[p * 2]) {
          signaltimings[p * 2] = sample[i];
        }
      } else {
        if (sample[i] < signaltimings[p * 2] && sample[i] > signaltimings[p * 2 - 1]) {
          signaltimings[p * 2] = sample[i];
        }
      }
    }

    for (int i = 1; i < samplecount; i++) {
      if (sample[i] < signaltimings[p * 2] + error_toleranz && sample[i] > signaltimings[p * 2 + 1]) {
        signaltimings[p * 2 + 1] = sample[i];
      }
    }

    for (int i = 1; i < samplecount; i++) {
      if (sample[i] >= signaltimings[p * 2] && sample[i] <= signaltimings[p * 2 + 1]) {
        signaltimingscount[p]++;
        signaltimingssum[p] += sample[i];
      }
    }
  }

  int firstsample = signaltimings[0];

  signalanz = signalstorage;
  for (int i = 0; i < signalstorage; i++) {
    if (signaltimingscount[i] == 0) {
      signalanz = i;
      i = signalstorage;
    }
  }

  for (int s = 1; s < signalanz; s++) {
    for (int i = 0; i < signalanz - s; i++) {
      if (signaltimingscount[i] < signaltimingscount[i + 1]) {
        int temp1 = signaltimings[i * 2];
        int temp2 = signaltimings[i * 2 + 1];
        int temp3 = signaltimingssum[i];
        int temp4 = signaltimingscount[i];
        signaltimings[i * 2] = signaltimings[(i + 1) * 2];
        signaltimings[i * 2 + 1] = signaltimings[(i + 1) * 2 + 1];
        signaltimingssum[i] = signaltimingssum[i + 1];
        signaltimingscount[i] = signaltimingscount[i + 1];
        signaltimings[(i + 1) * 2] = temp1;
        signaltimings[(i + 1) * 2 + 1] = temp2;
        signaltimingssum[i + 1] = temp3;
        signaltimingscount[i + 1] = temp4;
      }
    }
  }

  for (int i = 0; i < signalanz; i++) {
    timingdelay[i] = signaltimingssum[i] / signaltimingscount[i];
  }

  if (firstsample == sample[1] and firstsample < timingdelay[0]) {
    sample[1] = timingdelay[0];
  }


  bool lastbin = 0;
  for (int i = 1; i < samplecount; i++) {
    float r = (float)sample[i] / timingdelay[0];
    int calculate = r;
    r = r - calculate;
    r *= 10;
    if (r >= 5) { calculate += 1; }
    if (calculate > 0) {
      if (lastbin == 0) {
        lastbin = 1;
      } else {
        lastbin = 0;
      }
      if (lastbin == 0 && calculate > 8) {
        Serial.print(" [Pause: ");
        Serial.print(sample[i]);
        Serial.println(" samples]");
        appendFile(SD, "/logs.txt", NULL, " [Pause: ");
        appendFileLong(SD, "/logs.txt", sample[i]);
        appendFile(SD, "/logs.txt", " samples]", "\n");
      } else {
        for (int b = 0; b < calculate; b++) {
          Serial.print(lastbin);
          appendFileLong(SD, "/logs.txt", lastbin);
        }
      }
    }
  }
  Serial.println();
  Serial.print("Samples/Symbol: ");
  Serial.println(timingdelay[0]);
  Serial.println();
  appendFile(SD, "/logs.txt", "<br>\n", "<br>\n");
  appendFile(SD, "/logs.txt", NULL, "Samples/Symbol: ");
  appendFileLong(SD, "/logs.txt", timingdelay[0]);
  appendFile(SD, "/logs.txt", NULL, "<br>\n");

  int smoothcount = 0;
  for (int i = 1; i < samplecount; i++) {
    float r = (float)sample[i] / timingdelay[0];
    int calculate = r;
    r = r - calculate;
    r *= 10;
    if (r >= 5) { calculate += 1; }
    if (calculate > 0) {
      samplesmooth[smoothcount] = calculate * timingdelay[0];
      smoothcount++;
    }
  }
  Serial.println("Rawdata corrected:");
  Serial.print("Count=");
  Serial.println(smoothcount + 1);
  appendFile(SD, "/logs.txt", NULL, "Count=");
  appendFileLong(SD, "/logs.txt", smoothcount + 1);
  appendFile(SD, "/logs.txt", "\n", "<br>\n");
  appendFile(SD, "/logs.txt", NULL, "Rawdata corrected:\n");
  for (int i = 0; i < smoothcount; i++) {
    Serial.print(samplesmooth[i]);
    Serial.print(",");
    transmit_push[i] = samplesmooth[i];
    appendFileLong(SD, "/logs.txt", samplesmooth[i]);
    appendFile(SD, "/logs.txt", NULL, ",");
  }
  Serial.println();
  Serial.println();
  appendFile(SD, "/logs.txt", NULL, "<br>\n");
  appendFile(SD, "/logs.txt", "-------------------------------------------------------\n", "<br>");
  return;
}

// New helper method added for Serial// Helper function to split a string by a delimiter
void splitString(String str, char delimiter, std::vector<String> &results) {
  results.clear();
  int startIndex = 0;
  int endIndex = str.indexOf(delimiter);
  while (endIndex != -1) {
    results.push_back(str.substring(startIndex, endIndex));
    startIndex = endIndex + 1;
    endIndex = str.indexOf(delimiter, startIndex);
  }
  results.push_back(str.substring(startIndex));
}

// New Helper method to display Help via Serial
void printHelp() {
  Serial.println("\n--- Available Serial Commands ---");
  Serial.println("HELP - Display this command list");
  Serial.println("DEBUG_ON - Enable serial debug logging");
  Serial.println("DEBUG_OFF - Disable serial debug logging");
  Serial.println("WIFI_ON - Enable and start the WiFi interface and web server");
  Serial.println("WIFI_OFF - Disable and stop the WiFi interface and web server");
  Serial.println("TX_URH,<module>,<frequency>,<deviation>,<xmlname>,<mod>,<samplepulse>,<transmissions>");
  /*
  Serial.println("TX_RAW,<module>,<frequency>,<rawdata>,<deviation>,<mod>,<transmissions>");
  Serial.println("TX_BIN,<module>,<frequency>,<binarydata>,<deviation>,<mod>,<samplepulse>,<transmissions>");  
  Serial.println("TX_TESLA1,<frequency>");
  Serial.println("TX_TESLA2,<frequency>");
  Serial.println("RX_START,<module>,<frequency>,<setrxbw>,<mod>,<deviation>,<datarate>");
  Serial.println("RX_STOP - Stop current RF reception");
  Serial.println("JAMMER_ON,<module>,<frequency>,<powerjammer>");
  Serial.println("JAMMER_OFF - Stop current jamming");
  */
  Serial.println("-----------------------------------");
  Serial.println("Parameters:");
  Serial.println("  <module>: 1 or 2 (for CC1101 modules)");
  Serial.println("  <frequency>: Frequency in MHz (e.g., 433.92)");
  Serial.println("  <mod>: Modulation type (0=2FSK, 1=GFSK, 2=ASK/OOK, 3=4FSK, 4=MSK)");
  Serial.println("  <deviation>: Frequency deviation in kHz (float)");
  Serial.println("  <samplepulse>: Samples per symbol (integer)");
  Serial.println("  <transmissions>: Number of times to transmit (integer)");
  Serial.println("  <xmlname>: Full Path to URH XML file on SD card (e.g., /URH/signal.proto.xml)");
  /*
  Serial.println("  <rawdata>: Comma-separated pulse durations in microseconds");
  Serial.println("  <binarydata>: Binary string with [Pause: samples] markers");  
  Serial.println("  <setrxbw>: Receive bandwidth (float)");
  Serial.println("  <datarate>: Data rate in kBaud (integer)");
  Serial.println("  <powerjammer>: Jammer power setting (integer)");
  */
  Serial.println("-----------------------------------");
}

// New helper method added for Serial// Helper function to populate timing data from bindata_protocol into count_binconvert
void populateTimingData() {
  // This function populates data_to_send and count_binconvert from the global bindata_protocol (with underscore) string

  count_binconvert = 0;  // Initialize count_binconvert BEFORE this loop
  String lastbit_convert = "1";

  for (int i = 0; i < bindata_protocol.length() + 1; i++) {
    if (lastbit_convert != bindata_protocol.substring(i, i + 1)) {
      if (lastbit_convert == "1") {
        lastbit_convert = "0";
      } else if (lastbit_convert == "0") {
        lastbit_convert = "1";
      }
      count_binconvert++;
    }

    if (bindata_protocol.substring(i, i + 1) == "[") {
      data_to_send[count_binconvert] = bindata_protocol.substring(i + 1, bindata_protocol.indexOf("]", i)).toInt();
      lastbit_convert = "0";
      i += bindata_protocol.substring(i, bindata_protocol.indexOf("]", i)).length();
    } else {
      data_to_send[count_binconvert] += samplepulse;
    }
  }

  Serial.println("Data to Send");
  if (serialDebugEnabled) {
        for (int i = 0; i < count_binconvert; i++) {
          Serial.print(data_to_send[i]);
          Serial.print(",");
        }
        Serial.println();
      }
  
}

// Helper function to configure a CC1101 module
void configureCC1101(int cc_module_index, float frequency, float deviation, int modulation, bool setTxMode) {
  if (serialDebugEnabled) {
    Serial.print("DEBUG: Configuring CC1101 Module ");
    // Print 1-based module number
    Serial.print(cc_module_index + 1);
    Serial.print(" -> Freq: ");
    Serial.print(frequency);
    Serial.print(", Dev: ");
    Serial.print(deviation);
    Serial.print(", Mod: ");
    Serial.print(modulation);
    Serial.print(", Mode: ");
    Serial.println(setTxMode ? "TX" : "RX");
  }
  // Select the desired module (0 for Module 1, 1 for Module 2)
  ELECHOUSE_cc1101.setModul(cc_module_index);
  // Initialize the module with default/previous settings first
  ELECHOUSE_cc1101.Init();
  // Set the core RF parameters
  ELECHOUSE_cc1101.setMHZ(frequency);
  ELECHOUSE_cc1101.setDeviation(deviation);
  // 0=2FSK, 1=GFSK, 2=ASK/OOK, 3=4FSK, 4=MSK
  ELECHOUSE_cc1101.setModulation(modulation);

  // --- Add flush commands here if setting to TX ---
  if (setTxMode) {
    // Ensure idle before flushing/setting mode
    ELECHOUSE_cc1101.setSidle();
    // Flush TX FIFO
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SFTX);
    if (serialDebugEnabled) {
      Serial.print("DEBUG: Flushed CC1101 TX Module ");
    }
    // Optional: ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX); // Flush RX FIFO
  }


  // Set the operating mode (TX or RX)
  if (setTxMode) {
    // Need to set the correct TX pin mode before setting to TX. Assuming Pin 2 for Module 1, Pin 25 for Module 2 TX
    int tx_pin = (cc_module_index == 0) ? 2 : 25;
    pinMode(tx_pin, OUTPUT);
    digitalWrite(tx_pin, LOW);
    ELECHOUSE_cc1101.SetTx();
    delayMicroseconds(10);
  } else {
    // To be updated later as RX methods are not impleneted via Serial. For now, just setting to RX mode:
    ELECHOUSE_cc1101.SetRx();
  }
}

// Helper function to generate a single pulse pair on the correct TX pin. It takes the module index (0 or 1), and the durations for the HIGH and LOW pulses in microseconds.
void generatePulsePair(int cc_module_index, unsigned long high_duration_us, unsigned long low_duration_us) {
  // Determine the correct TX pin based on the module index. Assuming Pin 2 for Module 1 (index 0), Pin 25 for Module 2 (index 1)
  int tx_pin = (cc_module_index == 0) ? 2 : 25;
  // --- Minimal Debug Logging Attempt ---
  /*if (serialDebugEnabled) {
      Serial.print("[GP] HIGH:"); // Just print an indicator
      Serial.print(high_duration_us);
      Serial.print(" LOW:"); // Just print an indicator
      Serial.println(low_duration_us);
      Serial.flush(); // Try flushing the buffer immediately
  }
  */
  // Generate the pulse pair
  digitalWrite(tx_pin, HIGH);
  // --- Add a very small delay AFTER the pin goes HIGH ---
  /*if (high_duration_us > 0) { // Only add this if the HIGH pulse has a duration
      delayMicroseconds(2); // <-- Add this line. Try 1 or 2 microseconds.
  }*/
  delayMicroseconds(high_duration_us);
  digitalWrite(tx_pin, LOW);
  delayMicroseconds(low_duration_us);
}

/*
// Placeholder for TX_RAW command processing for Serial
void processTxRawCommand(String commandString) {
  if (serialDebugEnabled) {
    Serial.print("DEBUG: Called processTxRawCommand with: ");
    Serial.println(commandString);
  }
  // TODO: Implement logic for TX_RAW command here
}

// Placeholder for TX_BIN command processing for Serial
void processTxBinCommand(String commandString) {
  if (serialDebugEnabled) {
    Serial.print("DEBUG: Called processTxBinCommand with: ");
    Serial.println(commandString);
  }
  // TODO: Implement logic for TX_BIN command here
}

// Placeholder for TX_TESLA1 command processing for Serial
void processTxTesla1Command(String commandString) {
  if (serialDebugEnabled) {
    Serial.print("DEBUG: Called processTxTesla1Command with: ");
    Serial.println(commandString);
  }
  // TODO: Implement logic for TX_TESLA1 command here
}

// Placeholder for TX_TESLA2 command processing for Serial
void processTxTesla2Command(String commandString) {
  if (serialDebugEnabled) {
    Serial.print("DEBUG: Called processTxTesla2Command with: ");
    Serial.println(commandString);
  }
  // TODO: Implement logic for TX_TESLA2 command here
}

// Placeholder for RX_START command processing for Serial
void processRxStartCommand(String commandString) {
  if (serialDebugEnabled) {
    Serial.print("DEBUG: Called processRxStartCommand with: ");
    Serial.println(commandString);
  }
  // TODO: Implement logic for RX_START command here
}

// Placeholder for RX_STOP command processing for Serial
void stopReceiveSerial() {
  if (serialDebugEnabled) {
    Serial.println("DEBUG: Called stopReceiveSerial");
  }
  // TODO: Implement logic to stop receiving here
  // This will likely involve detaching interrupts and setting the CC1101 to idle
}

// Placeholder for JAMMER_ON command processing for Serial
void processJammerOnCommand(String commandString) {
  if (serialDebugEnabled) {
    Serial.print("DEBUG: Called processJammerOnCommand with: ");
    Serial.println(commandString);
  }
  // TODO: Implement logic for JAMMER_ON command here
}

// Placeholder for JAMMER_OFF command processing for Serial
void stopJammerSerial() {
  if (serialDebugEnabled) {
    Serial.println("DEBUG: Called stopJammerSerial");
  }
  // TODO: Implement logic to stop jamming here
}

*/

// New method for sending signal using proto.xml via Serial. Similar to /settxprotocol in WEb interface
void processTxUrhCommand(String commandString) {

  if (serialDebugEnabled) {
    Serial.print("DEBUG: Called processTxUrhCommand with: ");
    Serial.println(commandString);
  }
  std::vector<String> params;
  splitString(commandString, ',', params);
  String serial_module;
  String serial_frequency_str;
  String serial_deviation_str;
  String serial_xmlname;
  String serial_mod_str;
  String serial_samplepulse_str;
  String serial_transmissions_str;

  if (params.size() == 7) {
    serial_module = params[0];
    serial_frequency_str = params[1];
    serial_deviation_str = params[2];
    // full path to the proto.xml file, e.g., /URH/Start.proto.xml
    serial_xmlname = params[3];
    serial_mod_str = params[4];
    serial_samplepulse_str = params[5];
    serial_transmissions_str = params[6];

    if (serialDebugEnabled) {
      Serial.print("DEBUG: Parsed parameters: Module=");
      Serial.print(serial_module);
      Serial.print(", Freq=");
      Serial.print(serial_frequency_str);
      Serial.print(", Dev=");
      Serial.print(serial_deviation_str);
      Serial.print(", XML=");
      Serial.print(serial_xmlname);
      Serial.print(", Mod=");
      Serial.print(serial_mod_str);
      Serial.print(", SP=");
      Serial.println(serial_samplepulse_str);
    }

    frequency = serial_frequency_str.toFloat();
    deviation = serial_deviation_str.toFloat();
    mod = serial_mod_str.toInt();
    samplepulse = serial_samplepulse_str.toInt();
    //int temp_transmissions = serial_transmissions_str.toInt();
    transmissions = ((serial_transmissions_str.toInt() > 0) ? serial_transmissions_str.toInt() : 1);

    for (int i = 0; i < samplesize; i++) {
      data_to_send[i] = 0;
    }
    bindata_protocol = "";

    if (serialDebugEnabled) {
      Serial.print("DEBUG: Attempting to read file: ");
      Serial.println(serial_xmlname);
    }
    // Read passed xml (protocol) file
    readFile(SD, serial_xmlname);

    if (serialDebugEnabled) {
      Serial.print("DEBUG: readFile finished. bindataprotocol length: ");
      Serial.println(bindataprotocol.length());
    }

    if (bindataprotocol.length() > 0) {
      if (serialDebugEnabled) {
        Serial.println("DEBUG: File read successfully, parsing data...");
      }
      bindata_protocol = "";
      // This populates bindata_protocol
      parse_data();

      // Now, populate data_to_send and count_binconvert from bindata_protocol
      populateTimingData();

      if (serialDebugEnabled) {
        Serial.print("DEBUG: parse_data finished. count_binconvert = ");
        Serial.println(count_binconvert);
      }

      if (count_binconvert > 0) {
        if (serialDebugEnabled) {
          Serial.print("DEBUG: Parsed ");
          Serial.print(count_binconvert);
        }
        Serial.println("Completed parsing timing values. Configuring CC1101 and transmitting...");

        int cc_module_index = serial_module.toInt() - 1;
        if (cc_module_index >= 0 && cc_module_index < 2) {
          // Transmit the signal "transmissions" times
          for (int r = 0; r < transmissions; r++) {
            if (serialDebugEnabled) {
              Serial.println("DEBUG: Transmission loop " + String(r + 1) + " started.");
            }
            ELECHOUSE_cc1101.setSidle();
            // New shared configuration function here to initialize CC1101
            configureCC1101(cc_module_index, frequency, deviation, mod, true);  // true because TX_URH is for Transmit
            delay(200);

            for (int i = 0; i < count_binconvert; i += 2) {
              if (i + 1 < count_binconvert) {
                /* Repalced by the generatePulsePair method. Same change can be done at multiple other Web interfaces as well
              digitalWrite((cc_module_index == 0) ? 2 : 25, HIGH);
              delayMicroseconds(data_to_send[i]);
              digitalWrite((cc_module_index == 0) ? 2 : 25, LOW);
              delayMicroseconds(data_to_send[i + 1]);
              */
                generatePulsePair(cc_module_index, data_to_send[i], data_to_send[i + 1]);
              }
            }

            delay(2000);
          }
          if (serialDebugEnabled) {
            Serial.println("DEBUG: Transmission loop finished.");
          }
          ELECHOUSE_cc1101.setSidle();
          Serial.println("CC1101 set to idle.");
        } else {
          Serial.println("ERROR: Invalid module specified.");
        }
      } else {
        Serial.println("ERROR: No valid timing data parsed from XML (count_binconvert is 0).");
      }
    } else {
      Serial.println("ERROR: Failed to read or file is empty.");
    }
  } else {
    Serial.println("ERROR: Invalid TX_URH command format or parameter count.");
    Serial.println("Expected format: TX_URH module,frequency,deviation,xmlname,mod,samplepulse");
    Serial.print("Received parameters count: ");
    Serial.println(params.size());
  }
}
// New Method for Serial commands
void handleSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    Serial.print("Received command: ");
    Serial.println(command);
    if (command.equalsIgnoreCase("HELP") || command.equalsIgnoreCase("?")) {
      // Call the printHelp function
      printHelp();
    } else if (command.equalsIgnoreCase("DEBUG_ON")) {
      serialDebugEnabled = true;
      Serial.println("Serial debug logging enabled.");
    } else if (command.equalsIgnoreCase("DEBUG_OFF")) {
      Serial.println("Serial debug logging disabled.");
      serialDebugEnabled = false;
    }  // Check if WIFI already ON
    else if (command.equalsIgnoreCase("WIFI_ON")) {
      if (!wifiEnabled) {
        wifiEnabled = true;
        Serial.println("Attempting to start WiFi...");
        // Call the function to start WiFi
        startWiFi();

      } else {
        Serial.println("WiFi is already on.");
      }
    }  // Check if WIFI already OFF
    else if (command.equalsIgnoreCase("WIFI_OFF")) {
      if (wifiEnabled) {
        wifiEnabled = false;
        Serial.println("Attempting to stop WiFi...");
        // Call the function to stop WiFi
        stopWiFi();
      } else {
        Serial.println("WiFi is already off.");
      }
      /*  Commenting this section as not being used. Might update in future if required.
    } else if (command.startsWith("TX_RAW")) {
      processTxRawCommand(command.substring(7));
    } else if (command.startsWith("TX_BIN")) {
      processTxBinCommand(command.substring(7));
    } else if (command.startsWith("TX_TESLA1")) {
      processTxTesla1Command(command.substring(9));
    } else if (command.startsWith("TX_TESLA2")) {
      processTxTesla2Command(command.substring(9));
    } else if (command.startsWith("RX_START")) {
      processRxStartCommand(command.substring(9));
    } else if (command.startsWith("RX_STOP")) {
      stopReceiveSerial();
    } else if (command.startsWith("JAMMER_ON")) {
      processJammerOnCommand(command.substring(10));
    } else if (command.startsWith("JAMMER_OFF")) {
      stopJammerSerial();
      */
    } else if (command.startsWith("TX_URH")) {
      processTxUrhCommand(command.substring(7));
    } else {
      Serial.println("Unknown command.");
    }
  }
}


void loop() {
  ElegantOTA.loop();
  poweron_blink();

  pushbutton1 = digitalRead(push1);
  pushbutton2 = digitalRead(push2);
  // Serial command processing
  handleSerialCommands();

  if (raw_rx == "1") {
    if (checkReceived()) {
      printReceived();
      signalanalyse();
      enableReceive();
      delay(200);
      //sdspi.end();
      //sdspi.begin(18, 19, 23, 22);
      //SD.begin(22, sdspi);
      delay(500);
    }
  }

  if (jammer_tx == "1") {
    raw_rx = "0";

    if (tmp_module == "1") {
      for (int i = 0; i < 12; i += 2) {
        digitalWrite(2, HIGH);
        delayMicroseconds(jammer[i]);
        digitalWrite(2, LOW);
        delayMicroseconds(jammer[i + 1]);
      }
    } else if (tmp_module == "2") {
      for (int i = 0; i < 12; i += 2) {
        digitalWrite(25, HIGH);
        delayMicroseconds(jammer[i]);
        digitalWrite(25, LOW);
        delayMicroseconds(jammer[i + 1]);
      }
    }
  }

  if (pushbutton1 == LOW) {
    if (btn1tesla == "1") {
      sendSignalsBT1();
    } else {
      raw_rx = "0";
      tmp_btn1_deviation = btn1_deviation.toFloat();
      tmp_btn1_mod = btn1_mod.toInt();
      tmp_btn1_frequency = btn1_frequency.toFloat();
      tmp_btn1_transmission = btn1_transmission.toInt();
      pinMode(25, OUTPUT);
      ELECHOUSE_cc1101.setModul(1);
      ELECHOUSE_cc1101.Init();
      ELECHOUSE_cc1101.setModulation(tmp_btn1_mod);
      ELECHOUSE_cc1101.setMHZ(tmp_btn1_frequency);
      ELECHOUSE_cc1101.setDeviation(tmp_btn1_deviation);
      //delay(400);
      ELECHOUSE_cc1101.SetTx();

      for (int r = 0; r < tmp_btn1_transmission; r++) {
        for (int i = 0; i < counter; i += 2) {
          digitalWrite(25, HIGH);
          delayMicroseconds(data_button1[i]);
          digitalWrite(25, LOW);
          delayMicroseconds(data_button1[i + 1]);
          Serial.print(data_button1[i]);
          Serial.print(",");
        }
        delay(2000);  //Set this for the delay between retransmissions
      }
      Serial.println();
      ELECHOUSE_cc1101.setSidle();
    }
  }

  if (pushbutton2 == LOW) {
    if (btn2tesla == "1") {
      sendSignalsBT2();
    } else {
      raw_rx = "0";
      tmp_btn2_deviation = btn2_deviation.toFloat();
      tmp_btn2_mod = btn2_mod.toInt();
      tmp_btn2_frequency = btn2_frequency.toFloat();
      tmp_btn2_transmission = btn2_transmission.toInt();
      pinMode(25, OUTPUT);
      ELECHOUSE_cc1101.setModul(1);
      ELECHOUSE_cc1101.Init();
      ELECHOUSE_cc1101.setModulation(tmp_btn2_mod);
      ELECHOUSE_cc1101.setMHZ(tmp_btn2_frequency);
      ELECHOUSE_cc1101.setDeviation(tmp_btn2_deviation);
      ELECHOUSE_cc1101.SetTx();

      for (int r = 0; r < tmp_btn2_transmission; r++) {
        for (int i = 0; i < counter; i += 2) {
          digitalWrite(25, HIGH);
          delayMicroseconds(data_button2[i]);
          digitalWrite(25, LOW);
          delayMicroseconds(data_button2[i + 1]);
          Serial.print(data_button2[i]);
          Serial.print(",");
        }
        delay(2000);  //Set this for the delay between retransmissions
      }
      Serial.println();
      ELECHOUSE_cc1101.setSidle();
    }
  }
}
