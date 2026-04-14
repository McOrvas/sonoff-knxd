/*
 * Notwendige Einstellungen in der Arduino IDE
 * 
 * Boardverwalter-URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json
 * 
 * Sonoff S20 / Nous A1T
 * Board:      Generic ESP8266 Module
 * Flash Size: 1MB (FS:64KB OTA:~470KB) [Notwendig für Updates über die Weboberfläche]
 * 
 * Sonoff 4CH
 * Board:      Generic ESP8285 Module
 * Flash Size: 1MB (FS:64KB OTA:~470KB) [Notwendig für Updates über die Weboberfläche]
 * 
 * CO2 traffic light with Sensirion SCD30 air quality sensors
 * Board:      Adafruit Feather HUZZAH ESP8266
 * Flash Size: 4MB (FS:2MB OTA:~1019KB) [Notwendig für Updates über die Weboberfläche]
 */

/*
 * *******************************
 * *** Laden der Konfiguration ***
 * *******************************
 */

#include "Configuration.h"

/*
 * *************************
 * *** Include libraries ***
 * *************************
 */

#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include "eibtypes.h" // https://github.com/knxd/knxd/blob/master/src/include/eibtypes.h

/* 
 * ***********************************************************************************************
 * *** Optional components:                                                                    ***
 * *** Sensirion SCD30 air quality sensor (https://sensirion.com/products/catalog/SCD30)       ***
 * *** Seeed Studio Grove LCD             (https://wiki.seeedstudio.com/Grove-16x2_LCD_Series) *** 
 * *** ntfy for push notifications        (https://ntfy.sh)                                    ***
 * ***********************************************************************************************
 */

#if SCD30_ENABLE == true
   #include <Wire.h>                           // https://www.arduino.cc/en/reference/wire
   #include <SparkFun_SCD30_Arduino_Library.h> // https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library
   SCD30 airSensorSCD30;
      #if LCD_ENABLE == true
         #include <rgb_lcd.h>                  // https://github.com/Seeed-Studio/Grove_LCD_RGB_Backlight
         rgb_lcd lcd;
   #endif
#endif

#if NTFY_ENABLE == true
   #include "NtfyClient.h"
   NtfyClient ntfy(NTFY_IP, NTFY_PORT, NTFY_TOPIC);
#endif

/* 
 * *************************
 * *** Interne Variablen *** 
 * *************************
 */

static const char *SOFTWARE_VERSION             = "2026-04-14";

const uint8_t  GA_SWITCH_COUNT                  = sizeof(GA_SWITCH[0]) / 3,
               GA_LOCK_COUNT                    = sizeof(GA_LOCK[0]) / 3,
               KNXD_GROUP_CONNECTION_REQUEST[]  = {0x00, 0x05, EIB_OPEN_GROUPCON >> 8, EIB_OPEN_GROUPCON & 0xFF, 0x00, 0x00, 0x00};

const bool     GA_DATE_VALID = GA_DATE[0] + GA_DATE[1] + GA_DATE[2] > 0,
               GA_TIME_VALID = GA_TIME[0] + GA_TIME[1] + GA_TIME[2] > 0;

enum class WifiState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    NetworkReady
};

WifiState wifiState = WifiState::Disconnected;
               
uint8_t        messageLength = 0,
               messageResponse[32],
               timeWeekday   = 0,
               timeHours     = 0,
               timeMinutes   = 0,
               timeSeconds   = 0,
               dateDay       = 0,
               dateMonth     = 0;
uint16_t       dateYear      = 0;
time_t         bootTime      = 0;

bool           knxdConnectionInitiated          = false,
               knxdConnectionConfirmed          = false,
               lockActive[]                     = {false, false, false, false},
               buttonLastState[]                = {true, true, true, true},
               buttonDebouncedState[]           = {true, true, true, true},
               autoOffTimerActive[]             = {false, false, false, false},
               autoUnlockTimerActive[]          = {false, false, false, false},
               relayStatus[]                    = {false, false, false, false},
               ledBlinkStatus                   = false,
               timeValid                        = false,
               dateValid                        = false,
               missingTelegramTimeoutEnabled    = false,
               incompleteTelegramTimeoutEnabled = false,
               airSensorSCD30Connected          = false,
               airSensorSCD30Stuck              = false;

uint32_t       knxdConnectionInitiatedCount     = 0,
               knxdConnectionFailedCount        = 0,
               knxdConnectionHandshakeTimeouts  = 0,
               knxdConnectionConfirmedCount     = 0,
               receivedTelegrams                = 0,
               missingTelegramTimeouts          = 0,
               incompleteTelegramTimeouts       = 0,
               wifiDisconnections               = 0,
               wifiCurrentConnectionAttempts    = 0,
               knxdDisconnections               = 0,
               loopCounter                      = 0,
               
// Variablen zur Zeitmessung
               currentMillis                    = 0,
               currentMillisTemp                = 0,
               millisOverflows                  = 0,
               loopCounterMillis                = 0,
               buttonDebounceMillis[]           = {0, 0, 0, 0},
               autoOffTimerStartMillis[]        = {0, 0, 0, 0},
               autoUnlockTimerStartMillis[]     = {0, 0, 0, 0},
               knxdConnectionInitiatedMillis    = 0,
               knxdConnectionFailedMillis       = CONNECTION_LOST_DELAY_S * 1000,
               ledBlinkLastSwitch               = 0,
               lastTelegramReceivedMillis       = 0,
               lastTelegramHeaderReceivedMillis = 0,
               timeTelegramReceivedMillis       = 0,
               dateTelegramReceivedMillis       = 0,
               wifiConnectionInitiatedMillis    = 0,
               wifiDisconnectedMillis           = WIFI_CONNECTION_LOST_DELAY_S * 1000,
               lastAirMeasurementReceivedMillis = 0,
               lastAirSensorPolledMillis        = 0,
               airCO2LastSentMillis             = 0,
               airTemperatureLastSentMillis     = 0,
               airHumidityLastSentMillis        = 0;

// Variables for air quality
float          airTemperature         = 0.0,
               airTemperatureLastSent = 0.0,
               airHumidity            = 0.0,
               airHumidityLastSent    = 0.0;
uint16_t       airCO2                 = 0,
               airCO2LastSent         = 0;

// Variables for ntfy
bool           ntfyInitialMessageSent = false;

// WLAN-Client
WiFiClient              knxdClient;
IPAddress               lastIPAddress(0, 0, 0, 0);
WiFiEventHandler        connectHandler,
                        disconnectHandler,
                        gotIpHandler,
                        dhcpTimeoutHandler;

// Webserver
ESP8266WebServer        webServer(80);
ESP8266HTTPUpdateServer httpUpdateServer;

/*
 * *************************************
 * *** Ring buffer for event logging ***
 * *************************************
 */

const uint32_t  LOG_SIZE             = 100;
uint32_t        connectionLogEntries = 0;
uint32_t        switchLogEntries     = 0;

enum class ConnectionLogEvent : uint8_t {
   WLAN_CONNECTION_INITIATED,
   WLAN_CONNECTED,
   WLAN_DHCP_COMPLETED,
   WLAN_DISCONNECTED,
   WLAN_CONNECTION_TIMEOUT,
   WLAN_DHCP_TIMEOUT,
   KNXD_CONNECTION_INITIATED,
   KNXD_CONNECTION_HANDSHAKE_TIMEOUT,
   KNXD_CONNECTION_CONFIRMED,
   KNXD_DISCONNECTED,
   MISSING_TELEGRAM_TIMEOUT,
   INCOMPLETE_TELEGRAM_TIMEOUT   
};

struct ConnectionLogEntry {   
   uint32_t           entry,
                      uptimeSeconds;
   time_t             timestamp;   
   int32_t            wlanChannel;
   uint8_t            wlanBssid[6],
                      wiFiDisconnectReason;
   ConnectionLogEvent event;
   bool               timestampValid;
} connectionLogRingbuffer[LOG_SIZE];

enum class SwitchLogEvent : uint8_t {
   OFF,
   ON,
   LOCK,
   UNLOCK
};

enum class SwitchLogSource : uint8_t {
   GROUP_ADDRESS,
   BUTTON,
   ON_BY_LOCK,
   ON_BY_UNLOCK,
   OFF_BY_LOCK,
   OFF_BY_UNLOCK,
   AUTO_OFF_TIMER,
   AUTO_UNLOCK_TIMER,
   WEBSERVER
};

struct SwitchLogEntry {
   const uint8_t   *ga;
   uint32_t        entry,
                   uptimeSeconds;
   time_t          timestamp;
   uint8_t         channel;   
   SwitchLogEvent  event;
   SwitchLogSource source;
   bool            timestampValid;
} switchLogRingbuffer[LOG_SIZE];


void setup() {
   Serial.begin(115200);
   delay(10);
   
   pinMode(GPIO_LED, OUTPUT);
   digitalWrite(GPIO_LED, !relayStatus[0]);
   
   Serial.print("\n\n");
   Serial.print(HOST_NAME);
   Serial.print(" (");
   Serial.print(HOST_DESCRIPTION);
   Serial.println(")");
   Serial.print("Software-Version: ");
   Serial.print(SOFTWARE_VERSION);
   Serial.println("\n");
   
   for (uint8_t ch=0; ch<CHANNELS; ch++){
      pinMode(GPIO_BUTTON[ch], INPUT_PULLUP);
      pinMode(GPIO_RELAY[ch], OUTPUT);      
      
      if (RELAY_NORMALLY_OPEN[ch])
         digitalWrite(GPIO_RELAY[ch], relayStatus[ch]);
      else
         digitalWrite(GPIO_RELAY[ch], !relayStatus[ch]);
      
      Serial.print("Kanal ");
      Serial.print(ch + 1);
      Serial.print(" GAs schalten: ");
      for (uint8_t i=0; i<GA_SWITCH_COUNT; i++){
         if (GA_SWITCH[ch][i][0] + GA_SWITCH[ch][i][1] + GA_SWITCH[ch][i][2] > 0) {
            Serial.print(GA_SWITCH[ch][i][0]);
            Serial.print("/");
            Serial.print(GA_SWITCH[ch][i][1]);
            Serial.print("/");
            Serial.print(GA_SWITCH[ch][i][2]);
            Serial.print(" ");
         }
      }
      Serial.println();
      
      Serial.print("Kanal ");
      Serial.print(ch + 1);
      Serial.print(" GAs sperren:  ");
      for (uint8_t i=0; i<GA_LOCK_COUNT; i++){
         if (GA_LOCK[ch][i][0] + GA_LOCK[ch][i][1] + GA_LOCK[ch][i][2] > 0) {
            Serial.print(GA_LOCK[ch][0][0]);
            Serial.print("/");
            Serial.print(GA_LOCK[ch][0][1]);
            Serial.print("/");
            Serial.print(GA_LOCK[ch][0][2]);
            Serial.print(" ");
         }
      }
      Serial.println();

      Serial.print("Kanal ");
      Serial.print(ch + 1);
      Serial.print(" GA  Status:   ");
      if (GA_STATUS[ch][0] + GA_STATUS[ch][1] + GA_STATUS[ch][2] > 0) {
         Serial.print(GA_STATUS[ch][0]);
         Serial.print("/");
         Serial.print(GA_STATUS[ch][1]);
         Serial.print("/");
         Serial.print(GA_STATUS[ch][2]);
      }
      Serial.println();
   }
      
   Serial.print("GA Zeit:              ");
   Serial.print(GA_TIME[0]);
   Serial.print("/");
   Serial.print(GA_TIME[1]);
   Serial.print("/");
   Serial.println(GA_TIME[2]);
   Serial.print("GA Datum:             ");
   Serial.print(GA_DATE[0]);
   Serial.print("/");
   Serial.print(GA_DATE[1]);
   Serial.print("/");
   Serial.println(GA_DATE[2]);

   WiFi.disconnect();
   WiFi.mode(WIFI_STA);
   WiFi.hostname(HOST_NAME);
   WiFi.persistent(false);
   WiFi.setAutoReconnect(false);

   setupWebServer();
      
   connectHandler     = WiFi.onStationModeConnected(onWifiConnected);
   disconnectHandler  = WiFi.onStationModeDisconnected(onWifiDisconnect);
   gotIpHandler       = WiFi.onStationModeGotIP(onWifiGotIP);
   dhcpTimeoutHandler = WiFi.onStationModeDHCPTimeout(onWifiDhcpTimeout);
   
   #if SCD30_ENABLE == true
      // Initialize I2C bus
      Wire.begin();
      Wire.setClock(100000L);             // 100 kHz SCD30 (standard value)
      Wire.setClockStretchLimit(200000L); // CO2-SCD30 

      if (Wire.status() != I2C_OK)
         Serial.println("Something wrong with I2C");
      
      // Initialize SCD30 environment sensor
      airSensorSCD30Connected = airSensorSCD30.begin();
      if (!airSensorSCD30Connected) 
         Serial.println("The SCD30 did not respond. Please check wiring."); 

      // Sensirion no auto calibration
      airSensorSCD30.setAutoSelfCalibration(false);
      airSensorSCD30.setTemperatureOffset(AIR_SENSOR_TEMPERATURE_OFFSET_K);
      airSensorSCD30.setAltitudeCompensation(AIR_SENSOR_ALTITUDE_COMPENSATION_M);

      // Measure air every AIR_SENSOR_MEASUREMENT_INTERVAL_S seconds
      airSensorSCD30.setMeasurementInterval(AIR_SENSOR_MEASUREMENT_INTERVAL_S);
      
      #if LCD_ENABLE == true
         // Initialize LCD with 16 columns and 2 rows
         lcd.begin(16, 2);
         lcd.setCursor(0,0);
         lcd.print("Initialization");
      #endif   
   #endif

   #if NTFY_ENABLE == true
      ntfy.begin();
   #endif
}


void onWifiConnected(const WiFiEventStationModeConnected& event) {
   wifiState = WifiState::Connected;
   logConnectionEvent(ConnectionLogEvent::WLAN_CONNECTED);
   
   uint8_t *bssid = WiFi.BSSID();
   char buffer[18];
   snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);

   Serial.print("WLAN-Verbindung hergestellt mit ");
   Serial.print(buffer);
   Serial.print(" auf Kanal ");
   Serial.println(WiFi.channel());
}


void onWifiGotIP(const WiFiEventStationModeGotIP& event) {
   wifiState = WifiState::NetworkReady;
   wifiCurrentConnectionAttempts = 0;
   
   IPAddress ip = WiFi.localIP();
   logConnectionEvent(ConnectionLogEvent::WLAN_DHCP_COMPLETED);
   Serial.print("IP-Adresse: ");
   Serial.println(ip);
   
   if (!ntfyInitialMessageSent){
      ntfyInitialMessageSent = true;
      lastIPAddress = ip;
      char buffer[64];
      snprintf(buffer, sizeof(buffer), "Modul gestartet, IP-Adresse: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
      ntfySendMessage(buffer);
   }
   else if (ip != lastIPAddress){
      lastIPAddress = ip;
      char buffer[64];
      snprintf(buffer, sizeof(buffer), "Neue IP-Adresse: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
      ntfySendMessage(buffer);
   }
   

   // Disable the Access Point again
   if (WiFi.getMode() == WIFI_AP_STA || WiFi.getMode() == WIFI_AP) {
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      Serial.println("Deaktiviere den Access Point und setze den WLAN-Modus zurück auf WIFI_STA.");
   }
}


void onWifiDhcpTimeout() {
   wifiState = WifiState::Disconnected;
   wifiDisconnectedMillis = currentMillis;
   WiFi.disconnect();
   logConnectionEvent(ConnectionLogEvent::WLAN_DHCP_TIMEOUT);
   Serial.println(getConnectionLogEventString(ConnectionLogEvent::WLAN_DHCP_TIMEOUT));
}


void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
   wifiState = WifiState::Disconnected;
   wifiDisconnectedMillis = currentMillis;  
   wifiDisconnections++;
   resetKnxdConnection();
   logConnectionEvent(ConnectionLogEvent::WLAN_DISCONNECTED, event.reason); 
   Serial.print(getConnectionLogEventString(ConnectionLogEvent::WLAN_DISCONNECTED));
   Serial.print(", Grund: ");
   Serial.print(getWiFiDisconnectReasonString(event.reason));
   Serial.print(" (");
   Serial.print(wifiCurrentConnectionAttempts);
   Serial.println(". Versuch)");

   // After 3 attempts, open a fallback access point to enable flashing.
   if (wifiCurrentConnectionAttempts == 3) {
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP("sonoff-knxd", "winter07");
      IPAddress IP = WiFi.softAPIP();
      Serial.print("Verbindung zum WLAN nicht möglich, öffne Access Point zum Flashen. SSID 'sonoff-knxd', Passwort 'winter07', IP-Adresse: ");
      Serial.println(IP);
   }
}

uint32_t getLoopsPerSecond() {
   uint32_t deltaMillis = currentMillis - loopCounterMillis;

   if (deltaMillis == 0)
      return 0;

   uint32_t loopsPerSecond = ((uint64_t)loopCounter * 1000ULL) / deltaMillis;

   loopCounter = 0;
   loopCounterMillis = currentMillis;

   return loopsPerSecond;
}

void loop() {
   currentMillisTemp = millis();
   loopCounter++;

   // loopCounter overflow
   if (loopCounter == 0)
      loopCounterMillis = currentMillisTemp;
   
   // Überlauf von millis()
   if (currentMillis > currentMillisTemp) {
      millisOverflows++;
   }
   currentMillis = currentMillisTemp;

   for (uint8_t ch=0; ch<CHANNELS; ch++) {
      // Check hardware buttons
      checkButton(ch);

      // Check if a channel has to be switched off by a timer
      if (relayStatus[ch] && autoOffTimerActive[ch] && (currentMillis - autoOffTimerStartMillis[ch]) >= (AUTO_OFF_TIMER_S[ch] * 1000)){
         Serial.print("Auto off timer for channel ");
         Serial.print(ch + 1);
         Serial.println(" has expired!");
         // Set to false even if the channel cannot be switched off due to an active lock.
         autoOffTimerActive[ch] = false;
         switchRelay(ch, false, AUTO_OFF_TIMER_OVERRIDES_LOCK, SwitchLogSource::AUTO_OFF_TIMER, 0);
      }

      // Check if a channel has to be unlocked by a timer
      if (lockActive[ch] && autoUnlockTimerActive[ch] && (currentMillis - autoUnlockTimerStartMillis[ch]) >= (AUTO_UNLOCK_TIMER_S[ch] * 1000)){
         Serial.print("Auto unlock timer for channel ");
         Serial.print(ch + 1);
         Serial.println(" has expired!");
         autoUnlockTimerActive[ch] = false;
         lockRelay(ch, false, SwitchLogSource::AUTO_UNLOCK_TIMER, 0);
      }
   }
   
   if (WiFi.status() != WL_CONNECTED || wifiState != WifiState::NetworkReady) {              
      // Wifi connection not yet initialized or delay has expired
      if (wifiState == WifiState::Disconnected && (currentMillis - wifiDisconnectedMillis) >= WIFI_CONNECTION_LOST_DELAY_S * 1000) {
         wifiState = WifiState::Connecting;
         wifiConnectionInitiatedMillis = currentMillis;
         wifiCurrentConnectionAttempts++;
         WiFi.begin(SSID, PASSWORD);
         logConnectionEvent(ConnectionLogEvent::WLAN_CONNECTION_INITIATED);
         Serial.print("Verbinde mit WLAN '");
         Serial.print(SSID);
         Serial.println("'");   
      }
      
      // The connection is already initialized
      else if (wifiState != WifiState::Disconnected && (currentMillis - wifiConnectionInitiatedMillis) >= WIFI_CONNECTION_TIMEOUT_S * 1000) {
         wifiState = WifiState::Disconnected;
         wifiDisconnectedMillis = currentMillis;
         WiFi.disconnect();
         logConnectionEvent(ConnectionLogEvent::WLAN_CONNECTION_TIMEOUT);
         Serial.println(getConnectionLogEventString(ConnectionLogEvent::WLAN_CONNECTION_TIMEOUT));
      }
   }
   
   // WLAN connected and IP address received
   else if (wifiState == WifiState::NetworkReady) {
      // KNX-Kommunikation
      knxLoop();
      
      // Falls eine Verbindung zum EIBD/KNXD aufgebaut ist, blinkt die LED sofern gewünscht.
      if (LED_BLINKS_WHEN_CONNECTED)
         ledBlink();
   }

   // Always run the web server, regardless of the WLAN mode. Necessary for the fallback access point.
   webServer.handleClient();
   
   #if SCD30_ENABLE == true
      if ((currentMillis - lastAirSensorPolledMillis) >= AIR_SENSOR_POLL_INTERVAL_S * 1000)
         measureAir();
   #endif

   #if NTFY_ENABLE == true
      ntfy.loop();
   #endif
}


void knxLoop(){
   // Keine Verbindung zum knxd
   if (!knxdConnectionInitiated){
      // Pause zwischen zwei Verbindungsversuchen abgelaufen
      if ((currentMillis - knxdConnectionFailedMillis) >= CONNECTION_LOST_DELAY_S * 1000){
         connectToKnxd();
      }
   }
   
   // Die Verbindung zum knxd wurde unterbrochen
   else if (!knxdClient.connected()){
      Serial.println("Die Verbindung zum knxd wurde getrennt.");      
      logConnectionEvent(ConnectionLogEvent::KNXD_DISCONNECTED);
      knxdDisconnections++;
      resetKnxdConnection();
   }
   
   // Der Verbindungsaufbau zum knxd wurde nicht rechtzeitig bestätigt
   else if (!knxdConnectionConfirmed && (currentMillis - knxdConnectionInitiatedMillis) >= CONNECTION_CONFIRMATION_TIMEOUT_MS){
      Serial.print("Der Verbindungsaufbau zum knxd wurde nicht innerhalb von ");
      Serial.print(CONNECTION_CONFIRMATION_TIMEOUT_MS);
      Serial.println(" ms bestätigt.");
      logConnectionEvent(ConnectionLogEvent::KNXD_CONNECTION_HANDSHAKE_TIMEOUT);
      knxdConnectionHandshakeTimeouts++;
      resetKnxdConnection();
   }
   
   // Die Verbindung steht prinzipiell, aber seit MISSING_TELEGRAM_TIMEOUT_MIN wurde kein Telegramm mehr empfangen und deshalb wird ein Timeout ausgelöst
   else if (missingTelegramTimeoutEnabled && MISSING_TELEGRAM_TIMEOUT_MIN > 0 && (currentMillis - lastTelegramReceivedMillis) >= MISSING_TELEGRAM_TIMEOUT_MIN * 60000){
      Serial.print("Timeout: No telegram received during ");
      Serial.print(MISSING_TELEGRAM_TIMEOUT_MIN);
      Serial.println(" minutes");
      logConnectionEvent(ConnectionLogEvent::MISSING_TELEGRAM_TIMEOUT);
      missingTelegramTimeouts++;
      resetKnxdConnection();
   }
   
   // Die Verbindung ist etabliert
   else {
      // Die Länge einer neuen Nachricht lesen
      if (messageLength == 0 && knxdClient.available() >= 2){
         messageLength = (((int) knxdClient.read()) << 8) + knxdClient.read();
         lastTelegramHeaderReceivedMillis = currentMillis;
         incompleteTelegramTimeoutEnabled = true;
      }
      
      // Die Nutzdaten einer Nachricht lesen
      if (messageLength > 0 && knxdClient.available() >= messageLength){
         knxdClient.read(messageResponse, messageLength);
         
         // Prüfen, ob der initiale Verbindungsaufbau korrekt bestätigt wurde      
         if (!knxdConnectionConfirmed && messageLength == 2 && messageResponse[0] == KNXD_GROUP_CONNECTION_REQUEST[2] && messageResponse[1] == KNXD_GROUP_CONNECTION_REQUEST[3]){
            Serial.println("EIBD/KNXD Verbindung hergestellt");            
            logConnectionEvent(ConnectionLogEvent::KNXD_CONNECTION_CONFIRMED);
            knxdConnectionConfirmed = true;   
            knxdConnectionConfirmedCount++;
            
            // Auch wenn bisher kein echtes Telegramm vom Bus kam, die Variablen hier bereits entsprechend setzen,
            // da ansonsten ein dauerhaft toter Bus nicht über den Timeout erkannt würde.
            lastTelegramReceivedMillis = currentMillis;
            missingTelegramTimeoutEnabled = true;
            
            // Die Status-GA senden, sobald die Verbindung steht. Dadurch wird sie beim ersten Start nach einem Spannungsausfall gesendet.
            for (uint8_t ch = 0; ch < CHANNELS; ch++)
               writeGA(GA_STATUS[ch], relayStatus[ch]);
            
            if (REQUEST_DATE_AND_TIME_INITIALLY){
               if (GA_DATE_VALID && !dateValid)
                  readGA(GA_DATE);
               if (GA_TIME_VALID && !timeValid)
                  readGA(GA_TIME);               
            }
         }
         
         if (     messageLength >= 8
               && messageResponse[0] == EIB_GROUP_PACKET >> 8
               && messageResponse[1] == EIB_GROUP_PACKET & 0xFF){
            
            receivedTelegrams++;  
            lastTelegramReceivedMillis = currentMillis;
            missingTelegramTimeoutEnabled = true;
            
//             Serial.printf("Received group packet telegram (#%d): ", receivedTelegrams);
//             for (uint8_t i=0; i<messageLength; i++){
//                Serial.printf("%02X", messageResponse[i]);
//                Serial.print(" ");
//             }
//             Serial.printf("(%d.%d.%d -> ", messageResponse[2] >> 4, messageResponse[2] & 0xF, messageResponse[3]);
//             Serial.printf("%d/%d/%d, ", messageResponse[4] >> 3, messageResponse[4] & 0x7, messageResponse[5]);
//             if (messageResponse[6] == 0x00 && (messageResponse[7] & 0x80))
//                Serial.print("GroupValueWrite");
//             else if (messageResponse[6] == 0x00 && (messageResponse[7] & 0x40))
//                Serial.print("GroupValueResponse");
//             else if (messageResponse[6] == 0x00 && messageResponse[7] == 0x00)
//                Serial.print("GroupValueRead");
//             Serial.println(")");
            
            // Schalten oder sperren
            if (      messageLength == 8
                  &&  messageResponse[6] == 0x00
                  && (messageResponse[7] & 0x80) == 0x80){
               
               // Die übertragene Gruppenadresse mit denen aller Kanäle vergleichen
               for (uint8_t ch=0; ch<CHANNELS; ch++){
                  for (uint8_t i=0; i<GA_SWITCH_COUNT; i++){
                     // Schalten
                     if (GA_SWITCH[ch][i][0] + GA_SWITCH[ch][i][1] + GA_SWITCH[ch][i][2] > 0
                         && messageResponse[4] == (GA_SWITCH[ch][i][0] << 3) + GA_SWITCH[ch][i][1]
                         && messageResponse[5] == GA_SWITCH[ch][i][2]){
                         switchRelay(ch, messageResponse[7] & 0x0F, false, SwitchLogSource::GROUP_ADDRESS, &GA_SWITCH[ch][i][0]);
                     }
                  }
                  for (uint8_t i=0; i<GA_LOCK_COUNT; i++){
                     // Sperren
                     if (GA_LOCK[ch][i][0] + GA_LOCK[ch][i][1] + GA_LOCK[ch][i][2] > 0
                         && messageResponse[4] == (GA_LOCK[ch][i][0] << 3) + GA_LOCK[ch][i][1]
                         && messageResponse[5] == GA_LOCK[ch][i][2]){
                         lockRelay(ch, (messageResponse[7] & 0x0F) ^ LOCK_INVERTED[ch], SwitchLogSource::GROUP_ADDRESS, &GA_LOCK[ch][i][0]);
                     }
                  }
               }
            }
            
            // Status lesen
            else if (messageLength == 8
                  && messageResponse[6] == 0x00
                  && messageResponse[7] == 0x00){
               
               // Die übertragene Gruppenadresse denen aller Kanäle vergleichen
               for (uint8_t ch=0; ch<CHANNELS; ch++){
                  if (GA_STATUS[ch][0] + GA_STATUS[ch][1] + GA_STATUS[ch][2] > 0
                      && messageResponse[4] == (GA_STATUS[ch][0] << 3) + GA_STATUS[ch][1] && messageResponse[5] == GA_STATUS[ch][2]) {
                     Serial.print("Leseanforderung Status Kanal ");
                     Serial.println(ch + 1);
                     
                     responseGA(GA_STATUS[ch], relayStatus[ch]);
                  }
               }
               
               #if SCD30_ENABLE == true
                  // Read GA CO2
                  if (GA_AIR_CO2[0] + GA_AIR_CO2[1] + GA_AIR_CO2[2] > 0
                     && messageResponse[4] == (GA_AIR_CO2[0] << 3) + GA_AIR_CO2[1] && messageResponse[5] == GA_AIR_CO2[2]) {
                     responseGA(GA_AIR_CO2, encodeDpt9Int(airCO2));
                  }
                  
                  // Read GA Temperature
                  if (GA_AIR_TEMPERATURE[0] + GA_AIR_TEMPERATURE[1] + GA_AIR_TEMPERATURE[2] > 0
                     && messageResponse[4] == (GA_AIR_TEMPERATURE[0] << 3) + GA_AIR_TEMPERATURE[1] && messageResponse[5] == GA_AIR_TEMPERATURE[2]) {
                     responseGA(GA_AIR_TEMPERATURE, encodeDpt9Float(airTemperature));
                  }
                  
                  // Read GA Humidity
                  if (GA_AIR_HUMIDITY[0] + GA_AIR_HUMIDITY[1] + GA_AIR_HUMIDITY[2] > 0
                     && messageResponse[4] == (GA_AIR_HUMIDITY[0] << 3) + GA_AIR_HUMIDITY[1] && messageResponse[5] == GA_AIR_HUMIDITY[2]) {
                     responseGA(GA_AIR_HUMIDITY, encodeDpt9Float(airHumidity));
                  }
               #endif
            }
            
            // Time or Date received
            else if (messageLength == 11
                  && messageResponse[6] == 0x00
                  && (messageResponse[7] & 0xC0)){ // Accept GroupValueWrite (0x80) and GroupValueResponse (0x40)
               
               // Date telegram
               if (GA_DATE_VALID && messageResponse[4] == (GA_DATE[0] << 3) + GA_DATE[1] && messageResponse[5] == GA_DATE[2]) {
                  
                  dateDay   = messageResponse[8] & 0x1F;
                  dateMonth = messageResponse[9] & 0x0F;
                  dateYear  = (messageResponse[10] & 0x7F);
                  dateYear  = dateYear >= 90 ? 1900 + dateYear : 2000 + dateYear;                  
                  dateTelegramReceivedMillis = currentMillis;
                  
                  Serial.printf("Date telegram received: %04d-%02d-%02d\n", dateYear, dateMonth, dateDay);
                  
                  Serial.printf("Old date and time: %04d-%02d-%02d %02d:%02d:%02d (%1d)\n", year(), month(), day(), hour(), minute(), second(), timeStatus());
                  setTime(hour(), minute(), second(), dateDay, dateMonth, dateYear);                  
                  Serial.printf("New date and time: %04d-%02d-%02d %02d:%02d:%02d (%1d)\n", year(), month(), day(), hour(), minute(), second(), timeStatus());
                  
                  if (!dateValid || !timeValid){
                     bootTime = now() - getUptimeSeconds();
                  }
                  
                  dateValid = true;
               }
               
               // Time telegram
               if (GA_TIME_VALID && messageResponse[4] == (GA_TIME[0] << 3) + GA_TIME[1] && messageResponse[5] == GA_TIME[2]) {
                  
                  timeWeekday = messageResponse[8] >> 5;
                  timeHours   = messageResponse[8] & 0x1F;
                  timeMinutes = messageResponse[9] & 0x3F;
                  timeSeconds = messageResponse[10] & 0x3F;                    
                  timeTelegramReceivedMillis = currentMillis;
                  
                  Serial.printf("Time telegram received: %02d:%02d:%02d\n", timeHours, timeMinutes, timeSeconds);
                  
                  Serial.printf("Old date and time: %04d-%02d-%02d %02d:%02d:%02d (%1d)\n", year(), month(), day(), hour(), minute(), second(), timeStatus());
                  setTime(timeHours, timeMinutes, timeSeconds, day(), month(), year());
                  Serial.printf("New date and time: %04d-%02d-%02d %02d:%02d:%02d (%1d)\n", year(), month(), day(), hour(), minute(), second(), timeStatus());
                  
                  if (!dateValid || !timeValid){
                     bootTime = now() - getUptimeSeconds();
                  }
                  
                  timeValid   = true;
               }
            }
         }
         
         // Für die nächste Nachricht zurücksetzen
         messageLength = 0;
      }
      
      // Incomplete telegram received timeout
      else if (incompleteTelegramTimeoutEnabled && INCOMPLETE_TELEGRAM_TIMEOUT_MS > 0 && messageLength > 0 && (currentMillis - lastTelegramHeaderReceivedMillis) >= INCOMPLETE_TELEGRAM_TIMEOUT_MS){
         Serial.print("Timeout: Incomplete received telegram after ");
         Serial.print(INCOMPLETE_TELEGRAM_TIMEOUT_MS);
         Serial.println(" ms");
         logConnectionEvent(ConnectionLogEvent::INCOMPLETE_TELEGRAM_TIMEOUT);
         incompleteTelegramTimeouts++;
         resetKnxdConnection();
      }
   }
}


bool connectToKnxd(){
   resetKnxdConnection();
   
   Serial.print("Verbinde mit knxd auf ");
   Serial.println(KNXD_IP);
   
   if (knxdClient.connect(KNXD_IP, KNXD_PORT)) {
      knxdClient.setNoDelay(true);
      knxdClient.keepAlive(KA_IDLE_S, KA_INTERVAL_S, KA_RETRY_COUNT);      
      knxdClient.write(KNXD_GROUP_CONNECTION_REQUEST, sizeof(KNXD_GROUP_CONNECTION_REQUEST));
      
      knxdConnectionInitiated       = true;
      knxdConnectionInitiatedMillis = currentMillis;
      knxdConnectionInitiatedCount++;      
      logConnectionEvent(ConnectionLogEvent::KNXD_CONNECTION_INITIATED);
      
      return knxdClient.connected();
   }
   else{
      Serial.println("Verbindung fehlgeschlagen!");
      knxdConnectionFailedMillis = currentMillis;
      knxdConnectionFailedCount++;
      return false;
   }
}


void resetKnxdConnection(){
   knxdClient.stop();
   knxdConnectionInitiated          = false;
   knxdConnectionConfirmed          = false;
   missingTelegramTimeoutEnabled    = false;
   incompleteTelegramTimeoutEnabled = false;
   knxdConnectionFailedMillis       = currentMillis;
   messageLength                    = 0;
}


void checkButton(uint8_t ch){
   // Button pressed (true = released, false = pressed)
   if (digitalRead(GPIO_BUTTON[ch]) == BUTTON_INVERTED){
      // Button state changed, start debouncing timer
      if (buttonLastState[ch] != BUTTON_INVERTED)
         buttonDebounceMillis[ch] = currentMillis;
      // Check debouncing timer
      else if (buttonDebouncedState[ch] == !BUTTON_INVERTED && (currentMillis - buttonDebounceMillis[ch]) >= BUTTON_DEBOUNCING_TIME_MS) {
         buttonDebouncedState[ch] = BUTTON_INVERTED;
         if (BUTTON_TOGGLE)
            switchRelay(ch, !relayStatus[ch], false, SwitchLogSource::BUTTON, 0);
         else
            switchRelay(ch, true, false, SwitchLogSource::BUTTON, 0);
      }
      
      buttonLastState[ch] = BUTTON_INVERTED;
   }
   // Button released
   else {
      // Button state changed, start debouncing timer
      if (buttonLastState[ch] == BUTTON_INVERTED)
         buttonDebounceMillis[ch] = currentMillis;
      // Check debouncing timer
      else if (buttonDebouncedState[ch] == BUTTON_INVERTED && (currentMillis - buttonDebounceMillis[ch]) >= BUTTON_DEBOUNCING_TIME_MS) {
         buttonDebouncedState[ch] = !BUTTON_INVERTED;
         // Switch relay off if button is not in toggle mode
         if (!BUTTON_TOGGLE && relayStatus[ch])
            switchRelay(ch, false, false, SwitchLogSource::BUTTON, 0);
      }
      
      buttonLastState[ch] = !BUTTON_INVERTED;
   }
}


void ledBlink(){
   // Falls die Verbindung steht, die LED blinken lassen.
   if (knxdConnectionConfirmed) {
      // LED-Blinkstatus ist zurzeit an
      if (ledBlinkStatus) {
         if ((currentMillis - ledBlinkLastSwitch) >= LED_BLINK_ON_TIME_MS){
            // LED invertieren
            if (LED_SHOWS_RELAY_STATUS && relayStatus[0])
               digitalWrite(GPIO_LED, LOW);
            else
               digitalWrite(GPIO_LED, HIGH);
            ledBlinkStatus = false;
            ledBlinkLastSwitch = currentMillis;
         }
      }
      // LED-Blinkstatus ist zurzeit aus
      else {
         if ((currentMillis - ledBlinkLastSwitch) >= LED_BLINK_OFF_TIME_MS){
            // LED invertieren
            if (LED_SHOWS_RELAY_STATUS && relayStatus[0])
               digitalWrite(GPIO_LED, HIGH);
            else
               digitalWrite(GPIO_LED, LOW);
            ledBlinkStatus = true;
            ledBlinkLastSwitch = currentMillis;
         }
      }
   }
   // Die Verbindung ist unterbrochen, auf den Zustand des Relais setzen
   else if (LED_SHOWS_RELAY_STATUS)
      digitalWrite(GPIO_LED,  !relayStatus[0]);
   // Die LED wird nicht für den Relais-Status verwendet, ausschalten
   else
      digitalWrite(GPIO_LED,  HIGH);
      
}


#if SCD30_ENABLE == true
   void measureAir(){
      if (airSensorSCD30.dataAvailable()){
         airCO2         = airSensorSCD30.getCO2();
         airTemperature = airSensorSCD30.getTemperature();
         airHumidity    = airSensorSCD30.getHumidity();
         lastAirMeasurementReceivedMillis = currentMillis;

         if (airSensorSCD30Stuck){
            airSensorSCD30Stuck = false;
            ntfySendMessage("Neuinitialisierung erfolgreich, der SCD30 liefert wieder Daten.");
         }         
      }
      // Check if the SC30 has not delivered new data for a long (10 * AIR_SENSOR_MEASUREMENT_INTERVAL_S) time
      // and reset values to 0, reset the I2C connection and restart the SCD30.
      else if (!airSensorSCD30Stuck && currentMillis - lastAirMeasurementReceivedMillis >= AIR_SENSOR_MEASUREMENT_INTERVAL_S * 10000){
         Serial.println("The SCD30 no longer provides any data. Trying to reset the I2C connection and to reinitialize the sensor. If inactivity continues, the module will be restarted."); 
         ntfySendMessage("Der SCD30 liefert keine Daten mehr. Die I2C-Verbindung wird nun zurückgesetzt und der Sensor anschließend neu initialisiert. Bei fortbestehender Inaktivität wird das Modul neu gestartet.");

         airSensorSCD30Stuck = true;
         airCO2              = 0;
         airTemperature      = 0.0;
         airHumidity         = 0.0;

         // Restart I2C
         Wire.begin(); 
         Wire.setClock(100000L);
         Wire.setClockStretchLimit(200000L);
         
         // Restart SCD30
         airSensorSCD30Connected = airSensorSCD30.begin();

         // Sensirion no auto calibration
         airSensorSCD30.setAutoSelfCalibration(false);
         airSensorSCD30.setTemperatureOffset(AIR_SENSOR_TEMPERATURE_OFFSET_K);
         airSensorSCD30.setAltitudeCompensation(AIR_SENSOR_ALTITUDE_COMPENSATION_M);

         // Measure air every AIR_SENSOR_MEASUREMENT_INTERVAL_S seconds
         airSensorSCD30.setMeasurementInterval(AIR_SENSOR_MEASUREMENT_INTERVAL_S);
      }
      // Check if the SC30 has not delivered new data for a long (20 * AIR_SENSOR_MEASUREMENT_INTERVAL_S) time
      // and restart the ESP, if the reset of I2C and SCD30 was not successful (condition above).
      else if (airSensorSCD30Stuck && currentMillis - lastAirMeasurementReceivedMillis >= AIR_SENSOR_MEASUREMENT_INTERVAL_S * 20000){
         Serial.println("The SCD30 still no longer provides any data. Restarting the Module.");
         delay(100);
         ESP.restart();
      }
      
      #if LCD_ENABLE == true
         char lcdRow1[17],
              lcdRow2[17];
         snprintf(lcdRow1, 17, "%02d:%02d %6d ppm", hour(), minute(), airCO2);
         snprintf(lcdRow2, 17, "%4.1f %cC  %5.1f %%", airTemperature, 223, airHumidity);
         lcd.setCursor(0,0);
         lcd.print(lcdRow1);      
         lcd.setCursor(0,1);
         lcd.print(lcdRow2);
         
         //Serial.println(lcdRow1);
         //Serial.println(lcdRow2);
      #endif
      
      // Send values to KNX
      if (knxdClient.connected() && knxdConnectionConfirmed){
         uint16_t airCO2Diff         = airCO2LastSent > airCO2 ? airCO2LastSent - airCO2 : airCO2 - airCO2LastSent;
         float    airTemperatureDiff = airTemperatureLastSent > airTemperature ? airTemperatureLastSent - airTemperature : airTemperature - airTemperatureLastSent,
                  airHumidityDiff    = airHumidityLastSent > airHumidity ? airHumidityLastSent - airHumidity : airHumidity - airHumidityLastSent;
         
         if ((currentMillis - airCO2LastSentMillis) >= KNX_SEND_INTERVAL_CO2_S * 1000 || airCO2Diff >= KNX_SEND_DIFFERENCE_VALUE_CO2){
            writeGA(GA_AIR_CO2, encodeDpt9Int(airCO2));
            airCO2LastSent       = airCO2;
            airCO2LastSentMillis = currentMillis;
         }
         if ((currentMillis - airTemperatureLastSentMillis) >= KNX_SEND_INTERVAL_TEMPERATURE_S * 1000 || airTemperatureDiff >= KNX_SEND_DIFFERENCE_VALUE_TEMPERATURE){
            writeGA(GA_AIR_TEMPERATURE, encodeDpt9Float(airTemperature));
            airTemperatureLastSent       = airTemperature;
            airTemperatureLastSentMillis = currentMillis;
         }
         if ((currentMillis - airHumidityLastSentMillis) >= KNX_SEND_INTERVAL_HUMIDITY_S * 1000 || airHumidityDiff >= KNX_SEND_DIFFERENCE_VALUE_HUMIDITY){         
            writeGA(GA_AIR_HUMIDITY, encodeDpt9Float(airHumidity));
            airHumidityLastSent       = airHumidity;
            airHumidityLastSentMillis = currentMillis;
         }
      }

      lastAirSensorPolledMillis = currentMillis;
   }
#endif


void switchRelay(const uint8_t ch, const bool on, const bool overrideLock, SwitchLogSource source, const uint8_t *ga){
   if (ch >= CHANNELS){
      Serial.print("Ungültiger Kanal: ");
      Serial.println(ch + 1);
   }
   else if (relayStatus[ch] != on){
      if (!overrideLock && lockActive[ch]){
         Serial.print("Schaltkommando wird ignoriert, Kanal ");
         Serial.print(ch + 1);
         Serial.println(" ist gesperrt!");
      }
      else{
         relayStatus[ch] = on;
         
         if (on){
            if (AUTO_OFF_TIMER_S[ch] > 0){
               autoOffTimerStartMillis[ch] = currentMillis;
               autoOffTimerActive[ch] = true;
            }
            logSwitchEvent(ch, SwitchLogEvent::ON, source, ga);
            Serial.print("Kanal ");
            Serial.print(ch + 1);
            Serial.println(" wird eingeschaltet");
         }
         else{
            autoOffTimerActive[ch] = false;
            logSwitchEvent(ch, SwitchLogEvent::OFF, source, ga);
            Serial.print("Kanal ");
            Serial.print(ch + 1);
            Serial.println(" wird ausgeschaltet");
         }
         
         if (RELAY_NORMALLY_OPEN[ch])
            digitalWrite(GPIO_RELAY[ch], relayStatus[ch]);
         else
            digitalWrite(GPIO_RELAY[ch], !relayStatus[ch]);

         // Die LED zeigt immer den Zustand des ersten Relais an
         if (LED_SHOWS_RELAY_STATUS)
            digitalWrite(GPIO_LED,  !relayStatus[0]);
            
         writeGA(GA_STATUS[ch], relayStatus[ch]);
      }
   }
}


void lockRelay(const uint8_t ch, const bool lock, SwitchLogSource source, const uint8_t *ga){
   if (ch >= CHANNELS){
      Serial.print("Ungültiger Kanal: ");
      Serial.println(ch + 1);
   }
   else {      
      if (lock && !lockActive[ch]){
         Serial.print("Kanal ");
         Serial.print(ch + 1);
         Serial.println(" wird gesperrt!");
         logSwitchEvent(ch, SwitchLogEvent::LOCK, source, ga);
         
         if (AUTO_UNLOCK_TIMER_S[ch] > 0){
            autoUnlockTimerStartMillis[ch] = currentMillis;
            autoUnlockTimerActive[ch] = true;
         }
         
         if (SWITCH_OFF_WHEN_LOCKED[ch])
            switchRelay(ch, false, false, SwitchLogSource::OFF_BY_LOCK, 0);
         else if (SWITCH_ON_WHEN_LOCKED[ch])
            switchRelay(ch, true, false, SwitchLogSource::ON_BY_LOCK, 0);
         
         lockActive[ch] = true;         
      }                  
      else if (!lock && lockActive[ch]){
         lockActive[ch] = false;
         autoUnlockTimerActive[ch] = false;
         
         Serial.print("Kanal ");
         Serial.print(ch + 1);
         Serial.println(" wird entsperrt!");
         logSwitchEvent(ch, SwitchLogEvent::UNLOCK, source, ga);
         
         if (SWITCH_OFF_WHEN_UNLOCKED[ch])
            switchRelay(ch, false, false, SwitchLogSource::OFF_BY_UNLOCK, 0);
         else if (SWITCH_ON_WHEN_UNLOCKED[ch])
            switchRelay(ch, true, false, SwitchLogSource::ON_BY_UNLOCK, 0);
      }
   }
}


void writeGA(const uint8_t ga[], const bool status){
   if (knxdClient.connected()){
      const uint8_t groupValueWrite[] = {0x00, 0x06, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, (uint8_t)((ga[0] << 3) + ga[1]), ga[2], 0x00, (uint8_t)(0x80 | status)};
      knxdClient.write(groupValueWrite, sizeof(groupValueWrite));
   }   
}


void writeGA(const uint8_t ga[], const uint16_t data){
   if (knxdClient.connected()){
      const uint8_t groupValueWrite[] = {0x00, 0x08, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, (uint8_t)((ga[0] << 3) + ga[1]), ga[2], 0x00, 0x80, (uint8_t)((data >> 8) & 0xFF), (uint8_t)(data & 0xFF)};
      knxdClient.write(groupValueWrite, sizeof(groupValueWrite));
   }   
}


void responseGA(const uint8_t ga[], const bool status){
   if (knxdClient.connected()){
      const uint8_t groupValueResponse[] = {0x00, 0x06, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, (uint8_t)((ga[0] << 3) + ga[1]), ga[2], 0x00, (uint8_t)(0x40 | status)};
      knxdClient.write(groupValueResponse, sizeof(groupValueResponse));
   }   
}


void responseGA(const uint8_t ga[], const uint16_t data){
   if (knxdClient.connected()){
      const uint8_t groupValueWrite[] = {0x00, 0x08, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, (uint8_t)((ga[0] << 3) + ga[1]), ga[2], 0x00, 0x40, (uint8_t)((data >> 8) & 0xFF), (uint8_t)(data & 0xFF)};
      knxdClient.write(groupValueWrite, sizeof(groupValueWrite));
   }   
}


void readGA(const uint8_t ga[]){
   if (knxdClient.connected()){
      const uint8_t groupValueRequest[] = {0x00, 0x06, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, (uint8_t)((ga[0] << 3) + ga[1]), ga[2], 0x00, 0x00};
      knxdClient.write(groupValueRequest, sizeof(groupValueRequest));
   }   
}


uint32_t getUptimeSeconds(){
   uint64_t uptimeMillis = (((uint64_t)millisOverflows) << 32) | currentMillis;
   return (uint32_t)(uptimeMillis / 1000ULL);
}


void getUptimeString(char* buffer, size_t size, uint32_t totalSeconds){
   uint32_t seconds       = totalSeconds % 60,
            minutes       = (totalSeconds / 60) % 60,
            hours         = (totalSeconds / (60 * 60)) % 24,
            days          = totalSeconds / (60 * 60 * 24);
   
        if (days == 0) snprintf(buffer, size,          "%02d:%02d:%02d", hours, minutes, seconds);
   else if (days == 1) snprintf(buffer, size,   "1 Tag, %02d:%02d:%02d", hours, minutes, seconds);    
   else                snprintf(buffer, size, "%d Tage, %02d:%02d:%02d", days, hours, minutes, seconds); // 21 + 1 characters
}


void getTimeString(char* buffer, size_t size, time_t timestamp){
   snprintf(buffer, size, "%02d:%02d:%02d", hour(timestamp), minute(timestamp), second(timestamp)); // 8 + 1 characters
}


void getDateString(char* buffer, size_t size, time_t timestamp){   
   snprintf(buffer, size, "%04d-%02d-%02d", year(timestamp), month(timestamp), day(timestamp)); // 10 + 1 characers
}


void getWeekdayString(char* buffer, size_t size, time_t timestamp){
   switch (weekday(timestamp)){
      case 1  : strncpy(buffer, "Sonntag",    size); break;
      case 2  : strncpy(buffer, "Montag",     size); break;
      case 3  : strncpy(buffer, "Dienstag",   size); break;
      case 4  : strncpy(buffer, "Mittwoch",   size); break;
      case 5  : strncpy(buffer, "Donnerstag", size); break; // 10 + 1 characters
      case 6  : strncpy(buffer, "Freitag",    size); break;
      case 7  : strncpy(buffer, "Samstag",    size); break;
      default : strncpy(buffer, "Invalid",    size); break;
   }
}


uint16_t encodeDpt9Internal(const int32_t value) {
   uint16_t sign     = 0;
   uint8_t  exponent = 0;   
   int32_t  mantissa = value;
   
   if (value < 0)
      sign = 0x8000;
   
   while (mantissa > 2047 || mantissa < -2048) {
      exponent++;
      mantissa >>= 1;
   }
   
   uint16_t data = sign | (exponent << 11) | (mantissa & 0x07FF);   
   
   return data;
}


uint16_t encodeDpt9Int(const int32_t value) {
   return encodeDpt9Internal(value * 100);
}


uint16_t encodeDpt9Float(const float value) {
   return encodeDpt9Internal((int32_t) ((value * 100) + 0.5));
}


float decodeDpt9(uint16_t data) {   
   int16_t mantissa = data & 0x07FF;
   uint8_t exponent = (data >> 11) & 0xF;
   bool sign = (data & 0x8000) == 0x8000;
   
   if (sign)
      mantissa = mantissa | 0xF800;
   
   float value = (mantissa / 100.0) * (1 << exponent);
   
   return value;   
}


void logConnectionEvent(ConnectionLogEvent event, uint8_t wiFiDisconnectReason){
   auto& clr = connectionLogRingbuffer[connectionLogEntries % LOG_SIZE];

   clr.entry                = connectionLogEntries;
   clr.uptimeSeconds        = getUptimeSeconds();
   clr.timestamp            = now();
   clr.timestampValid       = dateValid && timeValid;
   clr.wlanChannel          = WiFi.channel();
   clr.event                = event;
   clr.wiFiDisconnectReason = wiFiDisconnectReason;
   memcpy(clr.wlanBssid, WiFi.BSSID(), 6);
   connectionLogEntries++;
}


void logConnectionEvent(ConnectionLogEvent event){
   logConnectionEvent(event, 0);
}


void logSwitchEvent(const uint8_t ch, SwitchLogEvent event, SwitchLogSource source, const uint8_t *ga){
   auto& slr = switchLogRingbuffer[switchLogEntries % LOG_SIZE];

   slr.entry          = switchLogEntries;
   slr.uptimeSeconds  = getUptimeSeconds();
   slr.timestamp      = now();
   slr.timestampValid = dateValid && timeValid;
   slr.event          = event;
   slr.channel        = ch;
   slr.source         = source;
   slr.ga             = ga;
   switchLogEntries++;
}


const char* getWiFiDisconnectReasonString(uint8_t reason) {
   switch (reason) {
      case WIFI_DISCONNECT_REASON_UNSPECIFIED:              return "Unspecified";
      case WIFI_DISCONNECT_REASON_AUTH_EXPIRE:              return "Auth expired";
      case WIFI_DISCONNECT_REASON_AUTH_LEAVE:               return "Auth leave";
      case WIFI_DISCONNECT_REASON_ASSOC_EXPIRE:             return "Association expired";
      case WIFI_DISCONNECT_REASON_ASSOC_TOOMANY:            return "Too many associations";
      case WIFI_DISCONNECT_REASON_NOT_AUTHED:               return "Not authenticated";
      case WIFI_DISCONNECT_REASON_NOT_ASSOCED:              return "Not associated";
      case WIFI_DISCONNECT_REASON_ASSOC_LEAVE:              return "Association leave";
      case WIFI_DISCONNECT_REASON_ASSOC_NOT_AUTHED:         return "Association not authenticated";
      case WIFI_DISCONNECT_REASON_DISASSOC_PWRCAP_BAD:      return "Power capability bad";
      case WIFI_DISCONNECT_REASON_DISASSOC_SUPCHAN_BAD:     return "Supported channel bad";
      case WIFI_DISCONNECT_REASON_IE_INVALID:               return "Invalid IE";
      case WIFI_DISCONNECT_REASON_MIC_FAILURE:              return "MIC failure";
      case WIFI_DISCONNECT_REASON_4WAY_HANDSHAKE_TIMEOUT:   return "4-way handshake timeout";
      case WIFI_DISCONNECT_REASON_GROUP_KEY_UPDATE_TIMEOUT: return "Group key timeout";
      case WIFI_DISCONNECT_REASON_IE_IN_4WAY_DIFFERS:       return "IE mismatch in handshake";
      case WIFI_DISCONNECT_REASON_GROUP_CIPHER_INVALID:     return "Group cipher invalid";
      case WIFI_DISCONNECT_REASON_PAIRWISE_CIPHER_INVALID:  return "Pairwise cipher invalid";
      case WIFI_DISCONNECT_REASON_AKMP_INVALID:             return "AKMP invalid";
      case WIFI_DISCONNECT_REASON_UNSUPP_RSN_IE_VERSION:    return "Unsupported RSN IE";
      case WIFI_DISCONNECT_REASON_INVALID_RSN_IE_CAP:       return "Invalid RSN capabilities";
      case WIFI_DISCONNECT_REASON_802_1X_AUTH_FAILED:       return "802.1X auth failed";
      case WIFI_DISCONNECT_REASON_CIPHER_SUITE_REJECTED:    return "Cipher rejected";
      case WIFI_DISCONNECT_REASON_BEACON_TIMEOUT:           return "Beacon timeout";
      case WIFI_DISCONNECT_REASON_NO_AP_FOUND:              return "No AP found";
      case WIFI_DISCONNECT_REASON_AUTH_FAIL:                return "Auth failed";
      case WIFI_DISCONNECT_REASON_ASSOC_FAIL:               return "Association failed";
      case WIFI_DISCONNECT_REASON_HANDSHAKE_TIMEOUT:        return "Handshake timeout";
      default:                                              return "Unknown reason";
  }
}


const char* getConnectionLogEventString(ConnectionLogEvent event) {
   switch (event) {
      case ConnectionLogEvent::WLAN_CONNECTION_INITIATED:         return "WLAN-Verbindung initiiert";
      case ConnectionLogEvent::WLAN_CONNECTED:                    return "WLAN-Verbindung hergestellt";
      case ConnectionLogEvent::WLAN_DHCP_COMPLETED:               return "IP-Adresse per DHCP erhalten";
      case ConnectionLogEvent::WLAN_DISCONNECTED:                 return "WLAN-Verbindung getrennt";
      case ConnectionLogEvent::WLAN_CONNECTION_TIMEOUT:           return "Zeitüberschreitungen beim Aufbau der WLAN-Verbindung";
      case ConnectionLogEvent::WLAN_DHCP_TIMEOUT:                 return "DHCP-Zeitüberschreitungen";
      case ConnectionLogEvent::KNXD_CONNECTION_INITIATED:         return "Initiale Verbindung zum knxd hergestellt";
      case ConnectionLogEvent::KNXD_CONNECTION_HANDSHAKE_TIMEOUT: return "Zeitüberschreitungen bei der Verbindungsbestätigung durch den knxd";
      case ConnectionLogEvent::KNXD_CONNECTION_CONFIRMED:         return "Verbindung vom knxd bestätigt";
      case ConnectionLogEvent::KNXD_DISCONNECTED:                 return "Verbindung zum knxd getrennt";
      case ConnectionLogEvent::MISSING_TELEGRAM_TIMEOUT:          return "Verbindungsabbruch wegen Zeitüberschreitung zwischen zwei Telegrammen";
      case ConnectionLogEvent::INCOMPLETE_TELEGRAM_TIMEOUT:       return "Verbindungsabbruch wegen unvollständig empfangenem Telegramm";
      default:                                                    return "Invalid";
  }
}


const char* getSwitchLogEventString(SwitchLogEvent event) {
   switch (event) {
      case SwitchLogEvent::OFF:    return "Ausgeschaltet";
      case SwitchLogEvent::ON:     return "Eingeschaltet";
      case SwitchLogEvent::LOCK:   return "Gesperrt";
      case SwitchLogEvent::UNLOCK: return "Entsperrt";
      default:                     return "Invalid";
  }
}


const char* getSwitchLogSourceString(SwitchLogSource source) {
   switch (source) {
      case SwitchLogSource::GROUP_ADDRESS:     return "GA";
      case SwitchLogSource::BUTTON:            return "Taster";
      case SwitchLogSource::ON_BY_LOCK:        // fall-through
      case SwitchLogSource::OFF_BY_LOCK:       return "Sperrautomatik";
      case SwitchLogSource::ON_BY_UNLOCK:      // fall-through
      case SwitchLogSource::OFF_BY_UNLOCK:     return "Entsperrautomatik";
      case SwitchLogSource::AUTO_OFF_TIMER:    return "Zeitschalter";
      case SwitchLogSource::AUTO_UNLOCK_TIMER: return "Zeitschalter";
      case SwitchLogSource::WEBSERVER:         return "Webserver";
      default:                                 return "Invalid";
  }
}

#if NTFY_ENABLE == true
   bool ntfySendMessage(const char* message) {
      return ntfy.enqueue(HOST_NAME, message);
   }
#else
   bool ntfySendMessage(const char* message) {
      Serial.print("ntfy ist deaktiviert, die Nachricht kann nicht gesendet werden: ");
      Serial.println(message);
      return false;
   }
#endif