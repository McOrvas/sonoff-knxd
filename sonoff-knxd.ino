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

const uint8_t  GA_SWITCH_BYTE[][3] = {{(GA_SWITCH[0][0] << 3) + GA_SWITCH[0][1], GA_SWITCH[0][2]},
                                      {(GA_SWITCH[1][0] << 3) + GA_SWITCH[1][1], GA_SWITCH[1][2]},
                                      {(GA_SWITCH[2][0] << 3) + GA_SWITCH[2][1], GA_SWITCH[2][2]},
                                      {(GA_SWITCH[3][0] << 3) + GA_SWITCH[3][1], GA_SWITCH[3][2]}},

               GA_LOCK_BYTE[][3]   = {{(GA_LOCK[0][0] << 3) + GA_LOCK[0][1], GA_LOCK[0][2]},
                                      {(GA_LOCK[1][0] << 3) + GA_LOCK[1][1], GA_LOCK[1][2]},
                                      {(GA_LOCK[2][0] << 3) + GA_LOCK[2][1], GA_LOCK[2][2]},
                                      {(GA_LOCK[3][0] << 3) + GA_LOCK[3][1], GA_LOCK[3][2]}},

               GA_STATUS_BYTE[][3] = {{(GA_STATUS[0][0] << 3) + GA_STATUS[0][1], GA_STATUS[0][2]},
                                      {(GA_STATUS[1][0] << 3) + GA_STATUS[1][1], GA_STATUS[1][2]},
                                      {(GA_STATUS[2][0] << 3) + GA_STATUS[2][1], GA_STATUS[2][2]},
                                      {(GA_STATUS[3][0] << 3) + GA_STATUS[3][1], GA_STATUS[3][2]}},

               KNXD_GROUP_CONNECTION_REQUEST[] = {0x00, 0x05, EIB_OPEN_GROUPCON >> 8, EIB_OPEN_GROUPCON & 0xFF, 0x00, 0x00, 0x00};

boolean        connectionConfirmed = false,
               lockActive[]        = {false, false, false, false},
               buttonPressed[]     = {false, false, false, false},
               relayStatus[]       = {false, false, false, false},
               ledBlinkStatus      = false;

uint8_t        messageLength = 0,
               messageResponse[32];

uint32_t       knxdConnectionCount         = 0,
               
// Variablen zur Zeitmessung
               currentMillis               = 0,
               millisOverflows             = 0,
               buttonPressedMillis[]       = {0, 0, 0, 0},
               connectionEstablishedMillis = 0,
               connectionFailedMillis      = 0,
               ledBlinkLastSwitch          = 0;

// WLAN-Client
WiFiClient              client;

// Webserver
ESP8266WebServer        webServer(80);
ESP8266HTTPUpdateServer httpUpdateServer;

const String HTML_HEADER =  "<!DOCTYPE HTML>\n"
                            "<html>\n"
                            "<head>\n"
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
      digitalWrite(GPIO_RELAY[ch], relayStatus[ch]);
      
      Serial.print("Kanal ");
      Serial.print(ch + 1);      
      Serial.print(" GA schalten: ");
      Serial.print(GA_SWITCH[ch][0]);
      Serial.print("/");
      Serial.print(GA_SWITCH[ch][1]);
      Serial.print("/");
      Serial.print(GA_SWITCH[ch][2]);
      Serial.println();
      
      Serial.print("Kanal ");
      Serial.print(ch + 1);
      Serial.print(" GA sperren:  ");
      Serial.print(GA_LOCK[ch][0]);
      Serial.print("/");
      Serial.print(GA_LOCK[ch][1]);
      Serial.print("/");
      Serial.print(GA_LOCK[ch][2]);
      Serial.println();
      
      Serial.print("Kanal ");
      Serial.print(ch + 1);
      Serial.print(" GA Status:   ");
      Serial.print(GA_STATUS[ch][0]);
      Serial.print("/");
      Serial.print(GA_STATUS[ch][1]);
      Serial.print("/");
      Serial.print(GA_STATUS[ch][2]);
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
}


void loop() {
   // Überlauf von millis()
   if (currentMillis > millis()){
      millisOverflows++;
   }
   currentMillis = millis();
   
   // Webserver 
   webServer.handleClient();
   
   // Check hardware buttons
   for (uint8_t ch=0; ch<CHANNELS; ch++)
      checkButton(ch);
   
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
   }
   
   // Die Verbindung ist etabliert
   else {
      // Die Länge einer neuen Nachricht lesen
      if (messageLength == 0 && client.available() >= 2)
         messageLength = (((int) client.read()) << 8) + client.read();
      
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
            
            // Schalten oder sperren
            if (      messageLength == 8 // Ein Bit bzw. DPT 1.001
                  &&  messageResponse[6] == 0x00
                  && (messageResponse[7] & 0x80) == 0x80){
               
               // Die übertragene Gruppenadresse denen aller Kanäle vergleichen
               for (uint8_t ch=0; ch<CHANNELS; ch++){
                  // Schalten
                  if (messageResponse[4] == GA_SWITCH_BYTE[ch][0] && messageResponse[5] == GA_SWITCH_BYTE[ch][1]){
                     switchRelay(ch, messageResponse[7] & 0x0F);      
                  }
                  
                  // Sperren
                  else if (messageResponse[4] == GA_LOCK_BYTE[ch][0] && messageResponse[5] == GA_LOCK_BYTE[ch][1]){  
                     lockRelay(ch, messageResponse[7] & 0x0F);                     
                  }
               }
            }
            
            // Status lesen
            else if (messageLength == 8 // Ein Bit bzw. DPT 1.001
                  && messageResponse[6] == 0x00
                  && messageResponse[7] == 0x00){
               
               // Die übertragene Gruppenadresse denen aller Kanäle vergleichen
               for (uint8_t ch=0; ch<CHANNELS; ch++){
                  if (messageResponse[4] == GA_STATUS_BYTE[ch][0] && messageResponse[5] == GA_STATUS_BYTE[ch][1]) {
                     Serial.print("Leseanforderung Status Kanal ");
                     Serial.println(ch+1);
            
                     const uint8_t groupValueResponse[] = {0x00, 0x06, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, GA_STATUS_BYTE[ch][0], GA_STATUS_BYTE[ch][1], 0x00, 0x40 | relayStatus[ch]};
                     client.write(groupValueResponse, sizeof(groupValueResponse));
                  }
               }
            }
         }         
         
         // Für die nächste Nachricht zurücksetzen
         messageLength = 0;
      }
   }
}


void checkButton(uint8_t ch){
   // true = nicht gedrückt, false = gedrückt
   if (!digitalRead(GPIO_BUTTON[ch])){
      if (buttonPressedMillis[ch] == 0)
         buttonPressedMillis[ch] = currentMillis;
      
      else if (!buttonPressed[ch] && (currentMillis - buttonPressedMillis[ch]) >= BUTTON_DEBOUNCING_TIME_MS) {
         buttonPressed[ch] = true;
         switchRelay(ch, !relayStatus[ch]);
      }      
   }
   else {
      buttonPressed[ch] = false;
      buttonPressedMillis[ch] = 0;
   }   
}


void ledBlink(){
   // Falls die Verbindung steht, die LED blinken lassen.
   if (connectionConfirmed){
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


void switchRelay(uint8_t ch, boolean on){
   if (ch >= CHANNELS){
      Serial.print("Ungültiger Kanal: ");
      Serial.println(ch);      
   }
   else if (lockActive[ch]){
      Serial.print("Schaltkommando wird ignoriert, Kanal ");
      Serial.print(ch+1);
      Serial.println(" ist gesperrt!");
   }
   else{
      relayStatus[ch] = on;
      
      Serial.print("Kanal ");
      Serial.print(ch + 1);
      
      if (on)
         Serial.println(" wird eingeschaltet");
      else
         Serial.println(" wird ausgeschaltet");
      
      digitalWrite(GPIO_RELAY[ch], relayStatus[ch]);   
      
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
      if (lock){
         Serial.print("Kanal ");
         Serial.print(ch + 1);
         Serial.println(" wird gesperrt!");
         if (SWITCH_ON_WHEN_LOCKED)
            switchRelay(ch, 1);
         if (SWITCH_OFF_WHEN_LOCKED)
            switchRelay(ch, 0);
         
         lockActive[ch] = true;
      }                  
      else{                 
         lockActive[ch] = false;
         
         Serial.print("Kanal ");
         Serial.print(ch + 1);
         Serial.println(" wird entsperrt!");
         if (SWITCH_ON_WHEN_UNLOCKED)
            switchRelay(ch, 1);
         if (SWITCH_OFF_WHEN_UNLOCKED)
            switchRelay(ch, 0);
      }
   }
}


void sendStatusGA(uint8_t ch){
   // Status-GA schreiben
   if (client.connected()){
      const uint8_t groupValueWrite[] = {0x00, 0x06, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, GA_STATUS_BYTE[ch][0], GA_STATUS_BYTE[ch][1], 0x00, 0x80 | relayStatus[ch]};
      client.write(groupValueWrite, sizeof(groupValueWrite));
   }   
}


String getUptimeString(){
   uint32_t secsUp  = currentMillis / 1000,      
            seconds = secsUp % 60,
            minutes = (secsUp / 60) % 60,
            hours   = (secsUp / (60 * 60)) % 24,
            days    = (millisOverflows * 50) + (secsUp / (60 * 60 * 24));
      
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
   return 
         HTML_HEADER + 
         
         "<H1>" + HOST_NAME + " (" + HOST_DESCRIPTION + ")</H1>\n"
         "Laufzeit: " + getUptimeString() + "\n"
         
         "<H2>KNX-Status</H2>\n"
         
         "<p>\n"
         + String(client.connected()
            ? "<div class=\"green\">Das Modul ist mit dem EIBD/KNXD verbunden!</div>\n"
            : "<div class=\"red\">Das Modul ist nicht mit dem EIBD/KNXD verbunden!</div>\n"
           ) +
         "Bisher aufgebaute Verbindungen: " + String(knxdConnectionCount) + "\n"
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
         "<td>" + String(GA_SWITCH[0][0]) + "/" + String(GA_SWITCH[0][1]) + "/" + String(GA_SWITCH[0][2]) + "</td>"
         + String(CHANNELS > 1 ? "<td>" + String(GA_SWITCH[1][0]) + "/" + String(GA_SWITCH[1][1]) + "/" + String(GA_SWITCH[1][2]) + "</td>" : "")
         + String(CHANNELS > 2 ? "<td>" + String(GA_SWITCH[2][0]) + "/" + String(GA_SWITCH[2][1]) + "/" + String(GA_SWITCH[2][2]) + "</td>" : "")
         + String(CHANNELS > 3 ? "<td>" + String(GA_SWITCH[3][0]) + "/" + String(GA_SWITCH[3][1]) + "/" + String(GA_SWITCH[3][2]) + "</td>" : "") +
         "</tr>\n"
         
         "<tr><td>Sperren</td>"
         "<td>" + String(GA_LOCK[0][0]) + "/" + String(GA_LOCK[0][1]) + "/" + String(GA_LOCK[0][2]) + "</td>"
         + String(CHANNELS > 1 ? "<td>" + String(GA_LOCK[1][0]) + "/" + String(GA_LOCK[1][1]) + "/" + String(GA_LOCK[1][2]) + "</td>" : "")
         + String(CHANNELS > 2 ? "<td>" + String(GA_LOCK[2][0]) + "/" + String(GA_LOCK[2][1]) + "/" + String(GA_LOCK[2][2]) + "</td>" : "")
         + String(CHANNELS > 3 ? "<td>" + String(GA_LOCK[3][0]) + "/" + String(GA_LOCK[3][1]) + "/" + String(GA_LOCK[3][2]) + "</td>" : "") +
         "</tr>\n"
         
         "<tr><td>Status</td>"
         "<td>" + String(GA_STATUS[0][0]) + "/" + String(GA_STATUS[0][1]) + "/" + String(GA_STATUS[0][2]) + "</td>"
         + String(CHANNELS > 1 ? "<td>" + String(GA_STATUS[1][0]) + "/" + String(GA_STATUS[1][1]) + "/" + String(GA_STATUS[1][2]) + "</td>" : "")
         + String(CHANNELS > 2 ? "<td>" + String(GA_STATUS[2][0]) + "/" + String(GA_STATUS[2][1]) + "/" + String(GA_STATUS[2][2]) + "</td>" : "")
         + String(CHANNELS > 3 ? "<td>" + String(GA_STATUS[3][0]) + "/" + String(GA_STATUS[3][1]) + "/" + String(GA_STATUS[3][2]) + "</td>" : "") +
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
         "<tr><td>RSSI</td><td>" + String(WiFi.RSSI()) + "</td></tr>\n"
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
      switchRelay(0, true);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/on", [](){
      switchRelay(1, true);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/on", [](){
      switchRelay(2, true);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/on", [](){
      switchRelay(3, true);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/off", [](){
      switchRelay(0, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });   
   
   webServer.on("/ch2/off", [](){
      switchRelay(1, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/off", [](){
      switchRelay(2, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/off", [](){
      switchRelay(3, false);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });  
   
   webServer.on("/ch1/toggle", [](){
      switchRelay(0, !relayStatus[0]);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/toggle", [](){
      switchRelay(1, !relayStatus[1]);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/toggle", [](){
      switchRelay(2, !relayStatus[2]);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/toggle", [](){
      switchRelay(3, !relayStatus[3]);
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
