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

const String   SOFTWARE_VERSION                 = "2019-11-17";

const uint8_t  GA_SWITCH_COUNT                  = sizeof(GA_SWITCH[0]) / 3,
               GA_LOCK_COUNT                    = sizeof(GA_LOCK[0]) / 3,
               KNXD_GROUP_CONNECTION_REQUEST[]  = {0x00, 0x05, EIB_OPEN_GROUPCON >> 8, EIB_OPEN_GROUPCON & 0xFF, 0x00, 0x00, 0x00};

uint8_t        messageLength = 0,
               messageResponse[32];

boolean        connectionConfirmed              = false,
               lockActive[]                     = {false, false, false, false},
               buttonLastState[]                = {true, true, true, true},
               buttonDebouncedState[]           = {true, true, true, true},
               relayStatus[]                    = {false, false, false, false},
               ledBlinkStatus                   = false;

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
               lastTelegramHeaderReceivedMillis = 0;

// WLAN-Client
WiFiClient              client;

// Webserver
ESP8266WebServer        webServer(80);
ESP8266HTTPUpdateServer httpUpdateServer;

const String HTML_HEADER =  "<!DOCTYPE HTML>\n"
                            "<html>\n"
                            "<head>\n"
                               "<meta charset=\"utf-8\"/>\n"
                               "<meta http-equiv=\"refresh\" content=\"15;URL=/\">\n"
                               "<style>\n"
                                  ".green {color:darkgreen; font-weight: bold;}\n"
                                  ".red   {color:darkred; font-weight: bold;}\n"
                                  "table, th, td {border-collapse:collapse; border: 1px solid black;}\n"
                                  "th, td {text-align: left; padding: 2px 10px 2px 10px;}\n"
                                  "A:link    {text-decoration: none;}\n"
                                  "A:visited {text-decoration: none;}\n"
                                  "A:active  {text-decoration: none;}\n"
                                  "A:hover   {text-decoration: underline;}\n" 
                               "</style>\n"
                            "</head>\n"
                            "<body>\n",
      
             HTML_FOOTER =  "</body>\n"
                            "</html>";


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
      Serial.println("\n");
   }
   
   Serial.print("Verbinde mit WLAN '");
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
               sendStatusGA(ch);
         }
         
         if (     messageLength >= 8 // Ein Bit bzw. DPT 1.001
               && messageResponse[0] == EIB_GROUP_PACKET >> 8
               && messageResponse[1] == EIB_GROUP_PACKET & 0xFF){
            
            receivedTelegrams++;  
            lastTelegramReceivedMillis = currentMillis;
            //Serial.print("Received Telegrams: ");
            //Serial.println(receivedTelegrams);
            
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
         
      sendStatusGA(ch);
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


void sendStatusGA(uint8_t ch){
   // Status-GA schreiben
   if (client.connected()){
      const uint8_t groupValueWrite[] = {0x00, 0x06, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, (GA_STATUS[ch][0] << 3) + GA_STATUS[ch][1], GA_STATUS[ch][2], 0x00, 0x80 | relayStatus[ch]};
      client.write(groupValueWrite, sizeof(groupValueWrite));
   }   
}


String getUptimeString(){
   uint32_t secsUp  = (currentMillis / 1000) + (millisOverflows * (0xFFFFFFFF / 1000)),
            seconds = secsUp % 60,
            minutes = (secsUp / 60) % 60,
            hours   = (secsUp / (60 * 60)) % 24,
            days    = secsUp / (60 * 60 * 24);
      
   char timeString[8];
   sprintf(timeString, "%02d:%02d:%02d", hours, minutes, seconds);
   
   if (days == 0)
      return timeString;
   else if (days == 1)
      return "1 Tag, " + String(timeString);
   else
      return String(days) + " Tage, " + timeString;
}


String getWebServerMainPage() {
   const String connectionToolTip   = "title=\""
                                      + String(knxdConnectionCount)        + " total connection" + String(knxdConnectionCount != 1 ? "s" : "") + " / "
                                      + String(missingTelegramTimeouts)    + " missing telegram timeout" + String(missingTelegramTimeouts != 1 ? "s" : "") + " / "
                                      + String(incompleteTelegramTimeouts) + " incomplete telegram timeout" + String(incompleteTelegramTimeouts != 1 ? "s" : "") + "\"";

   String GA_SWITCH_STRING[CHANNELS],
          GA_LOCK_STRING[CHANNELS],
          GA_STATUS_STRING[CHANNELS];

   for (uint8_t ch=0; ch<CHANNELS; ch++){
      for (uint8_t i=0; i<GA_SWITCH_COUNT; i++){
         if (GA_SWITCH[ch][i][0] + GA_SWITCH[ch][i][1] + GA_SWITCH[ch][i][2] > 0)
            GA_SWITCH_STRING[ch] += String(GA_SWITCH[ch][i][0]) + "/" + String(GA_SWITCH[ch][i][1]) + "/" + String(GA_SWITCH[ch][i][2]) + "<br />";
      }

      for (uint8_t i=0; i<GA_LOCK_COUNT; i++){
         if (GA_LOCK[ch][i][0] + GA_LOCK[ch][i][1] + GA_LOCK[ch][i][2] > 0)
            GA_LOCK_STRING[ch] += String(GA_LOCK[ch][i][0]) + "/" + String(GA_LOCK[ch][i][1]) + "/" + String(GA_LOCK[ch][i][2]) + "<br />";
      }

      if (GA_STATUS[ch][0] + GA_STATUS[ch][1] + GA_STATUS[ch][2] > 0)
         GA_STATUS_STRING[ch] += String(GA_STATUS[ch][0]) + "/" + String(GA_STATUS[ch][1]) + "/" + String(GA_STATUS[ch][2]);
   }
   
   return 
         HTML_HEADER + 
         
         "<H1>" + HOST_NAME + " (" + HOST_DESCRIPTION + ")</H1>\n"
         "Firmware: <a href=\"https://github.com/McOrvas/sonoff-knxd\">sonoff-knxd</a> (" + SOFTWARE_VERSION + ")<br />\n"
         "Laufzeit: " + getUptimeString() + "\n"
         
         "<H2>KNX-Status</H2>\n"
         
         "<p>\n"
         + String(client.connected()
            ? "<div class=\"green\" " + connectionToolTip + ">Das Modul ist mit dem EIBD/KNXD verbunden!</div>\n"
            : "<div class=\"red\" " + connectionToolTip + ">Das Modul ist nicht mit dem EIBD/KNXD verbunden!</div>\n"
           ) +         
         "</p>\n"
         
         "<table>\n"
         "<tr>"
         "<th>Gruppenadressen</th>"
         "<th>Kanal 1</th>"
         + String(CHANNELS > 1 ? "<th>Kanal 2</th>" : "")
         + String(CHANNELS > 2 ? "<th>Kanal 3</th>" : "")
         + String(CHANNELS > 3 ? "<th>Kanal 4</th>" : "") +
         "</tr>\n"
         
         "<tr><td>Schalten</td>"
         "<td>" + GA_SWITCH_STRING[0] + "</td>"
         + String(CHANNELS > 1 ? "<td>" + GA_SWITCH_STRING[1] + "</td>" : "")
         + String(CHANNELS > 2 ? "<td>" + GA_SWITCH_STRING[2] + "</td>" : "")
         + String(CHANNELS > 3 ? "<td>" + GA_SWITCH_STRING[3] + "</td>" : "") +
         "</tr>\n"
         
         "<tr><td>Sperren</td>"
         "<td>" + GA_LOCK_STRING[0] + "</td>"
         + String(CHANNELS > 1 ? "<td>" + GA_LOCK_STRING[1] + "</td>" : "")
         + String(CHANNELS > 2 ? "<td>" + GA_LOCK_STRING[2] + "</td>" : "")
         + String(CHANNELS > 3 ? "<td>" + GA_LOCK_STRING[3] + "</td>" : "") +
         "</tr>\n"
         
         "<tr><td>Status</td>"
         "<td>" + GA_STATUS_STRING[0] + "</td>"
         + String(CHANNELS > 1 ? "<td>" + GA_STATUS_STRING[1] + "</td>" : "")
         + String(CHANNELS > 2 ? "<td>" + GA_STATUS_STRING[2] + "</td>" : "")
         + String(CHANNELS > 3 ? "<td>" + GA_STATUS_STRING[3] + "</td>" : "") +
         "</tr>\n"
         "</table>\n"
         
         "<H2>Schaltstatus</H2>\n"         
         
         "<table>\n"
         "<tr>"
         "<th>Status</th>"
         "<th>Kanal 1</th>"
         + String(CHANNELS > 1 ? "<th>Kanal 2</th>" : "")
         + String(CHANNELS > 2 ? "<th>Kanal 3</th>" : "")
         + String(CHANNELS > 3 ? "<th>Kanal 4</th>" : "") +
         "</tr>\n"
         
         "<tr><td>Schaltstatus</td>"
         "<td><a href=\"ch1/toggle\" " + String(relayStatus[0] ? "class=\"green\">eingeschaltet" : "class=\"red\">ausgeschaltet") + "</a></td>"
         + String(CHANNELS > 1 ? "<td><a href=\"ch2/toggle\" " + String(relayStatus[1] ? "class=\"green\">eingeschaltet" : "class=\"red\">ausgeschaltet") + "</a></td>" : "")
         + String(CHANNELS > 2 ? "<td><a href=\"ch3/toggle\" " + String(relayStatus[2] ? "class=\"green\">eingeschaltet" : "class=\"red\">ausgeschaltet") + "</a></td>" : "")
         + String(CHANNELS > 3 ? "<td><a href=\"ch4/toggle\" " + String(relayStatus[3] ? "class=\"green\">eingeschaltet" : "class=\"red\">ausgeschaltet") + "</a></td>" : "") +
         "</tr>\n"
         
         "<tr><td>Sperre</td>"
         "<td><a href=\"ch1/toggleLock\" " + String(lockActive[0] ? "class=\"red\">gesperrt" : "class=\"green\">freigegeben") + "</a></td>"
         + String(CHANNELS > 1 ? "<td><a href=\"ch2/toggleLock\" " + String(lockActive[1] ? "class=\"red\">gesperrt" : "class=\"green\">freigegeben") + "</a></td>" : "")
         + String(CHANNELS > 2 ? "<td><a href=\"ch3/toggleLock\" " + String(lockActive[2] ? "class=\"red\">gesperrt" : "class=\"green\">freigegeben") + "</a></td>" : "")
         + String(CHANNELS > 3 ? "<td><a href=\"ch4/toggleLock\" " + String(lockActive[3] ? "class=\"red\">gesperrt" : "class=\"green\">freigegeben") + "</a></td>" : "") +
         "</tr>\n"         
         "</table>\n" 
         
         "<H2>WLAN-Status</H2>\n"
         "<table>\n"
         "<tr><td>IP-Adresse</td><td>" + WiFi.localIP().toString() + "</td></tr>\n"
         "<tr><td>Netzmaske</td><td>" + WiFi.subnetMask().toString() + "</td></tr>\n"
         "<tr><td>Gateway</td><td>" + WiFi.gatewayIP().toString() + "</td></tr>\n"
         "<tr><td>MAC</td><td>" + WiFi.macAddress() + "</td></tr>\n"
         "<tr><td>SSID</td><td>" + String(WiFi.SSID()) + "</td></tr>\n"
         "<tr><td>RSSI</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>\n"
         "</table>\n"
               
         "<H2>Ger&auml;tewartung</H2>\n"

         "<p><a href=\"update\">Software aktualisieren</a></p>\n"
         "<p><a href=\"reboot\">Ger&auml;t neustarten</a></p>\n"
         
         + HTML_FOOTER;      
}


void setupWebServer(){   
   webServer.on("/", [](){
      webServer.send(200, "text/html", getWebServerMainPage());
   });  
   
   webServer.on("/ch1/on", [](){
      switchRelay(0, true, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/on", [](){
      switchRelay(1, true, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/on", [](){
      switchRelay(2, true, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/on", [](){
      switchRelay(3, true, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/off", [](){
      switchRelay(0, false, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });   
   
   webServer.on("/ch2/off", [](){
      switchRelay(1, false, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/off", [](){
      switchRelay(2, false, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/off", [](){
      switchRelay(3, false, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });  
   
   webServer.on("/ch1/toggle", [](){
      switchRelay(0, !relayStatus[0], false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/toggle", [](){
      switchRelay(1, !relayStatus[1], false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/toggle", [](){
      switchRelay(2, !relayStatus[2], false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/toggle", [](){
      switchRelay(3, !relayStatus[3], false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/lock", [](){
      lockRelay(0, true);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/lock", [](){
      lockRelay(1, true);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/lock", [](){
      lockRelay(2, true);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/lock", [](){
      lockRelay(3, true);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/unlock", [](){
      lockRelay(0, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/unlock", [](){
      lockRelay(1, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/unlock", [](){
      lockRelay(2, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/unlock", [](){
      lockRelay(3, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/toggleLock", [](){
      lockRelay(0, !lockActive[0]);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/toggleLock", [](){
      lockRelay(1, !lockActive[1]);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/toggleLock", [](){
      lockRelay(2, !lockActive[2]);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/toggleLock", [](){
      lockRelay(3, !lockActive[3]);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/state", [](){webServer.send(200, "text/plain", relayStatus[0] ? "on" : "off");}); 
   webServer.on("/ch2/state", [](){webServer.send(200, "text/plain", relayStatus[1] ? "on" : "off");}); 
   webServer.on("/ch3/state", [](){webServer.send(200, "text/plain", relayStatus[2] ? "on" : "off");}); 
   webServer.on("/ch4/state", [](){webServer.send(200, "text/plain", relayStatus[3] ? "on" : "off");}); 
   
   webServer.on("/reboot", [](){
      webServer.send(200, "text/html", HTML_HEADER + "Modul wird neugestartet...<p><a href=\"..\">Zur&uuml;ck</a></p>\n" + HTML_FOOTER);
      delay(1000);
      ESP.restart();
   });
   
   httpUpdateServer.setup(&webServer);
   
   webServer.begin();
   Serial.println("Webserver gestartet");   
}
