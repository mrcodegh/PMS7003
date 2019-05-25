//Rev0 - PMS7003 PM2.5,PM10 air quality reporting.  OTA update support.
//       HTTP post packet destination configurable via WiFi PMS7003_AP.
// Credits to these include file/code providers:
#include <FS.h>                   //this needs to be first, or it all crashes and burns...

ADC_MODE(ADC_VCC)
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PMS.h>

// Enable debugging monitoring
//#define DEBUG

#ifdef DEBUG
  #define DBG(message)    Serial.print(message)
  #define DBGL(message)   Serial.println(message)
  #define WRITE_LED
  #define TOGGLE_LED
  #define SET_LED(x)
#else
  #define DBG(message)
  #define DBGL(message)
  #define set_serial(x)
  #define LED_OUTPUT_LEVEL (led_on == true ? LOW : HIGH)
  #define WRITE_LED digitalWrite(LED_BUILTIN, LED_OUTPUT_LEVEL)
  #define TOGGLE_LED (led_on = !led_on)
  #define SET_LED(x) (led_on = x)
  bool led_on = false;
#endif

//json defaults. If there are different values in config.json, they are overwritten.
const int host_max_len = 80;
char host[host_max_len] = {0};
const int url_max_len = 80;
char url[url_max_len] = {0};
char send_fail_str[10] = "0";
bool shouldSaveConfig = false;
int  serial_select = 0; // 0 = DBG GPIO1, GPIO3, 1 = PMS7003, GPIO15, GPIO13

float vdd = 0.0;

const unsigned max_ten_cnt_for_AP_config = 3;  // after this time device idles and waits for pwr reset
const unsigned max_minutes_for_update = 10;
const unsigned int retry_quit_cnt = 4;
const unsigned int retry_delay_sec = 20;
unsigned int send_fail_cnt = 0;
bool send_data_success = false;
bool read_data_success = false;
unsigned long update_timeout = 0;
bool update_in_progress = false;
const uint32_t pms_read_delay_ms = 30000;
const unsigned long sleep_time_in_ms = (10 * 60 * 1000) - pms_read_delay_ms;  // approx poll rate of air sample less read delay
const unsigned int retry_reset_connect_parms_cnt = 24*60*60*1000 / (pms_read_delay_ms + sleep_time_in_ms); // ~one day

WiFiClient client;
const int httpPort = 8080;
const char * AP_pwd = "your_pwd";

enum pms7003_action {
  pms_wakeup,
  pms_getdata,
  pms_sleep,
  pms_ota
};
pms7003_action pms_next_action = pms_wakeup;
unsigned long pms_next_action_ms = 0;

PMS pms(Serial);
PMS::DATA pmsdata;

//callback notifying us of the need to save config
void saveConfigCallback () {
  DBGL("Should save config");
  shouldSaveConfig = true;
}

void sleepCallback() {
#ifdef DEBUG
  DBGL();
  Serial.flush();
#endif
}

void connect(bool clr_connect_info) {
  unsigned wait_cnt;
  const unsigned connect_seconds_max = 20;
  WiFiManager wifiManager;
  int ten_cnt = 0;
    
  if(WiFi.status() != WL_CONNECTED) {
    DBGL("connecting WiFi...");
    //wifi_fpm_do_wakeup();
    WiFi.forceSleepWake();
    delay(100);
    wifi_fpm_close;
    wifi_set_sleep_type(MODEM_SLEEP_T);
    wifi_set_opmode(STATION_MODE);
    wifi_station_connect();

    wait_cnt = 0;
    while(wait_cnt++ < connect_seconds_max && WiFi.status() != WL_CONNECTED) {
      delay(1000);
      DBG(wait_cnt);
    }
    if(WiFi.status() != WL_CONNECTED) {
      DBGL("unable to connect");
      // The extra parameters to be configured (can be either global or just in the setup)
      // After connecting, parameter.getValue() will get you the configured value
      // id/name placeholder/prompt default length
      WiFiManagerParameter custom_host("host", "Host Website ie. site.com", host, host_max_len);
      WiFiManagerParameter custom_url("url", "url", url, url_max_len);
      
      #ifdef DEBUG
        wifiManager.setDebugOutput(true);
      #else
        wifiManager.setDebugOutput(false);
      #endif
    
      //set config save notify callback
      wifiManager.setSaveConfigCallback(saveConfigCallback);
    
      //set static ip
      //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
      
      //add all your parameters here
      wifiManager.addParameter(&custom_host);
      wifiManager.addParameter(&custom_url);
      
      //set minimu quality of signal so it ignores AP's under that quality
      //defaults to 8%
      //wifiManager.setMinimumSignalQuality();
      
      //sets timeout until configuration portal gets turned off
      //useful to make it all retry or go to sleep
      //in seconds
      
      wifiManager.setTimeout(10*60);  // 10 minutes

      //reset settings - for testing
      //wifiManager.resetSettings();
      if (clr_connect_info || strlen(host) == 0 || strlen(url) == 0) {
        WiFi.begin("0","0");
        //WiFi.disconnect(true);
      }

      //fetches ssid and pass and tries to send air quality data
      //if it does not connect it starts an access point with the specified name
      //here  "AutoConnectAP"
      //and goes into a blocking loop awaiting configuration
      while(ten_cnt++ < max_ten_cnt_for_AP_config && WiFi.status() != WL_CONNECTED) {
        //if (wifiManager.autoConnect("PMS7003_AP")) {
        if (wifiManager.autoConnect("PMS7003_AP", AP_pwd)) {
          strcpy(host, custom_host.getValue());
          strcpy(url, custom_url.getValue());
        } 
      }
    }
  }
}

void disconnect() {
  if (WiFi.status() == WL_CONNECTED) {
    DBGL("disconnecting WiFi...");
    wifi_set_opmode(NULL_MODE);
    wifi_fpm_open(); 
    wifi_fpm_set_wakeup_cb(sleepCallback);
    WiFi.forceSleepBegin();
    delay(100);
  }
}

void get_config() {
 //read configuration from FS json
  DBGL("mounting FS...");
  if (SPIFFS.begin()) {
    DBGL("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      DBGL("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        DBGL("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument doc(size + 100);
        DeserializationError error = deserializeJson(doc, buf.get());
        //doc.printTo(Serial);
        if (!error) {
          DBGL("\nparsed json");
          strcpy(host, doc["host"]);
          DBGL(host);
          strcpy(url, doc["url"]);
          DBGL(url);
          strcpy(send_fail_str, doc["send_fail"]);
          DBGL(send_fail_str);
        } else {
          DBGL("failed to load json config");
        }
      }
    }
  } else {
    DBGL("failed to mount FS");
    SPIFFS.format();
    DBGL("SPIFFS fortmatted !");
  }
  send_fail_cnt = atoi(send_fail_str);
  DBG("Send Fail Count ");
  DBGL(send_fail_cnt);
}

void get_vdd() {
  vdd = (float)ESP.getVcc() / 1000.0;
}

void save_config() {
  DBGL("saving config");
  DynamicJsonDocument doc(1024);
  doc["host"] = host;
  doc["url"] = url;
  itoa(send_fail_cnt, send_fail_str, 10);
  doc["send_fail"] = send_fail_str;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    DBGL("failed to open config file for writing");
  }

  //doc.printTo(Serial);
  serializeJson(doc, configFile);
  configFile.close();
  shouldSaveConfig = false;
}

bool send_data() {
  bool success = false;

  String senddata = "SP_1_0=" + String(pmsdata.PM_SP_UG_1_0) + "&SP_2_5=" + String(pmsdata.PM_SP_UG_2_5) + "&SP_10_0=" + String(pmsdata.PM_SP_UG_10_0) +
                    "&AE_1_0=" + String(pmsdata.PM_AE_UG_1_0) + "&AE_2_5=" + String(pmsdata.PM_AE_UG_2_5) + "&AE_10_0=" + String(pmsdata.PM_AE_UG_10_0) +
                    "&batt=" + String(vdd) + "&fail=" + String(send_fail_cnt);

  if (WiFi.status() != WL_CONNECTED) {
    connect(false); // Connect, no reset on fail
  }

  if (WiFi.status() == WL_CONNECTED) {
    DBG("Requesting URL: ");
    DBGL(url);
    DBGL(senddata);
  
    if (client.connect(host, httpPort)) {
      client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                   "Host: " + host + "\r\n" + 
                   "Content-Type: application/x-www-form-urlencoded\r\n" + 
                   "Content-Length: " + senddata.length() + "\r\n\r\n" +
                   senddata  + "\r\n");
      while (client.connected()) {
        if (client.available()) {
          String line = client.readStringUntil('\n');
          DBGL(line);
          if (line.startsWith("success")) {
            success = true;
          } else if (line.startsWith("update")) {
            success = true;
            pms_next_action = pms_ota;
            pms_next_action_ms = millis();
            update_timeout = pms_next_action_ms + max_minutes_for_update*60*1000;
          }
        }
      }
      client.stop();
    }
  }
  return success;
}

#ifdef DEBUG
void set_serial(int sel_pms7003) {
  if (serial_select != sel_pms7003) {
    Serial.flush();
    while (Serial.available()) { Serial.read(); }
    // esp8266 lib 2.5 Serial.swap is broke - gotta do this
    //Serial.end();
    if (sel_pms7003) {
      //Serial.begin(PMS::BAUD_RATE);
      Serial.swap(15);
    } else {
      //Serial.begin(115200);
      Serial.swap(1);
    }
    //Serial.swap();
    //pinMode(15, OUTPUT);
    serial_select = sel_pms7003;
  }
}
#endif

void setup() {

  #ifdef DEBUG
    // esp8266 lib 2.5 Serial.swap is broke - gotta have same baud as pms7003
    Serial.begin(PMS::BAUD_RATE);
    //Serial.begin(115200); 
    delay(10000); // for USB to instantiate
    serial_select = 0;
  #else
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LED_OUTPUT_LEVEL);
    // GPIO15, GPIO13 (TX/RX pin on ESP-12F)
    Serial.begin(PMS::BAUD_RATE);
    Serial.swap(15); // switch pins to PMS7003 for full time use - Serial not shared for debug
    serial_select = 1; 
  #endif

  ArduinoOTA.begin();

  ArduinoOTA.onStart([]() {
    DBGL("OTA onstart");
    update_in_progress = true;
    pms_next_action = pms_ota;
    pms_next_action_ms = millis();
    update_timeout = pms_next_action_ms + max_minutes_for_update*60*1000;
    set_serial(0);
  });

  // Set modem sleep for accurate ADC reads
  disconnect();
  get_vdd();
  get_config();
}

void loop() {
  if (millis() > pms_next_action_ms) {
    switch (pms_next_action) {
      case pms_wakeup:
        DBGL("pms_wakeup");
        delay(2000);
        set_serial(1);
        pms.passiveMode();
        delay(1000); // seemed to need this for fan start?
        pms.wakeUp();
        set_serial(0);
        pms_next_action = pms_getdata;
        pms_next_action_ms = millis() + pms_read_delay_ms;
        break;

      case pms_getdata:
        DBGL("pms_getdata");
        set_serial(1);
        pms.requestRead();
        delay(1000);
        read_data_success = pms.readUntil(pmsdata);
        set_serial(0);
        if (read_data_success) {
          pms_next_action = pms_sleep;
          pms_next_action_ms = millis() + 1000;            
        } else {
          DBGL("failed to read data from PMS7003");
          //Wait for potential OTA
          pms_next_action = pms_ota;
          pms_next_action_ms = millis();
          update_timeout = pms_next_action_ms + max_minutes_for_update*60*1000;
          connect(false);
        }
        break;
        
      case pms_sleep:
        send_data_success = send_data(); // this blocks waiting for connect and send
        if (send_data_success == false) {
          send_fail_cnt++;
          if (send_fail_cnt > retry_reset_connect_parms_cnt) {
            send_fail_cnt = 1;
            connect(true);
          }
          save_config();
          if (send_fail_cnt % retry_quit_cnt == 0) {
            set_serial(1);
            pms.sleep();
            delay(2000);
            disconnect();
            ESP.deepSleep(sleep_time_in_ms * 1000);
            // code path ends here
          } else {
            pms_next_action_ms = millis() + retry_delay_sec * 1000;
          }
        } else { // this is also pms_ota path after successful send
          if (send_fail_cnt != 0) {
            send_fail_cnt = 0;
            shouldSaveConfig = true;
          }
          if (shouldSaveConfig) {
            save_config();
          }
          set_serial(1);
          pms.sleep();
          delay(2000);
          set_serial(0);
          if (pms_next_action != pms_ota) {
            disconnect();
            ESP.deepSleep(sleep_time_in_ms * 1000);
            // no code path here - resets after timeout
          }
        }
        break;

      case pms_ota:
        if( !update_in_progress && millis() > update_timeout ) {
          disconnect();
          ESP.deepSleep(sleep_time_in_ms * 1000);
        }
        break;
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
  }
}






