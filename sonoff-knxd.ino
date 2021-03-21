/*
 * Notwendige Einstellungen in der Arduino IDE
 * 
 * Boardverwalter-URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json
 * 
 * Sonoff S20
 * Board:              Generic ESP8266 Module
 * Flash Mode:         DOUT
 * Flash Size:         1M (64K SPIFFS) [Notwendig für Updates über die Weboberfläche]
 * 
 * Sonoff 4CH
 * Board:              Generic ESP8285 Module
 * Flash Size:         1M (64K SPIFFS) [Notwendig für Updates über die Weboberfläche]
 */

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


/* 
 * *************************
 * *** Interne Variablen *** 
 * *************************
 */

const char     *SOFTWARE_VERSION                      = "2021-03-21",

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
               dateMonth     = 0,
               dateYear      = 0;

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
               incompleteTelegramTimeoutEnabled = false;

uint32_t       knxdConnectionInitiatedCount     = 0,
               knxdConnectionFailedCount        = 0,
               knxdConnectionHandshakeTimeouts  = 0,
               knxdConnectionConfirmedCount     = 0,
               receivedTelegrams                = 0,
               missingTelegramTimeouts          = 0,
               incompleteTelegramTimeouts       = 0,
               wifiDisconnections               = 0,
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
               wifiDisconnectedMillis           = WIFI_CONNECTION_LOST_DELAY_S * 1000;

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
   boolean     timeValid,
               dateValid;
   uint32_t    entry,
               uptimeSeconds,
               timeSeconds;
   int32_t     wlanChannel;
   uint8_t     dateDay,
               dateMonth,
               dateYear,
               wlanBssid[6];
} connectionLogRingbuffer[LOG_SIZE];

struct logSwitchEvent {
   const uint8_t *ga;
   const char    *type,
                 *message;
   uint8_t       channel;   
   boolean       timeValid,
                 dateValid;
   uint32_t      entry,
                 uptimeSeconds,
                 timeSeconds;
   uint8_t       dateDay,
                 dateMonth,
                 dateYear;
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
    logConnectionEvent(LOG_WLAN_DHCP_COMPLETED);
    Serial.print("IP-Adresse: ");
    Serial.println(WiFi.localIP());

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
   Serial.print(": ");
   Serial.println(event.reason);
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
      
      // Webserver 
      webServer.handleClient();
      
      // Falls eine Verbindung zum EIBD/KNXD aufgebaut ist, blinkt die LED sofern gewünscht.
      if (LED_BLINKS_WHEN_CONNECTED)
         ledBlink();
   }
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
            if (      messageLength == 8 // Ein Bit bzw. DPT 1.001
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
            else if (messageLength == 8 // Ein Bit bzw. DPT 1.001
                  && messageResponse[6] == 0x00
                  && messageResponse[7] == 0x00){
               
               // Die übertragene Gruppenadresse denen aller Kanäle vergleichen
               for (uint8_t ch=0; ch<CHANNELS; ch++){
                  if (GA_STATUS[ch][0] + GA_STATUS[ch][1] + GA_STATUS[ch][2] > 0
                      && messageResponse[4] == (GA_STATUS[ch][0] << 3) + GA_STATUS[ch][1] && messageResponse[5] == GA_STATUS[ch][2]) {
                     Serial.print("Leseanforderung Status Kanal ");
                     Serial.println(ch + 1);
                     
                     const uint8_t groupValueResponse[] = {0x00, 0x06, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, (GA_STATUS[ch][0] << 3) + GA_STATUS[ch][1], GA_STATUS[ch][2], 0x00, 0x40 | relayStatus[ch]};
                     client.write(groupValueResponse, sizeof(groupValueResponse));
                  }
               }
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
                  dateValid = true;
                  dateTelegramReceivedMillis = currentMillis;
                  
                  Serial.print("Date telegram received: ");
                  Serial.println(getDateString(dateYear, dateMonth, dateDay));
               }
               
               // Time telegram
               if (GA_TIME_VALID && messageResponse[4] == (GA_TIME[0] << 3) + GA_TIME[1] && messageResponse[5] == GA_TIME[2]) {
                  
                  timeWeekday = messageResponse[8] >> 5;
                  timeHours   = messageResponse[8] & 0x1F;
                  timeMinutes = messageResponse[9] & 0x3F;
                  timeSeconds = messageResponse[10] & 0x3F;  
                  timeValid   = true;
                  timeTelegramReceivedMillis = currentMillis;
                  
                  Serial.print("Time telegram received: ");
                  Serial.println(getTimeString(timeWeekday, timeHours, timeMinutes, timeSeconds));
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


char* getTimeString(uint8_t weekday, uint8_t hours, uint8_t minutes, uint8_t seconds){
   static char timeString[22];
   
        if (weekday == 1) snprintf(timeString, 22, "%02d:%02d:%02d (Montag)",     hours, minutes, seconds);
   else if (weekday == 2) snprintf(timeString, 22, "%02d:%02d:%02d (Dienstag)",   hours, minutes, seconds);
   else if (weekday == 3) snprintf(timeString, 22, "%02d:%02d:%02d (Mittwoch)",   hours, minutes, seconds);
   else if (weekday == 4) snprintf(timeString, 22, "%02d:%02d:%02d (Donnerstag)", hours, minutes, seconds);
   else if (weekday == 5) snprintf(timeString, 22, "%02d:%02d:%02d (Freitag)",    hours, minutes, seconds);
   else if (weekday == 6) snprintf(timeString, 22, "%02d:%02d:%02d (Samstag)",    hours, minutes, seconds);
   else if (weekday == 7) snprintf(timeString, 22, "%02d:%02d:%02d (Sonntag)",    hours, minutes, seconds);
   else                   snprintf(timeString,  9, "%02d:%02d:%02d",              hours, minutes, seconds);
      
   return timeString;   
}


char* getTimeString(uint32_t totalSeconds){
   uint32_t seconds       = totalSeconds % 60,
            minutes       = (totalSeconds / 60) % 60,
            hours         = (totalSeconds / (60 * 60)) % 24,
            days          = totalSeconds / (60 * 60 * 24),
            weekday       = days == 0 ? 0 : ((days - 1) % 7) + 1;
   
   return getTimeString(weekday, hours, minutes, seconds);
}


uint32_t getUpdatedTimeSeconds(){
   uint32_t seconds = timeWeekday * 86400 + ((timeHours * 3600) + (timeMinutes * 60) + timeSeconds + ((currentMillis - timeTelegramReceivedMillis) / 1000));
   
   // Invalid weekday
   if (timeWeekday == 0)
      seconds = seconds % 86400;
   
   return seconds;
}


char* getDateString(uint8_t year, uint8_t month, uint8_t day){   
   static char dateString[11];
   snprintf(dateString, 11, "%04d-%02d-%02d", year >= 90 ? 1900 + year : 2000 + year, month, day);
   return dateString;
}


void logConnectionEvent(const char* message){
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].entry         = connectionLogEntries;
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].timeValid     = timeValid;
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].timeSeconds   = getUpdatedTimeSeconds();
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].dateValid     = dateValid;   
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].dateDay       = dateDay;
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].dateMonth     = dateMonth;
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].dateYear      = dateYear;
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].uptimeSeconds = getUptimeSeconds();
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].wlanChannel   = WiFi.channel();
   connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].message       = message;
   memcpy(connectionLogRingbuffer[connectionLogEntries % LOG_SIZE].wlanBssid, WiFi.BSSID(), 6);
   connectionLogEntries++;
}


void logSwitchEvent(const uint8_t ch, const char* type, const char* message, const uint8_t *ga){
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].entry         = switchLogEntries;
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].timeValid     = timeValid;
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].timeSeconds   = getUpdatedTimeSeconds();
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].dateValid     = dateValid;   
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].dateDay       = dateDay;
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].dateMonth     = dateMonth;
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].dateYear      = dateYear;
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].uptimeSeconds = getUptimeSeconds();
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].message       = message;
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].channel       = ch;
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].type          = type;
   switchLogRingbuffer[switchLogEntries % LOG_SIZE].ga            = ga;
   switchLogEntries++;
}
