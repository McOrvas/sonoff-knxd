const String HTML_FOOTER =  "</body>\n"
                            "</html>";


String getHtmlHeader(uint8_t refreshRate, String refreshUrl){
   return
         "<!DOCTYPE HTML>\n"
         "<html>\n"
         "<head>\n"
            "<meta charset=\"utf-8\"/>\n"
            "<meta http-equiv=\"refresh\" content=\"" + String(refreshRate) + "; URL=" + refreshUrl + "\">\n"
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
         "<body>\n"
         "<H1><a href=\"/\" style=\"color: inherit\">" + HOST_NAME + " (" + HOST_DESCRIPTION + ")</a></H1>\n";
}


String getKnxdStatusString(){
      return
         "<a href=\"maintenance\" title=\"Ger&auml;tewartung\" "
         + String(client.connected()
            ? "class=\"green\">Das Modul ist mit dem knxd verbunden!"
            : "class=\"red\">Das Modul ist nicht mit dem knxd verbunden!"
           ) +
         "</a>";
}


String getWebServerMainPage() {
   return
         getHtmlHeader(15, "/") +
         
         getKnxdStatusString() + "\n"
         
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
               
         "<p><a href=\"maintenance\" title=\"Ger&auml;tewartung\">Ger&auml;tewartung</a></p>\n"
         
         + HTML_FOOTER;
}


String getWebServerMaintenancePage() {
   String gaSwitchString[CHANNELS],
          gaLockString[CHANNELS],
          gaStatusString[CHANNELS],
          gaTimeString,
          gaDateString;

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

   return
         getHtmlHeader(60, "/maintenance") +
         
         "<table>\n"
         "<tr><td><a href=\"update\" title=\"Firmware aktualisieren\">Firmware</a></td>"
         "<td><a href=\"https://github.com/McOrvas/sonoff-knxd\">sonoff-knxd</a> (" + SOFTWARE_VERSION + ")</td></tr>\n"
         "<tr><td><a href=\"reboot\" title=\"Neustart\">Laufzeit</a></td></td>"
         "<td>" + getUptimeString(getUptimeSeconds()) + "</td></tr>\n"
         "</table>\n"
         
         "<H2>Netzwerk</H2>\n"
         
         "<table>\n"
         "<tr><td>IP-Adresse</td><td>" + WiFi.localIP().toString() + "</td></tr>\n"
         "<tr><td>Netzmaske</td><td>"  + WiFi.subnetMask().toString() + "</td></tr>\n"
         "<tr><td>Gateway</td><td>"    + WiFi.gatewayIP().toString() + "</td></tr>\n"
         "<tr><td>MAC</td><td>"        + WiFi.macAddress() + "</td></tr>\n"
         "<tr><td>knxd</td><td>"    + String(KNXD_IP) + "</td></tr>\n"
         "<tr><td>SSID</td><td>"       + String(WiFi.SSID()) + "</td></tr>\n"
         "<tr><td>RSSI</td><td>"       + String(WiFi.RSSI()) + " dBm</td></tr>\n"
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
         "</table>\n"
         
         "<H2>Verbindungsstatistik</H2>\n"
         
         "<p>" + getKnxdStatusString() + "</p>\n"
         
         "<table>\n"
         "<tr><td>Erfolgreiche Verbindungsaufbauten zum knxd</td>"
         "<td>" + String(knxdConnectionCount) + "</td></tr>\n"         
         "<tr><td>Zeit&uuml;berschreitungen beim Verbindungsaufbau zum knxd (" + CONNECTION_CONFIRMATION_TIMEOUT_MS + " ms)</td>"
         "<td>" + String(knxdHandshakeTimeouts) + "</td></tr>\n"
         "<tr><td>Verbindungsabbr&uuml;che wegen Zeit&uuml;berschreitung zwischen zwei Telegrammen (" + MISSING_TELEGRAM_TIMEOUT_MIN + " min)</td>"
         "<td>" + String(missingTelegramTimeouts) + "</td></tr>\n"
         "<tr><td>Verbindungsabbr&uuml;che wegen unvollständig empfangener Telegramme (" + INCOMPLETE_TELEGRAM_TIMEOUT_MS + " ms)</td>"
         "<td>" + String(incompleteTelegramTimeouts) + "</td></tr>\n"
         "<tr><td>Verbindungsabbr&uuml;che zum WLAN</td>"
         "<td>" + String(wifiDisconnections) + "</td></tr>\n"
         "<tr><td>Sonstige Verbindungsabbr&uuml;che zum knxd</td>"
         "<td>" + String(knxdDisconnections) + "</td></tr>\n"
         "<tr><td>Empfangene Gruppentelegramme</td>"
         "<td>" + String(receivedTelegrams) + " (&#8960; " + String(receivedTelegrams / (float) getUptimeSeconds()) +" / s)</td></tr>\n"
         + String(GA_DATE_VALID
            ? "<tr><td>Aktuelles Datum" + String(dateValid ? " (empfangen vor " + getUptimeString((currentMillis - dateTelegramReceivedMillis) / 1000) + ")": "") + "</td>"
              "<td>" + String(dateValid ? getDateString() : "-") + "</td></tr>\n"
            : "")
         + String(GA_TIME_VALID
            ? "<tr><td>Aktuelle Zeit" + String(timeValid ? " (empfangen vor " + getUptimeString((currentMillis - timeTelegramReceivedMillis) / 1000) + ")": "") + "</td>"
              "<td>" + String(timeValid ? getUpdatedTimeString() : "-") + "</td></tr>\n"
            : "") +
         "</table>\n"
         
         "<p><a href=\"..\">Zur&uuml;ck</a></p>\n"
         + HTML_FOOTER;
}


void setupWebServer(){
   webServer.on("/", [](){
      webServer.send(200, "text/html", getWebServerMainPage());
   });
   
   webServer.on("/maintenance", [](){
      webServer.send(200, "text/html", getWebServerMaintenancePage());
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
      webServer.send(200, "text/html", getHtmlHeader(15, "/") + "Modul wird neugestartet...<p><a href=\"..\">Zur&uuml;ck</a></p>\n" + HTML_FOOTER);
      delay(1000);
      ESP.restart();
   });
   
   httpUpdateServer.setup(&webServer);
   
   webServer.begin();
   Serial.println("Webserver gestartet");
}
