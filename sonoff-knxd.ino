/*
 * Einstellungen für Sonoff S20 in der Arduino IDE:
 * 
 * Boardverwalter-URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json
 * Board:              Generic ESP8266 Module
 * Flash Mode:         DOUT
 * Flash Size:         1M (64K SPIFFS) [Notwendig für Updates über die Weboberfläche]
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
// https://github.com/knxd/knxd/blob/master/src/include/eibtypes.h
#include "eibtypes.h"

/* 
 * **************************
 * *** Wichtige Parameter *** 
 * **************************
 */

// WLAN-Parameter
const char*    ssid     = "ssid";
const char*    password = "password";

// Gerätename und -beschreibung
// Bei mehreren Sonoff-Geräten kann dieser Zähler erhöht werden, wodurch automatisch der Host-Name und die Gruppenadressen angepasst werden.
const uint8_t  hostNumber = 0;

const String   hostName        = "Sonoff-S20-" + String(hostNumber + 1),
               hostDescription = "Description";

// KNX-Parameter
const char*    knxdIP   = "10.9.8.7";
const uint32_t knxdPort = 6720;

/* Hier werden die Gruppenadressen definiert. Beispiel:
 * Schalten: 31/0/0
 * Sperren:  31/0/1
 * Status:   31/0/2
 */
const uint8_t  gaSwitch[] = {31, 0, 0 + (hostNumber * 3)},
               gaLock[]   = {31, 0, 1 + (hostNumber * 3)},
               gaStatus[] = {31, 0, 2 + (hostNumber * 3)};

const boolean  switchOnWhenLocked    = false,
               switchOffWhenLocked   = false,
               switchOnWhenUnlocked  = false,
               switchOffWhenUnlocked = false;

/* 
 * ***************************
 * *** Allgemeine Optionen *** 
 * ***************************
 */

// Sekunden zwischen zwei Keep-Alive-Paketen bei einer ungenutzen Verbindung
#define        KA_IDLE_S                           600
// Sekunden zwischen zwei (unbestätigten) Keep-Alive-Paketen
#define        KA_INTERVAL_S                       10
// Anzahl von unbestätigten Keep-Alive-Paketen, nach denen die Verbindung geschlossen wird
#define        KA_RETRY_COUNT                      10
// Wartezeit in Sekunden vor dem Neuaufbau einer geschlossenen Verbindung
#define        CONNECTION_LOST_DELAY_S             10
// Maximale Zeit im Millisekunden, nach der eine Bestätigung der neu aufgebauten Verbindung vom EIBD/KNXD kommen muss
#define        CONNECTION_CONFIRMATION_TIMEOUT_MS  500
// Entprellzeit in Millisekunden für den Schaltknopf
#define        BUTTON_DEBOUNCING_TIME_MS           100

// LED wird parallel zum Relais (welches eine zusätzliche LED enthält) geschaltet
#define        LED_SHOWS_RELAY_STATUS              1
// LED blinkt bei bestehender EIBD/KNXD-Verbindung
#define        LED_BLINKS_IF_CONNECTED             0
// Zeit in Millisekunden, welche die LED während des Blinkens ausgeschaltet ist
#define        LED_BLINK_OFF_TIME_MS               900
// Zeit in Millisekunden, welche die LED während des Blinkens eingeschaltet ist
#define        LED_BLINK_ON_TIME_MS                100

const uint8_t  gpioLed    = 13,
               gpioRelay  = 12,
               gpioButton = 0;

/* 
 * *************************
 * *** Interne Variablen *** 
 * *************************
 */

const uint8_t  gaSwitchHex[]     = {(gaSwitch[0] << 3) + gaSwitch[1], gaSwitch[2]},
               gaLockHex[]       = {(gaLock[0] << 3) + gaLock[1], gaLock[2]},
               gaStatusHex[]     = {(gaStatus[0] << 3) + gaStatus[1], gaStatus[2]},
               groupConRequest[] = {0x00, 0x05, EIB_OPEN_GROUPCON >> 8, EIB_OPEN_GROUPCON & 0xFF, 0x00, 0x00, 0x00};

boolean        connectionConfirmed = false,
               lockActive          = false,
               buttonPressed       = false,
               relayStatus         = false,
               ledBlinkStatus      = false;

uint8_t        messageLength = 0,
               messageResponse[32];

uint32_t       knxdConnectionCount         = 0,
               
// Variablen zur Zeitmessung
               currentMillis               = 0,
               millisOverflows             = 0,
               buttonPressedMillis         = 0,
               connectionEstablishedMillis = 0,
               connectionFailedMillis      = 0,
               ledBlinkLastSwitch          = 0;

// WLAN-Client
WiFiClient              client;

// Webserver
ESP8266WebServer        webServer(80);
ESP8266HTTPUpdateServer httpUpdateServer;

const String htmlHeader =  "<!DOCTYPE HTML>\n"
                           "<html>\n"
                           "<head>\n"
                           "<meta http-equiv=\"refresh\" content=\"15;URL=/\">\n"
                           "</head>\n"
                           "<body>\n",
      
             htmlFooter =  "</body>\n"
                           "</html>";

void setup() {
   Serial.begin(115200);
   delay(10);
   
   pinMode(gpioLed, OUTPUT);
   digitalWrite(gpioLed, !relayStatus);
   pinMode(gpioRelay, OUTPUT);
   digitalWrite(gpioRelay, relayStatus);
   
   Serial.print(hostName);
   Serial.print(" (");
   Serial.print(hostDescription);
   Serial.println(")\n");
   
   Serial.print("Gruppenadresse schalten: ");
   Serial.print(gaSwitch[0]);
   Serial.print("/");
   Serial.print(gaSwitch[1]);
   Serial.print("/");
   Serial.print(gaSwitch[2]);
   Serial.println();
   
   Serial.print("Gruppenadresse sperren:  ");
   Serial.print(gaLock[0]);
   Serial.print("/");
   Serial.print(gaLock[1]);
   Serial.print("/");
   Serial.print(gaLock[2]);
   Serial.println();
   
   Serial.print("Gruppenadresse Status:   ");
   Serial.print(gaStatus[0]);
   Serial.print("/");
   Serial.print(gaStatus[1]);
   Serial.print("/");
   Serial.print(gaStatus[2]);
   Serial.println();   
   
   // We start by connecting to a WiFi network      
   Serial.println();
   Serial.print("Verbinde mit WLAN '");
   Serial.print(ssid);
   Serial.print("'");
   
   /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
   *   would try to act as both a client and an access-point and could cause
   *   network-issues with your other WiFi-devices on your WiFi-network. */
   WiFi.mode(WIFI_STA);
   WiFi.hostname(hostName);
   WiFi.begin(ssid, password);
   
   while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
   }
   
   Serial.println();
   Serial.print("Verbindung hergestellt. IP-Adresse: ");
   Serial.println(WiFi.localIP());
      
   webServer.on("/", [](){
      webServer.send(200, "text/html", 
         htmlHeader + 
         
         "<H1>" + hostName + " (" + hostDescription + ")</H1>\n"
         "Uptime: " + getUptimeString() + "\n"
         
         "<H2>KNX-Status</H2>\n"      
         "<table>\n"
         "<tr><td>Gruppenadresse schalten:</td><td>" + String(gaSwitch[0]) + "/" + String(gaSwitch[1]) + "/" + String(gaSwitch[2]) + "</td></tr>\n"
         "<tr><td>Gruppenadresse sperren:</td><td>" + String(gaLock[0]) + "/" + String(gaLock[1]) + "/" + String(gaLock[2]) + "</td></tr>\n"
         "<tr><td>Gruppenadresse Status:</td><td>" + String(gaStatus[0]) + "/" + String(gaStatus[1]) + "/" + String(gaStatus[2]) + "</td></tr>\n"
         "<tr><td>Verbunden mit EIBD/KNXD:</td><td>" + String(client.connected() ? "Ja" : "Nein") + "</td></tr>\n"
         "<tr><td>Bisher aufgebaute Verbindungen:</td><td>" + String(knxdConnectionCount) + "</td></tr>\n"
         "</table>\n"
      
         "<H2>WLAN-Status</H2>\n"
         "<table>\n"
         "<tr><td>IP-Adresse:</td><td>" + WiFi.localIP().toString() + "</td></tr>\n"
         "<tr><td>Netzmaske:</td><td>" + WiFi.subnetMask().toString() + "</td></tr>\n"
         "<tr><td>Gateway:</td><td>" + WiFi.gatewayIP().toString() + "</td></tr>\n"
         "<tr><td>MAC:</td><td>" + WiFi.macAddress() + "</td></tr>\n"
         "<tr><td>SSID:</td><td>" + String(WiFi.SSID()) + "</td></tr>\n"
         "<tr><td>RSSI:</td><td>" + String(WiFi.RSSI()) + "</td></tr>\n"
         "</table>\n"
               
         "<H2>Befehle</H2>\n"
         "<a href=\"on\">Einschalten</a><br />\n"
         "<a href=\"off\">Ausschalten</a><br />\n"
         "<a href=\"toggle\">Umschalten</a><br />\n"
         "<a href=\"state\">Schaltstatus</a><br />\n"
         "<a href=\"update\">Update</a><br />\n"
         "<a href=\"reboot\">Neustarten</a><br />\n"
         
         "<H2>Schaltstatus</H2>\n"         
         + String(relayStatus ? "Schaltsteckdose ist aktuell ein.<p><a href=\"off\">Ausschalten</a></p>\n" : "Schaltsteckdose ist aktuell aus.<p><a href=\"on\">Einschalten</a></p>\n") +
         
         htmlFooter
      );
   });  
   
   webServer.on("/on", [](){
      webServer.send(200, "text/html", htmlHeader + "Schaltsteckdose ist aktuell ein.<p><a href=\"off\">Ausschalten</a></p><p><a href=\"..\">Zur&uuml;ck</a></p>\n" + htmlFooter);
      switchRelay(true);
   });
   
   webServer.on("/off", [](){
      webServer.send(200, "text/html", htmlHeader + "Schaltsteckdose ist aktuell aus.<p><a href=\"on\">Einschalten</a></p><p><a href=\"..\">Zur&uuml;ck</a></p>\n" + htmlFooter);
      switchRelay(false);
   });   
   
   webServer.on("/state", [](){
      if(relayStatus == 0){
         webServer.send(200, "text/plain", "off");
      }
      else{
         webServer.send(200, "text/plain", "on");
      }
   });
   
   webServer.on("/toggle", [](){
      if(relayStatus == 0){
         webServer.send(200, "text/html", htmlHeader + "Schaltsteckdose ist aktuell ein.<p><a href=\"off\">Ausschalten</a></p><p><a href=\"..\">Zur&uuml;ck</a></p>\n" + htmlFooter);
         switchRelay(true);
      }
      else{
         webServer.send(200, "text/html", htmlHeader + "Schaltsteckdose ist aktuell aus.<p><a href=\"on\">Einschalten</a></p><p><a href=\"..\">Zur&uuml;ck</a></p>\n" + htmlFooter);
         switchRelay(false);
      }
   }); 
   
   webServer.on("/reboot", [](){
      webServer.send(200, "text/html", htmlHeader + "Schaltsteckdose wird neugestartet...<p><a href=\"..\">Zur&uuml;ck</a></p>\n" + htmlFooter);
      delay(1000);
      ESP.restart();
   });
   
   httpUpdateServer.setup(&webServer);
   
   webServer.begin();
   Serial.println("Webserver gestartet");
   
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
   
   // Taste an der S20 prüfen
   // true = nicht gedrückt, false = gedrückt
   if (!digitalRead(gpioButton)){
      if (buttonPressedMillis == 0)
         buttonPressedMillis = currentMillis;
      
      else if (!lockActive && !buttonPressed && (currentMillis - buttonPressedMillis) >= BUTTON_DEBOUNCING_TIME_MS) {
         buttonPressed = true;
         switchRelay(!relayStatus);      
      }      
   }
   else {
      buttonPressed = false;
      buttonPressedMillis = 0;
   }
   
   // KNX-Kommunikation
   knxLoop();
   
   // Falls eine Verbindung zum EIBD/KNXD aufgebaut ist, blinkt die LED sofern gewünscht.
   if (LED_BLINKS_IF_CONNECTED)
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
         if (!connectionConfirmed && messageLength == 2 && messageResponse[0] == groupConRequest[2] && messageResponse[1] == groupConRequest[3]){
            Serial.println("EIBD/KNXD Verbindung hergestellt");
            connectionConfirmed = true;         
            knxdConnectionCount++;
            
            // Die Status-GA senden, sobald die Verbindung steht. Dadurch wird sie beim ersten Start nach einem Spannungsausfall gesendet.
            sendStatusGA();
         }
         
         if (     messageLength >= 8 // Ein Bit bzw. DPT 1.001
               && messageResponse[0] == EIB_GROUP_PACKET >> 8
               && messageResponse[1] == EIB_GROUP_PACKET & 0xFF){
            
            // Schaltkommando
            if (     messageLength == 8 // Ein Bit bzw. DPT 1.001
                  && messageResponse[4] == gaSwitchHex[0]
                  && messageResponse[5] == gaSwitchHex[1]
                  && messageResponse[6] == 0x00
                  && (messageResponse[7] & 0x80) == 0x80){
               
               if (lockActive)
                  Serial.println("Schaltkommando wird ignoriert, Steckdose ist gesperrt!");
               else
                  switchRelay(messageResponse[7] & 0x0F);
            }
            
            // Sperren
            else if (messageLength == 8 // Ein Bit bzw. DPT 1.001
                  && messageResponse[4] == gaLockHex[0]
                  && messageResponse[5] == gaLockHex[1]
                  && messageResponse[6] == 0x00
                  && (messageResponse[7] & 0x80) == 0x80){
               
               lockActive = (messageResponse[7] & 0x0F);
            
               if (lockActive){
                  Serial.println("Steckdose wird gesperrt!");
                  if (switchOnWhenLocked)
                     switchRelay(1);
                  if (switchOffWhenLocked)
                     switchRelay(0);
               }                  
               else{                  
                  Serial.println("Steckdose wird entsperrt!");
                  if (switchOnWhenUnlocked)
                     switchRelay(1);
                  if (switchOffWhenUnlocked)
                     switchRelay(0);
               }
            }
            
            // Status lesen
            else if (messageLength == 8 // Ein Bit bzw. DPT 1.001
                  && messageResponse[4] == gaStatusHex[0]
                  && messageResponse[5] == gaStatusHex[1]
                  && messageResponse[6] == 0x00
                  && messageResponse[7] == 0x00){
               
               Serial.println("Leseanforderung Status");
            
               const uint8_t groupValueResponse[] = {0x00, 0x06, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, gaStatusHex[0], gaStatusHex[1], 0x00, 0x40 | relayStatus};
               client.write(groupValueResponse, sizeof(groupValueResponse));
            }
         }         
         
         // Für die nächste Nachricht zurücksetzen
         messageLength = 0;
      }
   }
}


void ledBlink(){
   // Falls die Verbindung steht, die LED blinken lassen.
   if (connectionConfirmed){
      // LED-Blinkstatus ist zurzeit an
      if (ledBlinkStatus) {
         if ((currentMillis - ledBlinkLastSwitch) >= LED_BLINK_ON_TIME_MS){
            // LED invertieren
            if (LED_SHOWS_RELAY_STATUS && relayStatus)
               digitalWrite(gpioLed, LOW);
            else
               digitalWrite(gpioLed, HIGH);
            ledBlinkStatus = false;
            ledBlinkLastSwitch = currentMillis;
         }
      }
      // LED-Blinkstatus ist zurzeit aus
      else {
         if ((currentMillis - ledBlinkLastSwitch) >= LED_BLINK_OFF_TIME_MS){
            // LED invertieren
            if (LED_SHOWS_RELAY_STATUS && relayStatus)
               digitalWrite(gpioLed, HIGH);
            else
               digitalWrite(gpioLed, LOW);
            ledBlinkStatus = true;
            ledBlinkLastSwitch = currentMillis;
         }
      }
   }
   // Die Verbindung ist unterbrochen, auf den Zustand des Relais setzen
   else if (LED_SHOWS_RELAY_STATUS)
      digitalWrite(gpioLed,  !relayStatus);
   // Die LED wird nicht für den Relais-Status verwendet, ausschalten
   else
      digitalWrite(gpioLed,  HIGH);
      
}


boolean connectToKnxd(){
   connectionConfirmed = false;
   client.stopAll();
   
   Serial.print("Verbinde mit EIBD/KNXD auf ");
   Serial.println(knxdIP);
   
   if (!client.connect(knxdIP, knxdPort)) {
      Serial.println("Verbindung fehlgeschlagen!");
      return false;
   }
   else{      
      client.setNoDelay(true);
      client.keepAlive(KA_IDLE_S, KA_INTERVAL_S, KA_RETRY_COUNT);      
      client.write(groupConRequest, sizeof(groupConRequest));
      
      connectionEstablishedMillis = millis();
      
      return client.connected();
   }
}


void switchRelay(boolean on){
   relayStatus = on;
   
   if (on)
      Serial.println("Steckdose eingeschaltet");
   else
      Serial.println("Steckdose ausgeschaltet");
   
   digitalWrite(gpioRelay, relayStatus);   
   
   if (LED_SHOWS_RELAY_STATUS)
      digitalWrite(gpioLed,  !relayStatus);
      
   sendStatusGA();
}


void sendStatusGA(){
   // Status-GA schreiben
   if (client.connected()){
      const uint8_t groupValueWrite[] = {0x00, 0x06, EIB_GROUP_PACKET >> 8, EIB_GROUP_PACKET & 0xFF, gaStatusHex[0], gaStatusHex[1], 0x00, 0x80 | relayStatus};
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
