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

const String   SOFTWARE_VERSION                 = "2019-12-25";

const uint8_t  GA_SWITCH_COUNT                  = sizeof(GA_SWITCH[0]) / 3,
               GA_LOCK_COUNT                    = sizeof(GA_LOCK[0]) / 3,
               KNXD_GROUP_CONNECTION_REQUEST[]  = {0x00, 0x05, EIB_OPEN_GROUPCON >> 8, EIB_OPEN_GROUPCON & 0xFF, 0x00, 0x00, 0x00};

const boolean  GA_DATE_VALID = GA_DATE[0] + GA_DATE[1] + GA_DATE[2] > 0,
               GA_TIME_VALID = GA_TIME[0] + GA_TIME[1] + GA_TIME[2] > 0;

uint8_t        messageLength = 0,
               messageResponse[32],
               timeWeekday = 0,
               timeHours   = 0,
               timeMinutes = 0,
               timeSeconds = 0,
               dateDay     = 0,
               dateMonth   = 0,
               dateYear    = 0;

boolean        connectionConfirmed              = false,
               lockActive[]                     = {false, false, false, false},
               buttonLastState[]                = {true, true, true, true},
               buttonDebouncedState[]           = {true, true, true, true},
               relayStatus[]                    = {false, false, false, false},
               ledBlinkStatus                   = false,
               timeValid                        = false,
               dateValid                        = false;

uint32_t       knxdConnectionCount              = 0,
               receivedTelegrams                = 0,
               missingTelegramTimeouts          = 0,
               incompleteTelegramTimeouts       = 0,
               
// Variablen zur Zeitmessung
               currentMillis                    = 0,
               millisOverflows                  = 0,
               buttonDebounceMillis[]           = {0, 0, 0, 0},
               autoOffTimerStartMillis[]        = {0, 0, 0, 0},
               connectionEstablishedMillis      = 0,
               connectionFailedMillis           = 0,
               ledBlinkLastSwitch               = 0,
               lastTelegramReceivedMillis       = 0,
               lastTelegramHeaderReceivedMillis = 0,
               timeTelegramReceivedMillis       = 0,
               dateTelegramReceivedMillis       = 0;

// WLAN-Client
WiFiClient              client;

// Webserver
ESP8266WebServer        webServer(80);
ESP8266HTTPUpdateServer httpUpdateServer;


void setup() {
   Serial.begin(115200);
   delay(10);
   
   pinMode(GPIO_LED, OUTPUT);
   digitalWrite(GPIO_LED, !relayStatus[0]);
   
   Serial.print(HOST_NAME);
   Serial.print(" (");
   Serial.print(HOST_DESCRIPTION);
   Serial.println(")\n");
   
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
      
      Serial.println("GA Zeit:              " + String(GA_TIME[0]) + "/" + String(GA_TIME[1]) + "/" + String(GA_TIME[2]));
      Serial.println("GA Datum:             " + String(GA_DATE[0]) + "/" + String(GA_DATE[1]) + "/" + String(GA_DATE[2]));
   }
   
   Serial.print("\nVerbinde mit WLAN '");
   Serial.print(SSID);
   Serial.print("'");
   
   /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
   *   would try to act as both a client and an access-point and could cause
   *   network-issues with your other WiFi-devices on your WiFi-network. */
   WiFi.mode(WIFI_STA);
   WiFi.hostname(HOST_NAME);
   WiFi.persistent(false);
   WiFi.begin(SSID, PASSWORD);
   
   while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
   }
   
   Serial.println();
   Serial.print("Verbindung hergestellt. IP-Adresse: ");
   Serial.println(WiFi.localIP());
      
   setupWebServer();
   
   connectToKnxd();
   
   // Initialize telegram timeout timer
   lastTelegramReceivedMillis = millis();
}


void loop() {
   // Überlauf von millis()
   if (currentMillis > millis()){
      millisOverflows++;
   }
   currentMillis = millis();
   
   // Webserver 
   webServer.handleClient();

   for (uint8_t ch=0; ch<CHANNELS; ch++){
      // Check hardware buttons
      checkButton(ch);

      // Check if a channel has to be switched off by a timer
      if (relayStatus[ch] && AUTO_OFF_TIMER_S[ch] > 0 && autoOffTimerStartMillis[ch] > 0 && (currentMillis - autoOffTimerStartMillis[ch]) >= (AUTO_OFF_TIMER_S[ch] * 1000)){
         Serial.print("Auto off timer for channel ");
         Serial.print(ch + 1);
         Serial.println(" has expired!");
         // Set value to 0 even if the channel cannot be switched off due to an active lock.
         autoOffTimerStartMillis[ch] = 0;
         switchRelay(ch, false, AUTO_OFF_TIMER_OVERRIDES_LOCK);
      }
   }

   // KNX-Kommunikation
   knxLoop();
   
   // Falls eine Verbindung zum EIBD/KNXD aufgebaut ist, blinkt die LED sofern gewünscht.
   if (LED_BLINKS_WHEN_CONNECTED)
      ledBlink();
}


void knxLoop(){
   // Falls die Verbindung zum EIBD/KNXD unterbrochen ist, diese hier neu aufbauen
   if (!client.connected() || (!connectionConfirmed && (currentMillis - connectionEstablishedMillis) >= CONNECTION_CONFIRMATION_TIMEOUT_MS)){
      // Beim ersten Ausfall der Verbindung
      if (connectionFailedMillis == 0){
         connectionFailedMillis = currentMillis;
         connectionConfirmed = false;
         
         Serial.print("Verbindung unterbrochen. Neuaufbau in ");
         Serial.print(CONNECTION_LOST_DELAY_S);
         Serial.println(" s.");
      }      

      // Pause zwischen zwei Verbindungsversuchen abgelaufen
      if ((currentMillis - connectionFailedMillis) >= CONNECTION_LOST_DELAY_S * 1000){
         if (connectToKnxd())
            connectionFailedMillis = 0;
         else
            connectionFailedMillis = currentMillis;
      }
      
      // Reset telegram timeout timer
      lastTelegramReceivedMillis = currentMillis;
   }
   
   // Die Verbindung steht prinzipiell, aber seit MISSING_TELEGRAM_TIMEOUT_MIN wurde kein Telegramm mehr empfangen und deshalb wird ein Timeout ausgelöst
   else if (MISSING_TELEGRAM_TIMEOUT_MIN > 0 && (currentMillis - lastTelegramReceivedMillis) >= MISSING_TELEGRAM_TIMEOUT_MIN * 60000){
      client.stopAll();
      missingTelegramTimeouts++;
      
      Serial.print("Timeout: No telegram received during ");
      Serial.print(MISSING_TELEGRAM_TIMEOUT_MIN);
      Serial.println(" minutes");
   }
   
   // Die Verbindung ist etabliert
   else {
      // Die Länge einer neuen Nachricht lesen
      if (messageLength == 0 && client.available() >= 2){
         messageLength = (((int) client.read()) << 8) + client.read();
         lastTelegramHeaderReceivedMillis = currentMillis;
      }
      
      // Die Nutzdaten einer Nachricht lesen
      if (messageLength > 0 && client.available() >= messageLength){
         client.read(messageResponse, messageLength);
         
         // Prüfen, ob der initiale Verbindungsaufbau korrekt bestätigt wurde      
         if (!connectionConfirmed && messageLength == 2 && messageResponse[0] == KNXD_GROUP_CONNECTION_REQUEST[2] && messageResponse[1] == KNXD_GROUP_CONNECTION_REQUEST[3]){
            Serial.println("EIBD/KNXD Verbindung hergestellt");
            connectionConfirmed = true;   
            knxdConnectionCount++;
            
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
                        switchRelay(ch, messageResponse[7] & 0x0F, false);
                     }
                  }
                  for (uint8_t i=0; i<GA_LOCK_COUNT; i++){
                     // Sperren
                     if (GA_LOCK[ch][i][0] + GA_LOCK[ch][i][1] + GA_LOCK[ch][i][2] > 0
                         && messageResponse[4] == (GA_LOCK[0][i][0] << 3) + GA_LOCK[0][i][1]
                         && messageResponse[5] == GA_LOCK[0][i][2]){
                        lockRelay(ch, (messageResponse[7] & 0x0F) ^ LOCK_INVERTED[ch]);
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
                     Serial.println(ch+1);
            
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
                  
                  Serial.println("Date telegram received: " + getDateString());
               }
               
               // Time telegram
               if (GA_TIME_VALID && messageResponse[4] == (GA_TIME[0] << 3) + GA_TIME[1] && messageResponse[5] == GA_TIME[2]) {
                  
                  timeWeekday = messageResponse[8] >> 5;
                  timeHours   = messageResponse[8] & 0x1F;
                  timeMinutes = messageResponse[9] & 0x3F;
                  timeSeconds = messageResponse[10] & 0x3F;  
                  timeValid   = true;
                  timeTelegramReceivedMillis = currentMillis;
                  
                  Serial.println("Time telegram received: " + getTimeString(timeWeekday, timeHours, timeMinutes, timeSeconds));
               }
            }
         }
         
         // Für die nächste Nachricht zurücksetzen
         messageLength = 0;
      }
      
      // Incomplete telegram received timeout
      else if (INCOMPLETE_TELEGRAM_TIMEOUT_MS > 0 && messageLength > 0 && lastTelegramHeaderReceivedMillis > 0 && (currentMillis - lastTelegramHeaderReceivedMillis) >= INCOMPLETE_TELEGRAM_TIMEOUT_MS){
         client.stopAll();
         incompleteTelegramTimeouts++;
         messageLength = 0;
         
         Serial.print("Timeout: Incomplete received telegram after ");
         Serial.print(INCOMPLETE_TELEGRAM_TIMEOUT_MS);
         Serial.println(" ms");
      }
   }
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
            switchRelay(ch, !relayStatus[ch], false);
         else
            switchRelay(ch, true, false);
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
            switchRelay(ch, false, false);
      }
      
      buttonLastState[ch] = !BUTTON_INVERTED;
   }
}


void ledBlink(){
   // Falls die Verbindung steht, die LED blinken lassen.
   if (connectionConfirmed) {
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


boolean connectToKnxd(){
   connectionConfirmed = false;
   client.stopAll();
   
   Serial.print("Verbinde mit EIBD/KNXD auf ");
   Serial.println(KNXD_IP);
   
   if (!client.connect(KNXD_IP, KNXD_PORT)) {
      Serial.println("Verbindung fehlgeschlagen!");
      return false;
   }
   else{      
      client.setNoDelay(true);
      client.keepAlive(KA_IDLE_S, KA_INTERVAL_S, KA_RETRY_COUNT);      
      client.write(KNXD_GROUP_CONNECTION_REQUEST, sizeof(KNXD_GROUP_CONNECTION_REQUEST));
      
      connectionEstablishedMillis = millis();      
      
      return client.connected();
   }
}


void switchRelay(uint8_t ch, boolean on, boolean overrideLock){
   if (ch >= CHANNELS){
      Serial.print("Ungültiger Kanal: ");
      Serial.println(ch);      
   }
   else if (!overrideLock && lockActive[ch]){
      Serial.print("Schaltkommando wird ignoriert, Kanal ");
      Serial.print(ch+1);
      Serial.println(" ist gesperrt!");
   }
   else if (relayStatus[ch] != on){
      relayStatus[ch] = on;
      
      Serial.print("Kanal ");
      Serial.print(ch + 1);
      
      if (on){
         autoOffTimerStartMillis[ch] = currentMillis;
         Serial.println(" wird eingeschaltet");
      }
      else{
         autoOffTimerStartMillis[ch] = 0;
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


void lockRelay(uint8_t ch, boolean lock){
   if (ch >= CHANNELS){
      Serial.print("Ungültiger Kanal: ");
      Serial.println(ch);      
   }
   else {      
      if (lock && !lockActive[ch]){
         Serial.print("Kanal ");
         Serial.print(ch + 1);
         Serial.println(" wird gesperrt!");
         
         if (SWITCH_OFF_WHEN_LOCKED[ch])
            switchRelay(ch, false, false);
         else if (SWITCH_ON_WHEN_LOCKED[ch])
            switchRelay(ch, true, false);
         
         lockActive[ch] = true;
      }                  
      else if (!lock && lockActive[ch]){
         lockActive[ch] = false;
         
         Serial.print("Kanal ");
         Serial.print(ch + 1);
         Serial.println(" wird entsperrt!");
         
         if (SWITCH_OFF_WHEN_UNLOCKED[ch])
            switchRelay(ch, false, false);
         else if (SWITCH_ON_WHEN_UNLOCKED[ch])
            switchRelay(ch, true, false);
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


String getUptimeString(uint32_t totalSeconds){
   uint32_t seconds       = totalSeconds % 60,
            minutes       = (totalSeconds / 60) % 60,
            hours         = (totalSeconds / (60 * 60)) % 24,
            days          = totalSeconds / (60 * 60 * 24);
      
   char timeString[8];
   sprintf(timeString, "%02d:%02d:%02d", hours, minutes, seconds);
   
   if (days == 0)
      return timeString;
   else if (days == 1)
      return "1 Tag, " + String(timeString);
   else
      return String(days) + " Tage, " + timeString;
}


String getTimeString(uint8_t weekday, uint8_t hours, uint8_t minutes, uint8_t seconds){
   char timeString[8];
   sprintf(timeString, "%02d:%02d:%02d", hours, minutes, seconds);                  
   
   String dayString;
   
        if (weekday == 1) dayString = "Montag";
   else if (weekday == 2) dayString = "Dienstag";
   else if (weekday == 3) dayString = "Mittwoch";
   else if (weekday == 4) dayString = "Donnerstag";
   else if (weekday == 5) dayString = "Freitag";
   else if (weekday == 6) dayString = "Samstag";
   else if (weekday == 7) dayString = "Sonntag";
   
   // Check if the weekday is valid
   if (timeWeekday == 0)
      return timeString;
   else
      return String(timeString) + " (" + dayString + ")";
}


String getUpdatedTimeString(){
   uint32_t totalSeconds  = (timeHours * 3600) + (timeMinutes * 60) + timeSeconds + ((currentMillis - timeTelegramReceivedMillis) / 1000),
            seconds       = totalSeconds % 60,
            minutes       = (totalSeconds / 60) % 60,
            hours         = (totalSeconds / (60 * 60)) % 24,
            daysOverflows = totalSeconds / (60 * 60 * 24),
            weekday       = (timeWeekday - 1 + daysOverflows) % 7;
   
   return getTimeString(weekday + 1, hours, minutes, seconds);
}


String getDateString(){   
   char dateString[10];
   sprintf(dateString, "%04d-%02d-%02d", dateYear >= 90 ? 1900 + dateYear : 2000 + dateYear, dateMonth, dateDay);
   return dateString;
}
