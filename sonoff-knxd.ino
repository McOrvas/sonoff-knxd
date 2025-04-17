/*
 * Notwendige Einstellungen in der Arduino IDE
 * 
 * Boardverwalter-URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json
 * 
 * Sonoff S20
 * Board:      Generic ESP8266 Module
 * Flash Size: 1MB (FS:64KB OTA:~470KB) [Notwendig für Updates über die Weboberfläche]
 * 
 * Sonoff 4CH
 * Board:      Generic ESP8285 Module
 * Flash Size: 1MB (FS:64KB OTA:~470KB) [Notwendig für Updates über die Weboberfläche]
 * 
 * Adafruit Feather HUZZAH ESP8266
 * Board:      Adafruit Feather HUZZAH ESP8266
 * Flash Size: 4MB (FS:2MB OTA:~1019KB) [Notwendig für Updates über die Weboberfläche]
 */

#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
// https://github.com/knxd/knxd/blob/master/src/include/eibtypes.h
#include "eibtypes.h"

/*
 * *******************************
 * *** Laden der Konfiguration ***
 * *******************************
 */

#include "Configuration.h"

#if SCD30_ENABLE == true
// https://www.arduino.cc/en/reference/wire
#include <Wire.h>
// https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library
#include <SparkFun_SCD30_Arduino_Library.h>

#if LCD_ENABLE == true
// https://github.com/Seeed-Studio/Grove_LCD_RGB_Backlight
#include <rgb_lcd.h>
#endif
#endif

/* 
 * *************************
 * *** Interne Variablen *** 
 * *************************
 */

const char     *SOFTWARE_VERSION                      = "2025-04-17",

               *LOG_WLAN_CONNECTION_INITIATED         = "WLAN-Verbindung initiiert",
               *LOG_WLAN_CONNECTED                    = "WLAN-Verbindung hergestellt",
               *LOG_WLAN_DHCP_COMPLETED               = "IP-Adresse per DHCP erhalten",
               *LOG_WLAN_DISCONNECTED                 = "WLAN-Verbindung getrennt",
               *LOG_WLAN_CONNECTION_TIMEOUT           = "Zeitüberschreitungen beim Aufbau der WLAN-Verbindung",
               *LOG_WLAN_DHCP_TIMEOUT                 = "DHCP-Zeitüberschreitungen",
               *LOG_KNXD_CONNECTION_INITIATED         = "Initiale Verbindung zum knxd hergestellt",
               *LOG_KNXD_CONNECTION_HANDSHAKE_TIMEOUT = "Zeitüberschreitungen bei der Verbindungsbestätigung durch den knxd",
               *LOG_KNXD_CONNECTION_CONFIRMED         = "Verbindung vom knxd bestätigt",
               *LOG_KNXD_DISCONNECTED                 = "Verbindung zum knxd getrennt",
               *LOG_MISSING_TELEGRAM_TIMEOUT          = "Verbindungsabbruch wegen Zeitüberschreitung zwischen zwei Telegrammen",
               *LOG_INCOMPLETE_TELEGRAM_TIMEOUT       = "Verbindungsabbruch wegen unvollständig empfangenem Telegramm",
               
               *SWITCH_LOG_OFF                        = "Ausgeschaltet",
               *SWITCH_LOG_ON                         = "Eingeschaltet",
               *SWITCH_LOG_LOCK                       = "Gesperrt",
               *SWITCH_LOG_UNLOCK                     = "Entsperrt",
               *SWITCH_LOG_BUTTON                     = "Taster",
               *SWITCH_LOG_ON_BY_LOCK                 = "Sperrautomatik",
               *SWITCH_LOG_OFF_BY_UNLOCK              = "Entsperrautomatik",
               *SWITCH_LOG_AUTO_OFF_TIMER             = "Zeitschalter",
               *SWITCH_LOG_AUTO_UNLOCK_TIMER          = "Zeitschalter",
               *SWITCH_LOG_WEBSERVER                  = "Webserver";

const uint8_t  GA_SWITCH_COUNT                  = sizeof(GA_SWITCH[0]) / 3,
               GA_LOCK_COUNT                    = sizeof(GA_LOCK[0]) / 3,
               KNXD_GROUP_CONNECTION_REQUEST[]  = {0x00, 0x05, EIB_OPEN_GROUPCON >> 8, EIB_OPEN_GROUPCON & 0xFF, 0x00, 0x00, 0x00};

const boolean  GA_DATE_VALID = GA_DATE[0] + GA_DATE[1] + GA_DATE[2] > 0,
               GA_TIME_VALID = GA_TIME[0] + GA_TIME[1] + GA_TIME[2] > 0;
               
uint8_t        messageLength = 0,
               messageResponse[32],
               wifiConnected = 0,
               timeWeekday   = 0,
               timeHours     = 0,
               timeMinutes   = 0,
               timeSeconds   = 0,
               dateDay       = 0,
               dateMonth     = 0;
uint16_t       dateYear      = 0;
time_t         bootTime      = 0;

boolean        knxdConnectionInitiated          = false,
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
               
// Variablen zur Zeitmessung
               currentMillis                    = 0,
               currentMillisTemp                = 0,
               millisOverflows                  = 0,
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

#if SCD30_ENABLE == true
// Objekt for SDC30 environment sensor
SCD30          airSensorSCD30;
#if LCD_ENABLE == true
// Object for LCD
rgb_lcd        lcd;
#endif
#endif

// Variables for air quality
float          airTemperature         = 0.0,
               airTemperatureLastSent = 0.0,
               airHumidity            = 0.0,
               airHumidityLastSent    = 0.0;
uint16_t       airCO2                 = 0,
               airCO2LastSent         = 0;

// WLAN-Client
WiFiClient              client;
WiFiEventHandler        connectHandler,
                        disconnectHandler,
                        gotIpHandler,
                        dhcpTimeoutHandler;

// Webserver
ESP8266WebServer        webServer(80);
ESP8266HTTPUpdateServer httpUpdateServer;

// Variablen zur Ereignisspeicherung
const uint32_t  LOG_SIZE = 100;
uint32_t        connectionLogEntries = 0;
uint32_t        switchLogEntries = 0;

struct logConnectionEvent {
   const char  *message;
   uint32_t    entry,
               uptimeSeconds;
   time_t      timestamp;
   boolean     timestampValid;
   int32_t     wlanChannel;
   uint8_t     wlanBssid[6];
} connectionLogRingbuffer[LOG_SIZE];

struct logSwitchEvent {
   const uint8_t *ga;
   const char    *type,
                 *message;
   uint8_t       channel;   
   uint32_t      entry,
                 uptimeSeconds;
   time_t        timestamp;
   boolean       timestampValid;
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

   if (Wire.status() != I2C_OK) Serial.println("Something wrong with I2C");
   
   // Initialize SCD30 environment sensor
   airSensorSCD30Connected = airSensorSCD30.begin();
   if (!airSensorSCD30Connected) {
      Serial.println("The SCD30 did not respond. Please check wiring."); 
   }

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
}


void onWifiConnected(const WiFiEventStationModeConnected& event) {
   wifiConnected = 2;
   logConnectionEvent(LOG_WLAN_CONNECTED);
   Serial.print("WLAN-Verbindung hergestellt mit ");
   Serial.print(WiFi.BSSIDstr());
   Serial.print(" auf Kanal ");
   Serial.println(WiFi.channel());
}


void onWifiGotIP(const WiFiEventStationModeGotIP& event) {
   wifiConnected = 3;
   wifiCurrentConnectionAttempts = 0;
   logConnectionEvent(LOG_WLAN_DHCP_COMPLETED);
   Serial.print("IP-Adresse: ");
   Serial.println(WiFi.localIP());

   // Disable the Access Point again
   if (WiFi.getMode() == WIFI_AP_STA || WiFi.getMode() == WIFI_AP) {
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      Serial.println("Deaktiviere den Access Point und setze den WLAN-Modus zurück auf WIFI_STA.");
   }
}


void onWifiDhcpTimeout() {
   wifiConnected = 0;
   wifiDisconnectedMillis = currentMillis;
   WiFi.disconnect();
   logConnectionEvent(LOG_WLAN_DHCP_TIMEOUT);
   Serial.println(LOG_WLAN_DHCP_TIMEOUT);
}


void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
   wifiConnected = 0;
   wifiDisconnectedMillis = currentMillis;  
   wifiDisconnections++;
   resetKnxdConnection();
   logConnectionEvent(LOG_WLAN_DISCONNECTED); 
   Serial.print(LOG_WLAN_DISCONNECTED);
   Serial.print(", Grund: ");
   Serial.print(event.reason);
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


void loop() {
   currentMillisTemp = millis();
   
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
         switchRelay(ch, false, AUTO_OFF_TIMER_OVERRIDES_LOCK, SWITCH_LOG_AUTO_OFF_TIMER, 0);
      }

      // Check if a channel has to be unlocked by a timer
      if (lockActive[ch] && autoUnlockTimerActive[ch] && (currentMillis - autoUnlockTimerStartMillis[ch]) >= (AUTO_UNLOCK_TIMER_S[ch] * 1000)){
         Serial.print("Auto unlock timer for channel ");
         Serial.print(ch + 1);
         Serial.println(" has expired!");
         autoUnlockTimerActive[ch] = false;
         lockRelay(ch, false, SWITCH_LOG_AUTO_UNLOCK_TIMER, 0);
      }
   }
   
   if (WiFi.status() != WL_CONNECTED || wifiConnected < 3) {              
      // Wifi connection not yet initialized or delay has expired
      if (wifiConnected == 0 && (currentMillis - wifiDisconnectedMillis) >= WIFI_CONNECTION_LOST_DELAY_S * 1000) {
         wifiConnected = 1;
         wifiConnectionInitiatedMillis = currentMillis;
         wifiCurrentConnectionAttempts++;
         WiFi.begin(SSID, PASSWORD);
         logConnectionEvent(LOG_WLAN_CONNECTION_INITIATED);
         Serial.print("Verbinde mit WLAN '");
         Serial.print(SSID);
         Serial.println("'");   
      }
      
      // The connection is already initialized
      else if (wifiConnected > 0 && (currentMillis - wifiConnectionInitiatedMillis) >= WIFI_CONNECTION_TIMEOUT_S * 1000) {
         wifiConnected = 0;
         wifiDisconnectedMillis = currentMillis;
         WiFi.disconnect();
         logConnectionEvent(LOG_WLAN_CONNECTION_TIMEOUT);
         Serial.println(LOG_WLAN_CONNECTION_TIMEOUT);
      }
   }
   
   // WLAN connected and IP address received
   else if (wifiConnected == 3) {
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
   else if (!client.connected()){
      Serial.println("Die Verbindung zum knxd wurde getrennt.");      
      logConnectionEvent(LOG_KNXD_DISCONNECTED);
      knxdDisconnections++;
      resetKnxdConnection();
   }
   
   // Der Verbindungsaufbau zum knxd wurde nicht rechtzeitig bestätigt
   else if (!knxdConnectionConfirmed && (currentMillis - knxdConnectionInitiatedMillis) >= CONNECTION_CONFIRMATION_TIMEOUT_MS){
      Serial.print("Der Verbindungsaufbau zum knxd wurde nicht innerhalb von ");
      Serial.print(CONNECTION_CONFIRMATION_TIMEOUT_MS);
      Serial.println(" ms bestätigt.");
      logConnectionEvent(LOG_KNXD_CONNECTION_HANDSHAKE_TIMEOUT);
      knxdConnectionHandshakeTimeouts++;
      resetKnxdConnection();
   }
   
   // Die Verbindung steht prinzipiell, aber seit MISSING_TELEGRAM_TIMEOUT_MIN wurde kein Telegramm mehr empfangen und deshalb wird ein Timeout ausgelöst
   else if (missingTelegramTimeoutEnabled && MISSING_TELEGRAM_TIMEOUT_MIN > 0 && (currentMillis - lastTelegramReceivedMillis) >= MISSING_TELEGRAM_TIMEOUT_MIN * 60000){
      Serial.print("Timeout: No telegram received during ");
      Serial.print(MISSING_TELEGRAM_TIMEOUT_MIN);
      Serial.println(" minutes");
      logConnectionEvent(LOG_MISSING_TELEGRAM_TIMEOUT);
      missingTelegramTimeouts++;
      resetKnxdConnection();
   }
   
   // Die Verbindung ist etabliert
   else {
      // Die Länge einer neuen Nachricht lesen
      if (messageLength == 0 && client.available() >= 2){
         messageLength = (((int) client.read()) << 8) + client.read();
         lastTelegramHeaderReceivedMillis = currentMillis;
         incompleteTelegramTimeoutEnabled = true;
      }
      
      // Die Nutzdaten einer Nachricht lesen
      if (messageLength > 0 && client.available() >= messageLength){
         client.read(messageResponse, messageLength);
         
         // Prüfen, ob der initiale Verbindungsaufbau korrekt bestätigt wurde      
         if (!knxdConnectionConfirmed && messageLength == 2 && messageResponse[0] == KNXD_GROUP_CONNECTION_REQUEST[2] && messageResponse[1] == KNXD_GROUP_CONNECTION_REQUEST[3]){
            Serial.println("EIBD/KNXD Verbindung hergestellt");            
            logConnectionEvent(LOG_KNXD_CONNECTION_CONFIRMED);
            knxdConnectionConfirmed = true;   
            knxdConnectionConfirmedCount++;
            
            // Auch wenn bisher kein echtes Telegramm vom Bus kam, die Variablen hier bereits entsprechend setzen,
            // da ansonsten ein dauerhaft toter Bus nicht über den Timeout erkannt würde.
            lastTelegramReceivedMillis = currentMillis;
            missingTelegramTimeoutEnabled = true;
            
            // Die Status-GA senden, sobald die Verbindung steht. Dadurch wird sie beim ersten Start nach einem Spannungsausfall gesendet.
            for (uint8_t ch= 0; ch<CHANNELS; ch++)
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
                         && messageResponse[4] == (GA_SWITCH[0][i][0] << 3) + GA_SWITCH[0][i][1]
                         && messageResponse[5] == GA_SWITCH[0][i][2]){
                         switchRelay(ch, messageResponse[7] & 0x0F, false, 0, &GA_SWITCH[ch][i][0]);
                     }
                  }
                  for (uint8_t i=0; i<GA_LOCK_COUNT; i++){
                     // Sperren
                     if (GA_LOCK[ch][i][0] + GA_LOCK[ch][i][1] + GA_LOCK[ch][i][2] > 0
                         && messageResponse[4] == (GA_LOCK[0][i][0] << 3) + GA_LOCK[0][i][1]
                         && messageResponse[5] == GA_LOCK[0][i][2]){
                         lockRelay(ch, (messageResponse[7] & 0x0F) ^ LOCK_INVERTED[ch], 0, &GA_LOCK[ch][i][0]);
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
         logConnectionEvent(LOG_INCOMPLETE_TELEGRAM_TIMEOUT);
         incompleteTelegramTimeouts++;
         resetKnxdConnection();
      }
   }
}


boolean connectToKnxd(){
   resetKnxdConnection();
   
   Serial.print("Verbinde mit knxd auf ");
   Serial.println(KNXD_IP);
   
   if (client.connect(KNXD_IP, KNXD_PORT)) {
      client.setNoDelay(true);
      client.keepAlive(KA_IDLE_S, KA_INTERVAL_S, KA_RETRY_COUNT);      
      client.write(KNXD_GROUP_CONNECTION_REQUEST, sizeof(KNXD_GROUP_CONNECTION_REQUEST));
      
      knxdConnectionInitiated       = true;
      knxdConnectionInitiatedMillis = currentMillis;
      knxdConnectionInitiatedCount++;      
      logConnectionEvent(LOG_KNXD_CONNECTION_INITIATED);
      
      return client.connected();
   }
   else{
      Serial.println("Verbindung fehlgeschlagen!");
      knxdConnectionFailedMillis = currentMillis;
      knxdConnectionFailedCount++;
      return false;
   }
}


void resetKnxdConnection(){
   client.stop();
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
            switchRelay(ch, !relayStatus[ch], false, SWITCH_LOG_BUTTON, 0);
         else
            switchRelay(ch, true, false, SWITCH_LOG_BUTTON, 0);
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
            switchRelay(ch, false, false, SWITCH_LOG_BUTTON, 0);
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
      airSensorSCD30Stuck = false;
   }
   // Check if the SC30 has not delivered new data for a long (10 * AIR_SENSOR_MEASUREMENT_INTERVAL_S) time
   // and reset values to 0, reset the I2C connection and restart the SCD30.
   else if (!airSensorSCD30Stuck && currentMillis - lastAirMeasurementReceivedMillis >= AIR_SENSOR_MEASUREMENT_INTERVAL_S * 10000){
      Serial.println("The SCD30 no longer provides any data. Trying to reset the I2C connection and to reinitialize the sensor."); 

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
      Serial.println("The SCD30 still no longer provides any data. Restarting the ESP."); 
      delay(1000);
      ESP.restart();
   }
   
   #if LCD_ENABLE == true
   static char lcdRow1[17],
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
   if (client.connected() && knxdConnectionConfirmed){
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


void switchRelay(const uint8_t ch, const boolean on, const boolean overrideLock, const char *source, const uint8_t *ga){
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
            logSwitchEvent(ch, SWITCH_LOG_ON, source, ga);
            Serial.print("Kanal ");
            Serial.print(ch + 1);
            Serial.println(" wird eingeschaltet");
         }
         else{
            autoOffTimerActive[ch] = false;
            logSwitchEvent(ch, SWITCH_LOG_OFF, source, ga);
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


void lockRelay(const uint8_t ch, const boolean lock, const char *source, const uint8_t *ga){
   if (ch >= CHANNELS){
      Serial.print("Ungültiger Kanal: ");
      Serial.println(ch + 1);
   }
   else {      
      if (lock && !lockActive[ch]){
         Serial.print("Kanal ");
         Serial.print(ch + 1);
         Serial.println(" wird gesperrt!");
         logSwitchEvent(ch, SWITCH_LOG_LOCK, source, ga);
         
         if (AUTO_UNLOCK_TIMER_S[ch] > 0){
            autoUnlockTimerStartMillis[ch] = currentMillis;
            autoUnlockTimerActive[ch] = true;
         }
         
         if (SWITCH_OFF_WHEN_LOCKED[ch])
            switchRelay(ch, false, false, SWITCH_LOG_ON_BY_LOCK, 0);
         else if (SWITCH_ON_WHEN_LOCKED[ch])
            switchRelay(ch, true, false, SWITCH_LOG_ON_BY_LOCK, 0);
         
         lockActive[ch] = true;         
      }                  
      else if (!lock && lockActive[ch]){
         lockActive[ch] = false;
         autoUnlockTimerActive[ch] = false;
         
         Serial.print("Kanal ");
         Serial.print(ch + 1);
         Serial.println(" wird entsperrt!");
         logSwitchEvent(ch, SWITCH_LOG_UNLOCK, source, ga);
         
         if (SWITCH_OFF_WHEN_UNLOCKED[ch])
            switchRelay(ch, false, false, SWITCH_LOG_OFF_BY_UNLOCK, 0);
         else if (SWITCH_ON_WHEN_UNLOCKED[ch])
            switchRelay(ch, true, false, SWITCH_LOG_OFF_BY_UNLOCK, 0);
      }
   }
}


void writeGA(const uint8_t ga[], const boolean status){
   if (client.connected()){
      const uint8_t groupValueWrite[] = {0x00, 0x06, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, (ga[0] << 3) + ga[1], ga[2], 0x00, 0x80 | status};
      client.write(groupValueWrite, sizeof(groupValueWrite));
   }   
}


void writeGA(const uint8_t ga[], const uint16_t data){
   if (client.connected()){
      const uint8_t groupValueWrite[] = {0x00, 0x08, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, (ga[0] << 3) + ga[1], ga[2], 0x00, 0x80, (data >> 8) & 0xFF, data & 0xFF};
      client.write(groupValueWrite, sizeof(groupValueWrite));
   }   
}


void responseGA(const uint8_t ga[], const boolean status){
   if (client.connected()){
      const uint8_t groupValueResponse[] = {0x00, 0x06, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, (ga[0] << 3) + ga[1], ga[2], 0x00, 0x40 | status};
      client.write(groupValueResponse, sizeof(groupValueResponse));
   }   
}


void responseGA(const uint8_t ga[], const uint16_t data){
   if (client.connected()){
      const uint8_t groupValueWrite[] = {0x00, 0x08, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, (ga[0] << 3) + ga[1], ga[2], 0x00, 0x40, (data >> 8) & 0xFF, data & 0xFF};
      client.write(groupValueWrite, sizeof(groupValueWrite));
   }   
}


void readGA(const uint8_t ga[]){
   if (client.connected()){
      const uint8_t groupValueRequest[] = {0x00, 0x06, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, (ga[0] << 3) + ga[1], ga[2], 0x00, 0x00};
      client.write(groupValueRequest, sizeof(groupValueRequest));
   }   
}


uint32_t getUptimeSeconds(){
   return (currentMillis / 1000) + (millisOverflows * (0xFFFFFFFF / 1000));
}


char* getUptimeString(uint32_t totalSeconds){
   uint32_t seconds       = totalSeconds % 60,
            minutes       = (totalSeconds / 60) % 60,
            hours         = (totalSeconds / (60 * 60)) % 24,
            days          = totalSeconds / (60 * 60 * 24);
      
   static char timeString[22];   
   
        if (days == 0) snprintf(timeString,  9,          "%02d:%02d:%02d", hours, minutes, seconds);
   else if (days == 1) snprintf(timeString, 22,   "1 Tag, %02d:%02d:%02d", hours, minutes, seconds);    
   else                snprintf(timeString, 22, "%d Tage, %02d:%02d:%02d", days, hours, minutes, seconds);
   
   return timeString;
}


char* getTimeString(time_t timestamp){
   static char timeString[9];   
   snprintf(timeString, 9, "%02d:%02d:%02d", hour(timestamp), minute(timestamp), second(timestamp));
   
   return timeString;   
}


char* getDateString(time_t timestamp){   
   static char dateString[11];
   snprintf(dateString, 11, "%04d-%02d-%02d", year(timestamp), month(timestamp), day(timestamp));
   
   return dateString;
}


char* getWeekdayString(time_t timestamp){   
   static char weekdayString[11];
   
   switch (weekday(timestamp)){
      case 1  : strcpy(weekdayString, "Sonntag");    break;
      case 2  : strcpy(weekdayString, "Montag");     break;
      case 3  : strcpy(weekdayString, "Dienstag");   break;
      case 4  : strcpy(weekdayString, "Mittwoch");   break;
      case 5  : strcpy(weekdayString, "Donnerstag"); break;
      case 6  : strcpy(weekdayString, "Freitag");    break;
      case 7  : strcpy(weekdayString, "Samstag");    break;
      default : strcpy(weekdayString, "Invalid");    break;
   }

   return weekdayString;
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


void logConnectionEvent(const char* message){
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].entry          = connectionLogEntries;
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].uptimeSeconds  = getUptimeSeconds();
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].timestamp      = now();
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].timestampValid = dateValid && timeValid;
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].wlanChannel    = WiFi.channel();
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].message        = message;
   memcpy(connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].wlanBssid, WiFi.BSSID(), 6);
   connectionLogEntries++;
}


void logSwitchEvent(const uint8_t ch, const char* type, const char* message, const uint8_t *ga){
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].entry          = switchLogEntries;
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].uptimeSeconds  = getUptimeSeconds();
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].timestamp      = now();
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].timestampValid = dateValid && timeValid;
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].message        = message;
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].channel        = ch;
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].type           = type;
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].ga             = ga;
   switchLogEntries++;
}
