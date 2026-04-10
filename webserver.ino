static const char HTML_FOOTER[] = "</body>\n</html>";

void sendHtmlHeader(const char* refreshRate, const char* refreshUrl, const char* bodyId){
   webServer.sendContent(
      "<!DOCTYPE HTML>\n"
      "<html lang=\"de\">\n"
      "<head>\n"
         "<meta charset=\"utf-8\">\n"
         "<meta http-equiv=\"refresh\" content=\"" 
   );
   webServer.sendContent(refreshRate);
   webServer.sendContent("; URL=");
   webServer.sendContent(refreshUrl);
   webServer.sendContent(
         "\">\n"
         "<title>"
   );
   webServer.sendContent(HOST_NAME);
   webServer.sendContent(
         "</title>"
         "<style>\n"
            ".green  {color:darkgreen; font-weight: bold;}\n"
            ".red    {color:darkred; font-weight: bold;}\n"
            ".orange {color:darkorange; font-weight: bold;}\n"
            "table, th, td {border-collapse:collapse; border: 1px solid black;}\n"
            "th, td {text-align: left; padding: 2px 10px 2px 10px;}\n"
            "th {background-color: #707070; color: white;}\n"
            "a:link    {text-decoration: none;}\n"
            "a:visited {text-decoration: none;}\n"
            "a:active  {text-decoration: none;}\n"
            "a:hover   {text-decoration: underline;}\n"
            "a.box {"
               "background-color: #707070;"
               "color: white;"
               "padding: 2px 10px;"
               "width: 200px;"
               "text-align: center;"
               "display: inline-block;"
               "font-weight: bold;"
               "text-decoration: none;"
               "border-radius: 5px;"
               "}\n"
            "body#maintenance a.box#maintenanceA, body#main a.box#mainA, body#connectionLog a.box#connectionLogA, body#switchLog a.box#switchLogA, body#SCD30 a.box#SCD30A, a.box:hover {background-color: black;}\n"
         "</style>\n"
      "</head>\n"
      "<body id=\"");
   webServer.sendContent(bodyId);
   webServer.sendContent(      
      "\">\n"
      "<H1><a href=\"/\" style=\"color: inherit\">");
   webServer.sendContent(HOST_NAME);
   webServer.sendContent(" (");
   webServer.sendContent(HOST_DESCRIPTION);
   webServer.sendContent(
      ")</a></H1>\n"      
      "<a href=\"maintenance\" title=\"Ger&auml;tewartung\" "
   );
   webServer.sendContent(knxdConnectionConfirmed
         ? "class=\"green\">Das Modul ist mit dem knxd verbunden!"
         : "class=\"red\">Das Modul ist nicht mit dem knxd verbunden!"
         );
   webServer.sendContent(
      "</a>\n"
      
      "<p>\n"
      "<a class=\"box\" id=\"mainA\"          href=\"..\"            title=\"Schaltstatus\">Schaltstatus</a>\n"
      "<a class=\"box\" id=\"maintenanceA\"   href=\"maintenance\"   title=\"Ger&auml;tewartung\">Ger&auml;tewartung</a>\n"
      "<a class=\"box\" id=\"switchLogA\"     href=\"switchLog\"     title=\"Schaltprotokoll\">Schaltprotokoll</a>\n"
      "<a class=\"box\" id=\"connectionLogA\" href=\"connectionLog\" title=\"Verbindungsprotokoll\">Verbindungsprotokoll</a>\n"
      #if SCD30_ENABLE == true
         "<a class=\"box\" id=\"SCD30A\"         href=\"SCD30\"         title=\"Luftqualit&auml;t\">Luftqualit&auml;t</a>\n"
      #endif
      "</p>\n"
   );
}


void sendInt(int32_t v) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%d", v);
  webServer.sendContent(buffer);
}


void sendUInt(uint32_t v) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%u", v);
  webServer.sendContent(buffer);
}


void sendFloat(float value, uint8_t decimals = 2) {
  char buffer[24];
  dtostrf(value, 0, decimals, buffer);
  webServer.sendContent(buffer);
}


void sendFormattedUInt(uint32_t number) {
   char buffer[32];

   if (number >= 1000000000) {
      uint32_t a = number / 1000000000;
      uint32_t b = (number / 1000000) % 1000;
      uint32_t c = (number / 1000) % 1000;
      uint32_t d = number % 1000;
      snprintf(buffer, sizeof(buffer), "%u&#8239;%03u&#8239;%03u&#8239;%03u", a, b, c, d);
   } 
   else if (number >= 1000000) {
      uint32_t a = number / 1000000;
      uint32_t b = (number / 1000) % 1000;
      uint32_t c = number % 1000;
      snprintf(buffer, sizeof(buffer), "%u&#8239;%03u&#8239;%03u", a, b, c);
   }
   else if (number >= 1000) {
      uint32_t a = number / 1000;
      uint32_t b = number % 1000;
      snprintf(buffer, sizeof(buffer), "%u&#8239;%03u", a, b);
   }
   else {
      snprintf(buffer, sizeof(buffer), "%u", number);
   }

   webServer.sendContent(buffer);
}


void sendIPAddress(const IPAddress& ip){
   char buffer[16];
   snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
   webServer.sendContent(buffer);
}


void sendMAC(const uint8_t mac[]){
   char buffer[18];
   snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
   webServer.sendContent(buffer);
}

void sendGA(const uint8_t ga[]){
   char buffer[9];
   snprintf(buffer, sizeof(buffer), "%u/%u/%u", ga[0], ga[1], ga[2]);
   webServer.sendContent(buffer);
}


void sendGATableCell(const uint8_t ga[][3], const uint8_t gaCount){
   webServer.sendContent("<td>");

   for (uint8_t i=0; i<gaCount; i++){
      if (ga[i][0] + ga[i][1] + ga[i][2] > 0){
         sendGA(ga[i]);
         webServer.sendContent("<br>");
      }
   }

   webServer.sendContent("</td>");
}


void setupWebServer(){
   /*
   * *****************
   * *** Main page ***
   * *****************
   */
   webServer.on("/", [](){
      webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
      webServer.send(200, "text/html", "");
      sendHtmlHeader("15", "/", "main");
      webServer.sendContent(
         "<H2>Schaltstatus</H2>\n"
         
         "<table>\n"
         "<tr>"
         "<th>Status</th>"
         "<th>Kanal 1</th>"
      );

      if (CHANNELS > 1) webServer.sendContent("<th>Kanal 2</th>");
      if (CHANNELS > 2) webServer.sendContent("<th>Kanal 3</th>");
      if (CHANNELS > 3) webServer.sendContent("<th>Kanal 4</th>");

      webServer.sendContent(
         "</tr>\n"
         
         "<tr><td>Schaltstatus</td>"
         "<td><a href=\"ch1/toggle\" "
      );
      static const char relayOnText[]  = "class=\"green\">eingeschaltet</a></td>";
      static const char relayOffText[] = "class=\"red\">ausgeschaltet</a></td>";
      webServer.sendContent(relayStatus[0] ? relayOnText : relayOffText);
      
      if (CHANNELS > 1) {
         webServer.sendContent("<td><a href=\"ch2/toggle\" ");
         webServer.sendContent(relayStatus[1] ? relayOnText : relayOffText);         
      }
      if (CHANNELS > 2) {
         webServer.sendContent("<td><a href=\"ch3/toggle\" ");
         webServer.sendContent(relayStatus[2] ? relayOnText : relayOffText);         
      }
      if (CHANNELS > 3) {
         webServer.sendContent("<td><a href=\"ch4/toggle\" ");
         webServer.sendContent(relayStatus[3] ? relayOnText : relayOffText);         
      }

      static const char relayLockedText[]   = "class=\"red\">gesperrt</a></td>";
      static const char relayUnlockedText[] = "class=\"green\">freigegeben</a></td>";
      webServer.sendContent(
         "</tr>\n"
         
         "<tr><td>Sperre</td>"
         "<td><a href=\"ch1/toggleLock\" "
      );
      webServer.sendContent(lockActive[0] ? relayLockedText : relayUnlockedText);

      if (CHANNELS > 1) {
         webServer.sendContent("<td><a href=\"ch2/toggleLock\" ");
         webServer.sendContent(lockActive[1] ? relayLockedText : relayUnlockedText);         
      }
      if (CHANNELS > 2) {
         webServer.sendContent("<td><a href=\"ch3/toggleLock\" ");
         webServer.sendContent(lockActive[2] ? relayLockedText : relayUnlockedText);         
      }
      if (CHANNELS > 3) {
         webServer.sendContent("<td><a href=\"ch4/toggleLock\" ");
         webServer.sendContent(lockActive[3] ? relayLockedText : relayUnlockedText);         
      }

      webServer.sendContent(
         "</tr>\n"
         "</table>\n"
      );

      webServer.sendContent(HTML_FOOTER);
   });

   /*
   * ************************
   * *** Maintenance page ***
   * ************************
   */
   webServer.on("/maintenance", [](){
      char buffer[32];

      webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
      webServer.send(200, "text/html", "");
      sendHtmlHeader("60", "/maintenance", "maintenance");
      webServer.sendContent(
         "<H2>Ger&auml;tewartung</H2>\n"
         
         "<table>\n"
         "<tr><td>Laufzeit</td>"
         "<td>"
      );
      getUptimeString(buffer, sizeof(buffer), getUptimeSeconds());
      webServer.sendContent(buffer);

      if (dateValid && timeValid){         
         webServer.sendContent(" (seit ");
         getDateString(buffer, sizeof(buffer), bootTime);
         webServer.sendContent(buffer);
         webServer.sendContent(" ");
         getTimeString(buffer, sizeof(buffer), bootTime);
         webServer.sendContent(buffer);
         webServer.sendContent(")");
      }
      
      webServer.sendContent(
         "</td></tr>\n"
         "<tr><td>Letzter Reset</td>"
         "<td>"
      );
      webServer.sendContent(ESP.getResetReason());
      webServer.sendContent(
         "</td></tr>\n"
         "<tr><td>Firmware</td>"
         "<td><a href=\"https://github.com/McOrvas/sonoff-knxd\" target=\"_blank\" rel=\"noopener noreferrer\">sonoff-knxd</a> (" 
      );
      webServer.sendContent(SOFTWARE_VERSION);
      webServer.sendContent(
         ")</td></tr>\n"
         "<tr><td>SDK-Version</td>"
         "<td>"
      );
      webServer.sendContent(ESP.getSdkVersion());
      webServer.sendContent(
         "</td></tr>\n"
         "<tr><td>Zusammenhängend / insgesamt freier Heap</td>"
         "<td>"
      );
      sendFormattedUInt(ESP.getMaxFreeBlockSize());
      webServer.sendContent(" / ");
      sendFormattedUInt(ESP.getFreeHeap());
      webServer.sendContent(" Byte (");
      sendFormattedUInt(ESP.getHeapFragmentation());
      webServer.sendContent(
         " % fragmentiert)</td></tr>\n"
         "<tr><td>Aktuelle / maximale Sketch-Gr&ouml;&szlig;e</td>"
         "<td>"
      );

      const uint32_t sketchSize       = ESP.getSketchSize(),
                     freeSketchSpace  = ESP.getFreeSketchSpace(),
                     sketchPercentage = 100 * sketchSize / freeSketchSpace;

      sendFormattedUInt(sketchSize);
      webServer.sendContent(" / ");
      sendFormattedUInt(freeSketchSpace);
      webServer.sendContent(" Byte (");
      sendFormattedUInt(sketchPercentage);
      webServer.sendContent(
         " % verwendet)</td></tr>\n"
         "<tr><td>Flash-Gr&ouml;&szlig;e / -Geschwindigkeit</td>"
         "<td>" 
      );
      sendFormattedUInt(ESP.getFlashChipSize());
      webServer.sendContent(" Byte / ");
      sendFormattedUInt(ESP.getFlashChipSpeed() / 1000000);
      webServer.sendContent(" MHz</td></tr>\n");
      webServer.sendContent(
         "<tr><td>Prozessortakt</td>"
         "<td>"
      );
      sendFormattedUInt(ESP.getCpuFreqMHz());
      webServer.sendContent(" MHz (");
      sendFormattedUInt(getLoopsPerSecond());
      webServer.sendContent(
         " loops/s)</td></tr>\n"
         "</table>\n"

         "<p><a href=\"update\" title=\"Firmware aktualisieren\" class=\"box\">Firmware aktualisieren</a>"
         "&nbsp;<a href=\"reboot\" title=\"Ger&auml;t neu starten\" class=\"box\">Ger&auml;t neu starten</a></p>\n"

         "<H2>Netzwerk</H2>\n"
         
         "<table>\n"
         "<tr><td>IP-Adresse</td><td>"
      );
      sendIPAddress(WiFi.localIP());
      webServer.sendContent(
         "</td></tr>\n"
         "<tr><td>Netzmaske</td><td>"  
      );
      sendIPAddress(WiFi.subnetMask());
      webServer.sendContent(
         "</td></tr>\n"
         "<tr><td>Gateway</td><td>"    
      );
      sendIPAddress(WiFi.gatewayIP());
      webServer.sendContent(
         "</td></tr>\n"
         "<tr><td>MAC</td><td>"        
      );
      uint8_t mac[6];
      WiFi.macAddress(mac);
      sendMAC(mac);
      webServer.sendContent(
         "</td></tr>\n"
         "<tr><td>SSID</td><td>"
      );
      webServer.sendContent(WiFi.SSID()); // Last function here which delivers a String() object. No alternative seems to be available.
      webServer.sendContent(
         "</td></tr>\n"
         "<tr><td>BSSID</td><td>"
      );
      sendMAC(WiFi.BSSID());
      webServer.sendContent( 
         "</td></tr>\n"
         "<tr><td>Kanal</td><td>"
      );
      sendInt(WiFi.channel());
      webServer.sendContent(
         "</td></tr>\n"
         "<tr><td>RSSI</td><td>"
      );
      sendInt(WiFi.RSSI());
      webServer.sendContent(
         " dBm</td></tr>\n"
         "<tr><td>knxd</td><td>"
      );
      webServer.sendContent(KNXD_IP);
      webServer.sendContent("</td></tr>\n");
         #if NTFY_ENABLE == true
            webServer.sendContent("<tr><td>ntfy</td><td>");
            webServer.sendContent(NTFY_IP);
            webServer.sendContent(":");
            sendInt(NTFY_PORT);
            webServer.sendContent("/");
            webServer.sendContent(NTFY_TOPIC);
            webServer.sendContent("</td></tr>\n");
         #endif
      webServer.sendContent("</table>\n"

         "<H2>KNX-Konfiguration</H2>\n"
         
         "<table>\n"
         "<tr>"
         "<th>Gruppenadressen</th>"
         "<th>Kanal 1</th>"
      );
      if (CHANNELS > 1) webServer.sendContent("<th>Kanal 2</th>");
      if (CHANNELS > 2) webServer.sendContent("<th>Kanal 3</th>");
      if (CHANNELS > 3) webServer.sendContent("<th>Kanal 4</th>");

      webServer.sendContent(
         "</tr>\n"
         
         "<tr><td>Schalten</td>"
      );

      sendGATableCell(GA_SWITCH[0], GA_SWITCH_COUNT);
      if (CHANNELS > 1) sendGATableCell(GA_SWITCH[1], GA_SWITCH_COUNT);
      if (CHANNELS > 2) sendGATableCell(GA_SWITCH[2], GA_SWITCH_COUNT);
      if (CHANNELS > 3) sendGATableCell(GA_SWITCH[3], GA_SWITCH_COUNT);

      webServer.sendContent(
         "</tr>\n"
         
         "<tr><td>Sperren</td>"
      );
       
      sendGATableCell(GA_LOCK[0], GA_LOCK_COUNT);
      if (CHANNELS > 1) sendGATableCell(GA_LOCK[1], GA_LOCK_COUNT);
      if (CHANNELS > 2) sendGATableCell(GA_LOCK[2], GA_LOCK_COUNT);
      if (CHANNELS > 3) sendGATableCell(GA_LOCK[3], GA_LOCK_COUNT);

      webServer.sendContent(
         "</tr>\n"
         
         "<tr><td>Status</td>"
         "<td>"
      );

      sendGA(GA_STATUS[0]);
      webServer.sendContent("</td>");
      if (CHANNELS > 1) {webServer.sendContent("<td>"); sendGA(GA_STATUS[1]); webServer.sendContent("</td>");}
      if (CHANNELS > 2) {webServer.sendContent("<td>"); sendGA(GA_STATUS[2]); webServer.sendContent("</td>");}
      if (CHANNELS > 3) {webServer.sendContent("<td>"); sendGA(GA_STATUS[3]); webServer.sendContent("</td>");}

      webServer.sendContent("</tr>\n");

      webServer.sendContent("<tr><td>Datum</td><td>");
      if (GA_DATE_VALID) sendGA(GA_DATE);
      webServer.sendContent("</td></tr>\n");

      webServer.sendContent("<tr><td>Zeit</td><td>");
      if (GA_TIME_VALID) sendGA(GA_TIME);
      webServer.sendContent("</td></tr>\n");
         
      #if SCD30_ENABLE == true
         webServer.sendContent("<tr><td>Temperatur</td><td>");
         sendGA(GA_AIR_TEMPERATURE);
         webServer.sendContent(
            "</td></tr>\n"
            "<tr><td>Luftfeuchtigkeit</td>"
            "<td>"
         );
         sendGA(GA_AIR_HUMIDITY);
         webServer.sendContent(
            "</td></tr>\n"
            "<tr><td>CO<sub>2</sub></td>"
            "<td>"
         );
         sendGA(GA_AIR_CO2);
         webServer.sendContent("</td></tr>\n");
      #endif
         
      webServer.sendContent(
         "</table>\n"

         "<H2>Verbindungsstatistik</H2>\n"
         
         "<table>\n"
         "<tr><th colspan=\"2\">Verbindungsaufbau zum knxd</th></tr>\n"
         "<tr><td>Fehlgeschlagene Verbindungsanfragen</td>"
         "<td>"
      );
      sendFormattedUInt(knxdConnectionFailedCount);
      webServer.sendContent(
         "</td></tr>\n"
         "<tr><td>Erfolgreiche Verbindungsanfragen</td>"
         "<td>"
      );
      sendFormattedUInt(knxdConnectionInitiatedCount);
      webServer.sendContent(
          "</td></tr>\n"
         "<tr><td>Zeit&uuml;berschreitungen bei der Verbindungsbest&auml;tigung durch den knxd ("
      );
      sendFormattedUInt(CONNECTION_CONFIRMATION_TIMEOUT_MS);
      webServer.sendContent(
          " ms)</td>"
         "<td>"
      );
      sendFormattedUInt(knxdConnectionHandshakeTimeouts);
      webServer.sendContent(
          "</td></tr>\n"
         "<tr><td>Vom knxd best&auml;tigte Verbindungen</td>"
         "<td>"
      );
      sendFormattedUInt(knxdConnectionConfirmedCount);
      webServer.sendContent(
          "</td></tr>\n"         
         
         "<tr><th colspan=\"2\">Trennung bestehender Verbindungen zum knxd</th></tr>\n"
         "<tr><td>Verbindungsabbr&uuml;che wegen unvollständig empfangener Telegramme ("
      );
      sendInt(INCOMPLETE_TELEGRAM_TIMEOUT_MS);
      webServer.sendContent(
          " ms)</td>"
         "<td>"
      );
      sendFormattedUInt(incompleteTelegramTimeouts);
      webServer.sendContent(
          "</td></tr>\n"
         "<tr><td>Verbindungsabbr&uuml;che wegen Zeit&uuml;berschreitung zwischen zwei Telegrammen ("
      );
      sendInt(MISSING_TELEGRAM_TIMEOUT_MIN);
      webServer.sendContent(
          " min)</td>"
         "<td>"
      );
      sendFormattedUInt(missingTelegramTimeouts);
      webServer.sendContent(
         "</td></tr>\n"
         "<tr><td>Verbindungsabbr&uuml;che zum WLAN</td>"
         "<td>"
      );
      sendFormattedUInt(wifiDisconnections);
      webServer.sendContent(
          "</td></tr>\n"
         "<tr><td>Sonstige Verbindungsabbr&uuml;che zum knxd</td>"
         "<td>"
      );
      sendFormattedUInt(knxdDisconnections);
      webServer.sendContent(
          "</td></tr>\n"
         
         "<tr><th colspan=\"2\">Sonstiges</th></tr>\n"
         "<tr><td>Empfangene Gruppentelegramme</td>"
         "<td>"
      );
      sendFormattedUInt(receivedTelegrams);
      webServer.sendContent(" (&#8960; ");
      sendFloat(receivedTelegrams / (float) getUptimeSeconds());
      webServer.sendContent(" / s)</td></tr>\n");

      if (GA_DATE_VALID){
         webServer.sendContent("<tr><td>Aktuelles Datum");
         if (dateValid){
            webServer.sendContent(" (empfangen vor ");
            getUptimeString(buffer, sizeof(buffer), (currentMillis - dateTelegramReceivedMillis) / 1000);
            webServer.sendContent(buffer);
            webServer.sendContent(")");
         }
         webServer.sendContent("</td><td>");
         if (dateValid){
            getDateString(buffer, sizeof(buffer), now());
            webServer.sendContent(buffer);
         }
         else webServer.sendContent("-");
         webServer.sendContent("</td></tr>\n");
      }

      if (GA_TIME_VALID){
         webServer.sendContent("<tr><td>Aktuelle Uhrzeit");
         if (timeValid){
            webServer.sendContent(" (empfangen vor ");
            getUptimeString(buffer, sizeof(buffer), (currentMillis - timeTelegramReceivedMillis) / 1000);
            webServer.sendContent(buffer);
            webServer.sendContent(")");
         }         
         webServer.sendContent("</td><td>");
         if (timeValid){
            getTimeString(buffer, sizeof(buffer), now());
            webServer.sendContent(buffer);
         }
         else webServer.sendContent("-");
         webServer.sendContent("</td></tr>\n");
      }

      webServer.sendContent("</table>\n");
      webServer.sendContent(HTML_FOOTER);
   });
   
   /*
   * ***********************
   * *** Switch log page ***
   * ***********************
   */
   webServer.on("/switchLog", [](){
      char buffer[32];

      webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
      webServer.send(200, "text/html", "");
      sendHtmlHeader("60", "/switchLog", "switchLog");

      webServer.sendContent(
         "<H2>Schaltprotokoll</H2>\n"
         "<table>\n<tr><th>#</th><th>Laufzeit</th><th>Datum</th><th>Tag</th><th>Uhrzeit</th><th>Kanal</th><th>Ereignis</th><th>Quelle</th></tr>\n"
      );
      
      // Log-Tabelle
      uint32_t start   = switchLogEntries <= LOG_SIZE ? 0                :  switchLogEntries % LOG_SIZE,
               end     = switchLogEntries <= LOG_SIZE ? switchLogEntries : (switchLogEntries % LOG_SIZE) + LOG_SIZE;
      
      for (uint32_t i=start; i<end; i++){
         time_t timestamp = 0;
         if (switchLogRingbuffer[i % LOG_SIZE].timestampValid)
            timestamp = switchLogRingbuffer[i % LOG_SIZE].timestamp;
         else if (dateValid && timeValid)
            timestamp = bootTime + switchLogRingbuffer[i % LOG_SIZE].uptimeSeconds;
         
         webServer.sendContent("<tr><td>");
         sendUInt(switchLogRingbuffer[i % LOG_SIZE].entry + 1);
         webServer.sendContent("</td><td>");
         getUptimeString(buffer, sizeof(buffer), switchLogRingbuffer[i % LOG_SIZE].uptimeSeconds);
         webServer.sendContent(buffer);
         webServer.sendContent("</td><td>");
         if (timestamp > 0){
            getDateString(buffer, sizeof(buffer), timestamp);
            webServer.sendContent(buffer);
         }
         else
            webServer.sendContent("-");
         webServer.sendContent("</td><td>");
         if (timestamp > 0){
            getWeekdayString(buffer, sizeof(buffer), timestamp);
            webServer.sendContent(buffer);
         }
         else
            webServer.sendContent("-");
         webServer.sendContent("</td><td>");
         if (timestamp > 0){
            getTimeString(buffer, sizeof(buffer), timestamp);
            webServer.sendContent(buffer);
         }
         else
            webServer.sendContent("-");
         webServer.sendContent("</td><td>");
         sendUInt(switchLogRingbuffer[i % LOG_SIZE].channel + 1);
         webServer.sendContent("</td>");
         webServer.sendContent("<td class=\"");

         if (switchLogRingbuffer[i % LOG_SIZE].type == SWITCH_LOG_OFF)
            webServer.sendContent("red");
         else if (switchLogRingbuffer[i % LOG_SIZE].type == SWITCH_LOG_ON)
            webServer.sendContent("green");
         else
            webServer.sendContent("orange");

         webServer.sendContent("\">");
         webServer.sendContent(switchLogRingbuffer[i % LOG_SIZE].type);
         webServer.sendContent("</td><td>");

         const char* message = switchLogRingbuffer[i % LOG_SIZE].message;
         const uint8_t *ga = switchLogRingbuffer[i % LOG_SIZE].ga;
         if (message != 0)
            webServer.sendContent(message);
         else if (ga != 0){
            snprintf(buffer, sizeof(buffer), "%u/%u/%u", ga[0], ga[1], ga[2]);
            webServer.sendContent(buffer);
         }
         else
            webServer.sendContent("-");

         webServer.sendContent("</td></tr>\n");
      }
      
      webServer.sendContent("</table>\n");
      webServer.sendContent(HTML_FOOTER);
    });
   
   /*
   * ***************************
   * *** Connection log page ***
   * ***************************
   */
   webServer.on("/connectionLog", [](){
      char buffer[32];

      webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
      webServer.send(200, "text/html", "");
      sendHtmlHeader("60", "/connectionLog", "connectionLog");

      webServer.sendContent(
         "<H2>Verbindungsprotokoll</H2>\n"
         "<table>\n<tr><th>#</th><th>Laufzeit</th><th>Datum</th><th>Tag</th><th>Uhrzeit</th><th>BSSID</th><th>Kanal</th><th>Ereignis</th></tr>\n"
      );
      
      // Log-Tabelle
      uint32_t start   = connectionLogEntries <= LOG_SIZE ? 0                    :  connectionLogEntries % LOG_SIZE,
               end     = connectionLogEntries <= LOG_SIZE ? connectionLogEntries : (connectionLogEntries % LOG_SIZE) + LOG_SIZE;   
         
      for (uint32_t i=start; i<end; i++){         
         time_t timestamp = 0;
         if (connectionLogRingbuffer[i % LOG_SIZE].timestampValid)
            timestamp = connectionLogRingbuffer[i % LOG_SIZE].timestamp;
         else if (dateValid && timeValid)
            timestamp = bootTime + connectionLogRingbuffer[i % LOG_SIZE].uptimeSeconds;
         
         webServer.sendContent("<tr><td>");
         sendUInt(connectionLogRingbuffer[i % LOG_SIZE].entry + 1);
         webServer.sendContent("</td><td>");
         getUptimeString(buffer, sizeof(buffer), connectionLogRingbuffer[i % LOG_SIZE].uptimeSeconds);
         webServer.sendContent(buffer);
         webServer.sendContent("</td><td>");

         if (timestamp > 0){
            getDateString(buffer, sizeof(buffer), timestamp);
            webServer.sendContent(buffer);
         }
         else
            webServer.sendContent("-");
         webServer.sendContent("</td><td>");
         if (timestamp > 0){
            getWeekdayString(buffer, sizeof(buffer), timestamp);
            webServer.sendContent(buffer);
         }
         else
            webServer.sendContent("-");
         webServer.sendContent("</td><td>");
         if (timestamp > 0){
            getTimeString(buffer, sizeof(buffer), timestamp);
            webServer.sendContent(buffer);
         }
         else
            webServer.sendContent("-");

         webServer.sendContent("</td><td>");
         sendMAC(connectionLogRingbuffer[i % LOG_SIZE].wlanBssid);
         webServer.sendContent("</td><td>");
         sendUInt(connectionLogRingbuffer[i % LOG_SIZE].wlanChannel);
         webServer.sendContent("</td>");
         if (connectionLogRingbuffer[i % LOG_SIZE].message == LOG_KNXD_CONNECTION_CONFIRMED)
            webServer.sendContent("<td class=\"green\">");
         else
            webServer.sendContent("<td>");
         webServer.sendContent(connectionLogRingbuffer[i % LOG_SIZE].message);
         webServer.sendContent("</td></tr>\n");         
      }
      
      webServer.sendContent("</table>\n");
      webServer.sendContent(HTML_FOOTER);
   });
   
   /*
   * ************************
   * *** Air quality page ***
   * ************************
   */
   #if SCD30_ENABLE == true
      webServer.on("/SCD30", [](){
         char buffer[32];

         webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
         webServer.send(200, "text/html", "");
         sendHtmlHeader("60", "/SCD30", "SCD30");

         if (airSensorSCD30Connected && !airSensorSCD30Stuck){
            webServer.sendContent(
               "<H2>Luftqualit&auml;t</H2>\n"
            
               "<table>\n"
               
               "<tr>"
               "<td>Temperatur</td>"
               "<td>"
            );
            sendFloat(airTemperature);
            webServer.sendContent( 
                " &deg;C</td>"
               "</tr>\n"
               
               "<tr>"
               "<td>Luftfeuchtigkeit</td>"
               "<td>"
            );
            sendFloat(airHumidity);
            webServer.sendContent(
                " %</td>"
               "</tr>\n"
               
               "<tr>"
               "<td>CO<sub>2</sub></td>"
               "<td>"
            );
            sendUInt(airCO2);
            webServer.sendContent(
                " ppm</td>"
               "</tr>\n"
               
               "</table>\n"
               
               "<H2>SCD30 kalibrieren</H2>\n"
               "<p>Hier kann der Sensor auf ");
            sendUInt(AIR_SENSOR_CO2_CALIBRATION_PPM);
            webServer.sendContent(
                " ppm kalibriert werden. Daf&uuml;r muss er seit mind. 2 Minuten von frischer Luft umgeben sein!</p>\n"
               "<a href=\"SCD30_Calibration\" title=\"SCD30 kalibrieren\"><button>Kalibrierung starten</button></a>"
            );
         }
         else {
            webServer.sendContent("<span class=\"red\">Der SCD30 ist nicht angeschlossen oder liefert keine Messwerte!</span>\n");
         }
         webServer.sendContent(HTML_FOOTER);
      });
      
      /*
      * ******************************
      * *** SCD30 calibration page ***
      * ******************************
      */
      webServer.on("/SCD30_Calibration", [](){
         airSensorSCD30.setForcedRecalibrationFactor(AIR_SENSOR_CO2_CALIBRATION_PPM);
         Serial.printf("SCD30 calibrated successfully to an altitude of %d m and a CO2 concentration of %d ppm!\n", AIR_SENSOR_ALTITUDE_COMPENSATION_M, AIR_SENSOR_CO2_CALIBRATION_PPM);

         webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
         webServer.send(200, "text/html", "");
         sendHtmlHeader("15", "/SCD30", "SCD30");         
         webServer.sendContent(            
            "<H2>SCD30 kalibrieren</H2>\n"
            "Kalibrierung abgeschlossen!"
            "<p><a href=\"SCD30\" title=\"Zur&uuml;ck\">Zur&uuml;ck</a></p>\n"
         );
         webServer.sendContent(HTML_FOOTER);
      });
   #endif
   
   /*
   * **************************************
   * *** Relay switch and lock commands ***
   * **************************************
   */
   webServer.on("/ch1/on", [](){
      switchRelay(0, true, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/on", [](){
      switchRelay(1, true, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/on", [](){
      switchRelay(2, true, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/on", [](){
      switchRelay(3, true, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/off", [](){
      switchRelay(0, false, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/off", [](){
      switchRelay(1, false, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/off", [](){
      switchRelay(2, false, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/off", [](){
      switchRelay(3, false, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/toggle", [](){
      switchRelay(0, !relayStatus[0], false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/toggle", [](){
      switchRelay(1, !relayStatus[1], false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/toggle", [](){
      switchRelay(2, !relayStatus[2], false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/toggle", [](){
      switchRelay(3, !relayStatus[3], false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/lock", [](){
      lockRelay(0, true, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/lock", [](){
      lockRelay(1, true, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/lock", [](){
      lockRelay(2, true, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/lock", [](){
      lockRelay(3, true, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/unlock", [](){
      lockRelay(0, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/unlock", [](){
      lockRelay(1, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/unlock", [](){
      lockRelay(2, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/unlock", [](){
      lockRelay(3, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/toggleLock", [](){
      lockRelay(0, !lockActive[0], SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/toggleLock", [](){
      lockRelay(1, !lockActive[1], SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/toggleLock", [](){
      lockRelay(2, !lockActive[2], SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/toggleLock", [](){
      lockRelay(3, !lockActive[3], SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", "/", true);
      webServer.send(302, "text/plain", "");
   });
   
   /*
   * ********************
   * *** Relay states ***
   * ********************
   */
   webServer.on("/ch1/state", [](){webServer.send(200, "text/plain", relayStatus[0] ? "on" : "off");});
   webServer.on("/ch2/state", [](){webServer.send(200, "text/plain", relayStatus[1] ? "on" : "off");});
   webServer.on("/ch3/state", [](){webServer.send(200, "text/plain", relayStatus[2] ? "on" : "off");});
   webServer.on("/ch4/state", [](){webServer.send(200, "text/plain", relayStatus[3] ? "on" : "off");});
   
   /*
   * **************
   * *** Reboot ***
   * **************
   */
   webServer.on("/reboot", [](){
      webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
      webServer.send(200, "text/html", "");
      sendHtmlHeader("15", "/", "reboot");

      webServer.sendContent("<H2>Modul wird neugestartet...</H2>\n");
      webServer.sendContent(HTML_FOOTER);
      
      delay(1000);
      ESP.restart();
   });
   
   /*
   * *****************
   * *** Debugging ***
   * *****************
   */
   // webServer.on("/telegramtimeout", [](){
   //    webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
   //    webServer.send(200, "text/html", "");
   //    sendHtmlHeader("15", "/", "");
   //    webServer.sendContent("<H2>Telegramm-Timeout!</H2>\n");
   //    webServer.sendContent(HTML_FOOTER);
   //    lastTelegramReceivedMillis = currentMillis - (MISSING_TELEGRAM_TIMEOUT_MIN * 60000);
   // });
   
   // webServer.on("/disconnectwlan", [](){
   //    webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
   //    webServer.send(200, "text/html", "");
   //    sendHtmlHeader("15", "/", "");
   //    webServer.sendContent("<H2>Trenne WLAN!</H2>\n");
   //    webServer.sendContent(HTML_FOOTER);
   //    WiFi.disconnect();
   // });
   
   // webServer.on("/disconnectknxd", [](){
   //    webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
   //    webServer.send(200, "text/html", "");
   //    sendHtmlHeader("15", "/", "");
   //    webServer.sendContent("<H2>Trenne KNXD!</H2>\n");
   //    webServer.sendContent(HTML_FOOTER);
   //    knxdClient.stop();
   // });
   
   // webServer.on("/wlantimeout", [](){
   //    webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
   //    webServer.send(200, "text/html", "");
   //    sendHtmlHeader("15", "/", "");
   //    webServer.sendContent("<H2>WLAN-Timeout!</H2>\n");
   //    webServer.sendContent(HTML_FOOTER);
   //    wifiState = WifiState::Connected;
   // });
      
   httpUpdateServer.setup(&webServer);
   
   webServer.begin();
   Serial.println("Webserver gestartet");
}
