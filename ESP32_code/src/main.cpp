#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <HTTPClient.h>
#include "SPIFFS.h"
#include "WiFiCredentials.h"
#include "I2SMEMSSampler.h"
#include "ADCSampler.h"
#include <AsyncElegantOTA.h>
#include <ESPmDNS.h>
#include "soc/soc.h" //disable esp brown out
#include "soc/rtc_cntl_reg.h" //disable esp brown out

WiFiClient *wifiClientADC = NULL;
HTTPClient *httpClientADC = NULL;
WiFiClient *wifiClientI2S = NULL;
HTTPClient *httpClientI2S = NULL;
ADCSampler *adcSampler = NULL;
I2SSampler *i2sSampler = NULL;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";

//Variables to save values from HTML form
String ssid;
String pass;
String ip;

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";

IPAddress localIP;

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

// Set LED GPIO
const int ledGPin = 14;
const int ledRPin = 15;
const int ledBPin = 13;

#define Dip1 18
#define Dip2 17
#define Dip3 16

int nodeNumber = 0;

String ledState;

void listAllFiles(){
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while(file){
 
      Serial.print("FILE: ");
      Serial.println(file.name());
 
      file = root.openNextFile();
  }
}

// Initialize SPIFFS
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}

// Read File from SPIFFS
String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }
  
  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;     
  }
  return fileContent;
}

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- frite failed");
  }
}

// Initialize WiFi
bool initWiFi() {
  if(ssid=="" || ip==""){
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while(WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  Serial.println(WiFi.localIP());
  return true;
}

// Replaces placeholder with LED state value
String processor(const String& var) {
  if(var == "STATE") {
    if(digitalRead(ledBPin)) {
      ledState = "ON";
    }
    else {
      ledState = "OFF";
    }
    return ledState;
  }
  return String();
}

// i2s config for using the internal ADC
i2s_config_t adcI2SConfig = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_I2S_LSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};

// i2s config for reading from left channel of I2S
i2s_config_t i2sMemsConfigLeftChannel = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};

// i2s pins
i2s_pin_config_t i2sPins = {
    .bck_io_num = GPIO_NUM_32,
    .ws_io_num = GPIO_NUM_25,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = GPIO_NUM_33};

// how many samples to read at once
const int SAMPLE_SIZE = 16384/4; 

// send data to a remote address
void sendData(WiFiClient *wifiClient, HTTPClient *httpClient, const char *url, uint8_t *bytes, size_t count)
{
  // send them off to the server
  digitalWrite(ledBPin, HIGH);
  httpClient->begin(*wifiClient, url);
  httpClient->addHeader("content-type", "application/octet-stream");
  httpClient->POST(bytes, count);
  httpClient->end();
  digitalWrite(ledBPin, LOW);
}

int16_t *samples = (int16_t *)malloc(sizeof(uint16_t) * SAMPLE_SIZE);
int samples_read;

// Semaphore to lock access to samples
SemaphoreHandle_t samplesSemaphore;

TaskHandle_t sendData1TaskHandle = NULL;

// send data to a remote address
void sendData1(void *param)
{
  Serial.println("send data");  
  int notificationValue;
  String server_url = "http://" + ip + ":5003/i2s_samples";
  Serial.println(server_url);  

  size_t samples_count; 
  size_t count;
  int16_t *local_samples = (int16_t *)malloc(sizeof(uint16_t) * SAMPLE_SIZE);

  uint8_t *bytes;
  
  WiFiClient *wifiClient = wifiClientI2S;
  HTTPClient *httpClient = httpClientI2S;

  while (true)
  {
    // Wait for a notification to send data
    notificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  
    if(notificationValue > 0)
    {
      Serial.println("sendData");

      xSemaphoreTake(samplesSemaphore, portMAX_DELAY);
      memcpy(local_samples, samples, samples_count);
      xSemaphoreGive(samplesSemaphore);

      samples_count = samples_read * sizeof(int16_t);
      count = samples_count;
      bytes = (uint8_t *)samples;

      Serial.println(notificationValue);
      // send them off to the server
      digitalWrite(ledBPin, HIGH);
      httpClient->begin(*wifiClient, server_url);
      httpClient->addHeader("content-type", "application/octet-stream");
      httpClient->POST(bytes, count);
      httpClient->end();
      digitalWrite(ledBPin, LOW);
      Serial.println(nodeNumber);  
    }
  }
}

// Task to write samples to our server
void i2sMemsWriterTask(void *param)
{
  Serial.println("i2sMemsWriterTask");  

  size_t samples_count; 
  size_t count;
  int16_t *local_samples = (int16_t *)malloc(sizeof(uint16_t) * SAMPLE_SIZE);

  I2SSampler *sampler = (I2SSampler *)param;
  if (!samples)
  {
    Serial.println("Failed to allocate memory for samples");
    return;
  }

  while (true)
  {
    Serial.println("i2sRead");

    samples_read = sampler->read(local_samples, SAMPLE_SIZE);
    samples_count = samples_read * sizeof(int16_t);
    
    xSemaphoreTake(samplesSemaphore, portMAX_DELAY);
    memcpy(samples, local_samples, samples_count);
    xSemaphoreGive(samplesSemaphore);

    // Notify the sendData task to send the data
    xTaskNotifyGive(sendData1TaskHandle);
  }
}


void setup() {
  // Serial port for debugging purposes
  pinMode(Dip1 , INPUT);
  pinMode(Dip2 , INPUT);
  pinMode(Dip3 , INPUT);

  Serial.begin(115200);

  nodeNumber = 1 * digitalRead(Dip1) + 2 * digitalRead(Dip2) + 4 * digitalRead(Dip3);
  Serial.print("nodeNumber");
  Serial.println(nodeNumber);
  initSPIFFS();

  // Set GPIO 2 as an OUTPUT
  pinMode(ledBPin, OUTPUT);
  pinMode(ledGPin, OUTPUT);
  pinMode(ledRPin, OUTPUT);
  digitalWrite(ledBPin, LOW);
  digitalWrite(ledRPin, LOW);
  digitalWrite(ledGPin, LOW);
  
  // Load values saved in SPIFFS
  ssid = readFile(SPIFFS, ssidPath);
  pass = readFile(SPIFFS, passPath);
  ip = readFile(SPIFFS, ipPath);

  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);

  listAllFiles();

  if(initWiFi()) {
    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/wifimanager.html", "text/html");
    });

    server.on("/nodeid", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi!\nmy Node Id: " + String(nodeNumber));
  });
    
    server.serveStatic("/", SPIFFS, "/");
    
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(SPIFFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(SPIFFS, passPath, pass.c_str());
          }
          // HTTP POST ip value
          if (p->name() == PARAM_INPUT_3) {
            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            // Write file to save value
            writeFile(SPIFFS, ipPath, ip.c_str());
          }
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
      delay(3000);
      ESP.restart();
    });

    AsyncElegantOTA.begin(&server);    // Start ElegantOTA
    server.begin();
    Serial.println("HTTP server started");
    String Mdns = "mic" + String(nodeNumber) + "host";
    while(!MDNS.begin("michost"))
    {
      Serial.println("Starting mDNS...");
      delay(1000);
    }
    Serial.println("MDNS started");
  

    Serial.println("");
    Serial.println("WiFi Connected");
    Serial.println("Started up");
    // indicator LED
    pinMode(2, OUTPUT);
    // setup the HTTP Client
    wifiClientADC = new WiFiClient();
    httpClientADC = new HTTPClient();

    wifiClientI2S = new WiFiClient();
    httpClientI2S = new HTTPClient();

    // Direct i2s input from INMP441 or the SPH0645
    i2sSampler = new I2SMEMSSampler(I2S_NUM_0, i2sPins, i2sMemsConfigLeftChannel, true);
    i2sSampler->start();
    // set up the i2s sample writer task
    TaskHandle_t i2sMemsWriterTaskHandle;
    samplesSemaphore = xSemaphoreCreateMutex();

    if(samplesSemaphore != NULL)
    {
      xTaskCreatePinnedToCore(sendData1, "sendData", 1024 * 4, NULL, 1, &sendData1TaskHandle, 0);
      xTaskCreatePinnedToCore(i2sMemsWriterTask, "i2sMemsWriterTask", 1024 * 4, i2sSampler, 1, NULL, 0);      
    }
  }
  else {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("ESP-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP); 

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/wifimanager.html", "text/html");
    });
    
    server.serveStatic("/", SPIFFS, "/");
    
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(SPIFFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(SPIFFS, passPath, pass.c_str());
          }
          // HTTP POST ip value
          if (p->name() == PARAM_INPUT_3) {
            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            // Write file to save value
            writeFile(SPIFFS, ipPath, ip.c_str());
          }
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
      delay(3000);
      ESP.restart();
    });
    
    server.begin();    
  }
}

void loop() {

}