const String HTML_FOOTER =  "</body>\n"
                            "</html>";


String getHtmlHeader(uint8_t refreshRate, String refreshUrl, String bodyId){
   return
         "<!DOCTYPE HTML>\n"
         "<html>\n"
         "<head>\n"
            "<meta charset=\"utf-8\"/>\n"
            "<meta http-equiv=\"refresh\" content=\"" + String(refreshRate) + "; URL=" + refreshUrl + "\">\n"
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
                  "width: 150px;"
                  "text-align: center;"
                  "display: inline-block;"
                  "font-weight: bold;"
                  "text-decoration: none;"
                  "}\n"
               "body#maintenance a.box#maintenanceA, body#main a.box#mainA, body#connectionLog a.box#connectionLogA, body#switchLog a.box#switchLogA, body#SCD30 a.box#SCD30A, a.box:hover {background-color: black;}\n"
            "</style>\n"
         "</head>\n"
         "<body id=\"" + bodyId + "\">\n"
         "<H1><a href=\"/\" style=\"color: inherit\">" + HOST_NAME + " (" + HOST_DESCRIPTION + ")</a></H1>\n"
         
         "<a href=\"maintenance\" title=\"Ger&auml;tewartung\" "
         + String(knxdConnectionConfirmed
            ? "class=\"green\">Das Modul ist mit dem knxd verbunden!"
            : "class=\"red\">Das Modul ist nicht mit dem knxd verbunden!"
           ) +
         "</a>\n"
         
         "<p>\n"
         "<a class=\"box\" id=\"mainA\"          href=\"..\"            title=\"Schaltstatus\">Schaltstatus</a>\n"
         "<a class=\"box\" id=\"maintenanceA\"   href=\"maintenance\"   title=\"Ger&auml;tewartung\">Ger&auml;tewartung</a>\n"
         "<a class=\"box\" id=\"switchLogA\"     href=\"switchLog\"     title=\"Schaltprotokoll\">Schaltprotokoll</a>\n"
         "<a class=\"box\" id=\"connectionLogA\" href=\"connectionLog\" title=\"Verbindungsprotokoll\">Verbindungsprotokoll</a>\n"
         #if SCD30_ENABLE == true
         "<a class=\"box\" id=\"SCD30A\"         href=\"SCD30\"         title=\"Luftqualit&auml;t\">Luftqualit&auml;t</a>\n"
         #endif
         "</p>\n";
}


void setupWebServer(){
   webServer.on("/", [](){
      webServer.send(200, "text/html",
         getHtmlHeader(15, "/", "main") +
         
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
         
         + HTML_FOOTER
      );
   });
   
   webServer.on("/maintenance", [](){
      String gaSwitchString[CHANNELS],
             gaLockString[CHANNELS],
             gaStatusString[CHANNELS],
             gaTimeString,
             gaDateString,
             bootDateTime = "";
      const uint32_t sketchSize       = ESP.getSketchSize(),
                     freeSketchSpace  = ESP.getFreeSketchSpace(),
                     sketchPercentage = 100 * sketchSize / freeSketchSpace;

      
      if (dateValid && timeValid){         
         bootDateTime = String(" (seit ") + getDateString(bootTime) + " " + getTimeString(bootTime) + ")";
      }

      for (uint8_t ch=0; ch<CHANNELS; ch++){
         for (uint8_t i=0; i<GA_SWITCH_COUNT; i++){
            if (GA_SWITCH[ch][i][0] + GA_SWITCH[ch][i][1] + GA_SWITCH[ch][i][2] > 0)
               gaSwitchString[ch] += String(GA_SWITCH[ch][i][0]) + "/" + String(GA_SWITCH[ch][i][1]) + "/" + String(GA_SWITCH[ch][i][2]) + "<br />";
         }

         for (uint8_t i=0; i<GA_LOCK_COUNT; i++){
            if (GA_LOCK[ch][i][0] + GA_LOCK[ch][i][1] + GA_LOCK[ch][i][2] > 0)
               gaLockString[ch] += String(GA_LOCK[ch][i][0]) + "/" + String(GA_LOCK[ch][i][1]) + "/" + String(GA_LOCK[ch][i][2]) + "<br />";
         }

         if (GA_STATUS[ch][0] + GA_STATUS[ch][1] + GA_STATUS[ch][2] > 0)
            gaStatusString[ch] += String(GA_STATUS[ch][0]) + "/" + String(GA_STATUS[ch][1]) + "/" + String(GA_STATUS[ch][2]);
         
         gaTimeString = "<tr><td>Zeit</td>"
            "<td>" + String(GA_TIME[0]) + "/" + String(GA_TIME[1]) + "/" + String(GA_TIME[2]) + "</td>"
            "</tr>\n";
         
         gaDateString = "<tr><td>Datum</td>"
            "<td>" + String(GA_DATE[0]) + "/" + String(GA_DATE[1]) + "/" + String(GA_DATE[2]) + "</td>"
            "</tr>\n";
      }
      
      webServer.send(200, "text/html",
         getHtmlHeader(60, "/maintenance", "maintenance") +
         
         "<H2>Ger&auml;tewartung</H2>\n"
         
         "<table>\n"
         "<tr><td>Laufzeit</td>"
         "<td>" + getUptimeString(getUptimeSeconds()) + bootDateTime + "</td></tr>\n"
         "<tr><td>Letzter Reset</td>"
         "<td>" + ESP.getResetReason() + "</td></tr>\n"
         "<tr><td>Firmware</td>"
         "<td><a href=\"https://github.com/McOrvas/sonoff-knxd\" target=\"_blank\" rel=\"noopener noreferrer\">sonoff-knxd</a> (" + SOFTWARE_VERSION + ")</td></tr>\n"
         "<tr><td>SDK-Version</td>"
         "<td>" + ESP.getSdkVersion() + "</td></tr>\n"
         "<tr><td>Zusammenhängend / insgesamt freier Heap</td>"
         "<td>" + formatNumberHTML(ESP.getMaxFreeBlockSize()) + " / " + formatNumberHTML(ESP.getFreeHeap()) + " Byte (" + ESP.getHeapFragmentation() + " % fragmentiert)</td></tr>\n"
         "<tr><td>Aktuelle / maximale Sketch-Gr&ouml;&szlig;e</td>"
         "<td>" + formatNumberHTML(sketchSize) + " / " + formatNumberHTML(freeSketchSpace) + " Byte ("+ sketchPercentage +" % verwendet)</td></tr>\n"
         "<tr><td>Flash-Gr&ouml;&szlig;e / -Geschwindigkeit</td>"
         "<td>" + formatNumberHTML(ESP.getFlashChipSize()) + " Byte / " + ESP.getFlashChipSpeed() / 1000000 + " MHz</td></tr>\n"
         "<tr><td>Prozessortakt</td>"
         "<td>" + ESP.getCpuFreqMHz() + " MHz (" + formatNumberHTML(loopsPerSecond) + " loops/s)</td></tr>\n"
         "</table>\n"

         "<p><a href=\"update\" title=\"Firmware aktualisieren\"><button>Firmware aktualisieren</button></a>"
         "&nbsp;<a href=\"reboot\" title=\"Ger&auml;t neu starten\"><button>Ger&auml;t neu starten</button></a></p>\n"
         
         "<H2>Netzwerk</H2>\n"
         
         "<table>\n"
         "<tr><td>IP-Adresse</td><td>" + WiFi.localIP().toString() + "</td></tr>\n"
         "<tr><td>Netzmaske</td><td>"  + WiFi.subnetMask().toString() + "</td></tr>\n"
         "<tr><td>Gateway</td><td>"    + WiFi.gatewayIP().toString() + "</td></tr>\n"
         "<tr><td>MAC</td><td>"        + WiFi.macAddress() + "</td></tr>\n"
         "<tr><td>SSID</td><td>"       + String(WiFi.SSID()) + "</td></tr>\n"
         "<tr><td>BSSID</td><td>"      + WiFi.BSSIDstr() + "</td></tr>\n"
         "<tr><td>Kanal</td><td>"      + String(WiFi.channel()) + "</td></tr>\n"
         "<tr><td>RSSI</td><td>"       + String(WiFi.RSSI()) + " dBm</td></tr>\n"
         "<tr><td>knxd</td><td>"       + String(KNXD_IP) + "</td></tr>\n"
         "</table>\n"
         
         "<H2>KNX-Konfiguration</H2>\n"
         
         "<table>\n"
         "<tr>"
         "<th>Gruppenadressen</th>"
         "<th>Kanal 1</th>"
         + String(CHANNELS > 1 ? "<th>Kanal 2</th>" : "")
         + String(CHANNELS > 2 ? "<th>Kanal 3</th>" : "")
         + String(CHANNELS > 3 ? "<th>Kanal 4</th>" : "") +
         "</tr>\n"
         
         "<tr><td>Schalten</td>"
         "<td>" + gaSwitchString[0] + "</td>"
         + String(CHANNELS > 1 ? "<td>" + gaSwitchString[1] + "</td>" : "")
         + String(CHANNELS > 2 ? "<td>" + gaSwitchString[2] + "</td>" : "")
         + String(CHANNELS > 3 ? "<td>" + gaSwitchString[3] + "</td>" : "") +
         "</tr>\n"
         
         "<tr><td>Sperren</td>"
         "<td>" + gaLockString[0] + "</td>"
         + String(CHANNELS > 1 ? "<td>" + gaLockString[1] + "</td>" : "")
         + String(CHANNELS > 2 ? "<td>" + gaLockString[2] + "</td>" : "")
         + String(CHANNELS > 3 ? "<td>" + gaLockString[3] + "</td>" : "") +
         "</tr>\n"
         
         "<tr><td>Status</td>"
         "<td>" + gaStatusString[0] + "</td>"
         + String(CHANNELS > 1 ? "<td>" + gaStatusString[1] + "</td>" : "")
         + String(CHANNELS > 2 ? "<td>" + gaStatusString[2] + "</td>" : "")
         + String(CHANNELS > 3 ? "<td>" + gaStatusString[3] + "</td>" : "") +
         "</tr>\n"
         
         + String(GA_DATE_VALID ? gaDateString : "")
         + String(GA_TIME_VALID ? gaTimeString : "") +
         
         #if SCD30_ENABLE == true
         "<tr><td>Temperatur</td>"
         "<td>" + GA_AIR_TEMPERATURE[0] + "/" + GA_AIR_TEMPERATURE[1] + "/" + GA_AIR_TEMPERATURE[2] + "</td></tr>\n"
         "<tr><td>Luftfeuchtigkeit</td>"
         "<td>" + GA_AIR_HUMIDITY[0] + "/" + GA_AIR_HUMIDITY[1] + "/" + GA_AIR_HUMIDITY[2] + "</td></tr>\n"
         "<tr><td>CO<sub>2</sub></td>"
         "<td>" + GA_AIR_CO2[0] + "/" + GA_AIR_CO2[1] + "/" + GA_AIR_CO2[2] + "</td></tr>\n"
         #endif
         
         "</table>\n"
         
         "<H2>Verbindungsstatistik</H2>\n"
         
         "<table>\n"
         "<tr><th colspan=\"2\">Verbindungsaufbau zum knxd</th></tr>\n"
         "<tr><td>Fehlgeschlagene Verbindungsanfragen</td>"
         "<td>" + String(knxdConnectionFailedCount) + "</td></tr>\n"
         "<tr><td>Erfolgreiche Verbindungsanfragen</td>"
         "<td>" + String(knxdConnectionInitiatedCount) + "</td></tr>\n"
         "<tr><td>Zeit&uuml;berschreitungen bei der Verbindungsbest&auml;tigung durch den knxd (" + CONNECTION_CONFIRMATION_TIMEOUT_MS + " ms)</td>"
         "<td>" + String(knxdConnectionHandshakeTimeouts) + "</td></tr>\n"
         "<tr><td>Vom knxd best&auml;tigte Verbindungen</td>"
         "<td>" + String(knxdConnectionConfirmedCount) + "</td></tr>\n"         
         
         "<tr><th colspan=\"2\">Trennung bestehender Verbindungen zum knxd</th></tr>\n"
         "<tr><td>Verbindungsabbr&uuml;che wegen unvollständig empfangener Telegramme (" + INCOMPLETE_TELEGRAM_TIMEOUT_MS + " ms)</td>"
         "<td>" + String(incompleteTelegramTimeouts) + "</td></tr>\n"
         "<tr><td>Verbindungsabbr&uuml;che wegen Zeit&uuml;berschreitung zwischen zwei Telegrammen (" + MISSING_TELEGRAM_TIMEOUT_MIN + " min)</td>"
         "<td>" + String(missingTelegramTimeouts) + "</td></tr>\n"
         "<tr><td>Verbindungsabbr&uuml;che zum WLAN</td>"
         "<td>" + String(wifiDisconnections) + "</td></tr>\n"
         "<tr><td>Sonstige Verbindungsabbr&uuml;che zum knxd</td>"
         "<td>" + String(knxdDisconnections) + "</td></tr>\n"
         
         "<tr><th colspan=\"2\">Sonstiges</th></tr>\n"
         "<tr><td>Empfangene Gruppentelegramme</td>"
         "<td>" + formatNumberHTML(receivedTelegrams) + " (&#8960; " + String(receivedTelegrams / (float) getUptimeSeconds()) +" / s)</td></tr>\n"

         + String(GA_DATE_VALID
            ? "<tr><td>Aktuelles Datum" + String(dateValid ? " (empfangen vor " + String(getUptimeString((currentMillis - dateTelegramReceivedMillis) / 1000)) + ")": "") + "</td>"
              "<td>" + String(dateValid ? getDateString(now()) : "-") + "</td></tr>\n"
            : "")
         + String(GA_TIME_VALID
            ? "<tr><td>Aktuelle Uhrzeit" + String(timeValid ? " (empfangen vor " + String(getUptimeString((currentMillis - timeTelegramReceivedMillis) / 1000)) + ")": "") + "</td>"
              "<td>" + String(timeValid ? getTimeString(now()) : "-") + "</td></tr>\n"
            : "") +
         "</table>\n"

         + HTML_FOOTER
      );
   });
   
   webServer.on("/switchLog", [](){
      webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
      webServer.send(200, "text/html", getHtmlHeader(60, "/switchLog", "switchLog"));   
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
         
         String color;
         if (switchLogRingbuffer[i % LOG_SIZE].type == SWITCH_LOG_OFF)
            color = "red";
         else if (switchLogRingbuffer[i % LOG_SIZE].type == SWITCH_LOG_ON)
            color = "green";
         else
            color = "orange";
         
         const char* message = switchLogRingbuffer[i % LOG_SIZE].message;
         const uint8_t *ga = switchLogRingbuffer[i % LOG_SIZE].ga;
         String text = "-";
         if (message != 0)
            text = message;
         else if (ga != 0)
            text = String(*ga) + "/" + String(*(ga+1)) + "/" + String(*(ga+2));
         
         webServer.sendContent(
            "<tr>"
            "<td>" + String(switchLogRingbuffer[i % LOG_SIZE].entry + 1)   + "</td>"
            "<td>" + String(getUptimeString(switchLogRingbuffer[i % LOG_SIZE].uptimeSeconds))  + "</td>"
            "<td>" + String(timestamp > 0 ? getDateString(timestamp) : "-")    + "</td>"
            "<td>" + String(timestamp > 0 ? getWeekdayString(timestamp) : "-")    + "</td>"
            "<td>" + String(timestamp > 0 ? getTimeString(timestamp) : "-")    + "</td>"
            "<td>" + String(switchLogRingbuffer[i % LOG_SIZE].channel + 1) + "</td>"
            "<td class=\"" + color + "\">" + switchLogRingbuffer[i % LOG_SIZE].type + "</td>"
            "<td>" + text + "</td>"
            "</tr>\n"
         );
      }
      
      webServer.sendContent("</table>\n");
      webServer.sendContent(HTML_FOOTER);
      webServer.sendContent("");
    });
   
   webServer.on("/connectionLog", [](){
      webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
      webServer.send(200, "text/html", getHtmlHeader(60, "/connectionLog", "connectionLog"));
      webServer.sendContent(
         "<H2>Verbindungsprotokoll</H2>\n"
         "<table>\n<tr><th>#</th><th>Laufzeit</th><th>Datum</th><th>Tag</th><th>Uhrzeit</th><th>BSSID</th><th>Kanal</th><th>Ereignis</th></tr>\n"
      );
      
      // Log-Tabelle
      uint32_t start   = connectionLogEntries <= LOG_SIZE ? 0                    :  connectionLogEntries % LOG_SIZE,
               end     = connectionLogEntries <= LOG_SIZE ? connectionLogEntries : (connectionLogEntries % LOG_SIZE) + LOG_SIZE;   
      char     bssidString[18];
         
      for (uint32_t i=start; i<end; i++){
         const uint8_t *bssid = connectionLogRingbuffer[i % LOG_SIZE].wlanBssid;
         snprintf(bssidString, 18, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
         
         time_t timestamp = 0;
         if (connectionLogRingbuffer[i % LOG_SIZE].timestampValid)
            timestamp = connectionLogRingbuffer[i % LOG_SIZE].timestamp;
         else if (dateValid && timeValid)
            timestamp = bootTime + connectionLogRingbuffer[i % LOG_SIZE].uptimeSeconds;
         
         webServer.sendContent(
            "<tr>"
            "<td>" + String(connectionLogRingbuffer[i % LOG_SIZE].entry + 1) + "</td>"
            "<td>" + String(getUptimeString(connectionLogRingbuffer[i % LOG_SIZE].uptimeSeconds))  + "</td>"
            "<td>" + String(timestamp > 0 ? getDateString(timestamp) : "-")    + "</td>"
            "<td>" + String(timestamp > 0 ? getWeekdayString(timestamp) : "-")    + "</td>"
            "<td>" + String(timestamp > 0 ? getTimeString(timestamp) : "-")    + "</td>"
            "<td>" + bssidString + "</td>"
            "<td>" + String(connectionLogRingbuffer[i % LOG_SIZE].wlanChannel) + "</td>" +
            (connectionLogRingbuffer[i % LOG_SIZE].message == LOG_KNXD_CONNECTION_CONFIRMED ? "<td class=\"green\">" : "<td>")
            + String(connectionLogRingbuffer[i % LOG_SIZE].message) + "</td>"
            "</tr>\n"
         );
      }
      
      webServer.sendContent("</table>\n");
      webServer.sendContent(HTML_FOOTER);
      webServer.sendContent("");
   });
   
   #if SCD30_ENABLE == true
   webServer.on("/SCD30", [](){
      if (airSensorSCD30Connected && !airSensorSCD30Stuck){
         webServer.send(200, "text/html",
            getHtmlHeader(60, "/SCD30", "SCD30") +
            
            "<H2>Luftqualit&auml;t</H2>\n"
         
            "<table>\n"
            
            "<tr>"
            "<td>Temperatur</td>"
            "<td>" + airTemperature + " &deg;C</td>"
            "</tr>\n"
            
            "<tr>"
            "<td>Luftfeuchtigkeit</td>"
            "<td>" + airHumidity + " %</td>"
            "</tr>\n"
            
            "<tr>"
            "<td>CO<sub>2</sub></td>"
            "<td>" + airCO2 + " ppm</td>"
            "</tr>\n"
            
            "</table>\n"
            
            "<H2>SCD30 kalibrieren</H2>\n"
            "<p>Hier kann der Sensor auf " + AIR_SENSOR_CO2_CALIBRATION_PPM + " ppm kalibriert werden. Daf&uuml;r muss er seit mind. 2 Minuten von frischer Luft umgeben sein!</p>\n"
            "<a href=\"SCD30_Calibration\" title=\"SCD30 kalibrieren\"><button>Kalibrierung starten</button></a>"
            
            + HTML_FOOTER
         );
      }
      else {
         webServer.send(200, "text/html",
            getHtmlHeader(60, "/SCD30", "SCD30") +
            
            "<span class=\"red\">Der SCD30 ist nicht angeschlossen oder liefert keine Messwerte!</span>\n"
            
            + HTML_FOOTER
         );
      }
   });
   
   webServer.on("/SCD30_Calibration", [](){
      airSensorSCD30.setForcedRecalibrationFactor(AIR_SENSOR_CO2_CALIBRATION_PPM);
      Serial.printf("SCD30 calibrated successfully to an altitude of %d m and a CO2 concentration of %d ppm!\n", AIR_SENSOR_ALTITUDE_COMPENSATION_M, AIR_SENSOR_CO2_CALIBRATION_PPM);
      
      webServer.send(200, "text/html",
         getHtmlHeader(15, "/SCD30", "SCD30") +
         
         "<H2>SCD30 kalibrieren</H2>\n"
         "Kalibrierung abgeschlossen!"
         "<p><a href=\"SCD30\" title=\"Zur&uuml;ck\">Zur&uuml;ck</a></p>\n"
         
         + HTML_FOOTER
      );
   });
   #endif
   
   webServer.on("/ch1/on", [](){
      switchRelay(0, true, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/on", [](){
      switchRelay(1, true, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/on", [](){
      switchRelay(2, true, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/on", [](){
      switchRelay(3, true, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/off", [](){
      switchRelay(0, false, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/off", [](){
      switchRelay(1, false, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/off", [](){
      switchRelay(2, false, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/off", [](){
      switchRelay(3, false, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/toggle", [](){
      switchRelay(0, !relayStatus[0], false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/toggle", [](){
      switchRelay(1, !relayStatus[1], false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/toggle", [](){
      switchRelay(2, !relayStatus[2], false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/toggle", [](){
      switchRelay(3, !relayStatus[3], false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/lock", [](){
      lockRelay(0, true, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/lock", [](){
      lockRelay(1, true, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/lock", [](){
      lockRelay(2, true, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/lock", [](){
      lockRelay(3, true, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/unlock", [](){
      lockRelay(0, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/unlock", [](){
      lockRelay(1, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/unlock", [](){
      lockRelay(2, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/unlock", [](){
      lockRelay(3, false, SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/toggleLock", [](){
      lockRelay(0, !lockActive[0], SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch2/toggleLock", [](){
      lockRelay(1, !lockActive[1], SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch3/toggleLock", [](){
      lockRelay(2, !lockActive[2], SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch4/toggleLock", [](){
      lockRelay(3, !lockActive[3], SWITCH_LOG_WEBSERVER, 0);
      webServer.sendHeader("Location", String("/"), true);
      webServer.send(302, "text/plain", "");
   });
   
   webServer.on("/ch1/state", [](){webServer.send(200, "text/plain", relayStatus[0] ? "on" : "off");});
   webServer.on("/ch2/state", [](){webServer.send(200, "text/plain", relayStatus[1] ? "on" : "off");});
   webServer.on("/ch3/state", [](){webServer.send(200, "text/plain", relayStatus[2] ? "on" : "off");});
   webServer.on("/ch4/state", [](){webServer.send(200, "text/plain", relayStatus[3] ? "on" : "off");});
   
   webServer.on("/reboot", [](){
      webServer.send(200, "text/html", getHtmlHeader(15, "/", "reboot") + "<H2>Modul wird neugestartet...</H2>\n" + HTML_FOOTER);
      delay(1000);
      ESP.restart();
   });
   
   /*webServer.on("/telegramtimeout", [](){
      webServer.send(200, "text/html", getHtmlHeader(15, "/", "") + "<H2>Telegramm-Timeout!</H2>\n" + HTML_FOOTER);
      lastTelegramReceivedMillis = currentMillis - (MISSING_TELEGRAM_TIMEOUT_MIN * 60000);
   });
   
   webServer.on("/disconnectwlan", [](){
      webServer.send(200, "text/html", getHtmlHeader(15, "/", "") + "<H2>Trenne WLAN!</H2>\n" + HTML_FOOTER);
      WiFi.disconnect();
   });
   
   webServer.on("/disconnectknxd", [](){
      webServer.send(200, "text/html", getHtmlHeader(15, "/", "") + "<H2>Trenne KNXD!</H2>\n" + HTML_FOOTER);
      client.stop();
   });
   
   webServer.on("/wlantimeout", [](){
      webServer.send(200, "text/html", getHtmlHeader(15, "/", "") + "<H2>WLAN-Timeout!</H2>\n" + HTML_FOOTER);
      wifiConnected = 2;
   });*/
      
   httpUpdateServer.setup(&webServer);
   
   webServer.begin();
   Serial.println("Webserver gestartet");
}
