  // Captive portal with (arduino) + JQUERY + OTA + SPIFFS Example 
  //
  // PREREQUISITES
  //
  // You need Arduino IDE 1.8.19 + ESP8266 Filesystem Uploader or manually upload spiffs image (using makespiffs and esptool)!
  //
  // Find the ESP8266 Filesytem Uploader here: https://github.com/esp8266/arduino-esp8266fs-plugin
  //
  // CC dennis de bel 2020-2023
  //
  // NOTES FOR DEV
  // Apple captive portal issues: https://community.arubanetworks.com/t5/Controller-Based-WLANs/Apple-CNA-take-a-long-time-to-change-the-Cancel-Button-to-Done/ta-p/267612
  
  #include <Arduino.h>
  #include <ESP8266WiFi.h>
  #include <ESP8266mDNS.h>
  #include <WiFiUdp.h>
  #include <ArduinoOTA.h> // Over-the-Air updates
  #include <ESP8266WebServer.h>
  #include "./DNSServer.h" // Dns server
  #include <FS.h> // SPIFFS
  #include <EEPROM.h> // for storing state flags in memory
  #define EEPROM_SIZE 1  // define the number of bytes you want to access

 
  DNSServer dnsServer;
  const byte DNS_PORT = 53;
  
  ESP8266WebServer server(80);

  #ifndef STASSID
  #define STASSID "\xF0\x9F\xA6\x92 example ssid \xF0\x9F\xA6\x92"
  #endif

  IPAddress apIP(8, 8, 8, 8);
  const char* ssid = STASSID;

  void setup() {
    Serial.begin(115200);
    EEPROM.begin(EEPROM_SIZE);
    Serial.println("Booting");

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(ssid);
    dnsServer.start(DNS_PORT, "*", apIP); // redirect dns request to AP ip
    
    MDNS.begin("opencoil", WiFi.softAPIP());
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());

    //Over-the-Air updates
    ArduinoOTA.setHostname("test"); // connect to http://test.local
    //ArduinoOTA.setPassword("&0_029100)jJapeM"); //enabling password disables SPIFFS upload
    ArduinoOTA.begin();
    SPIFFS.begin();

    //close the captive portal screen on ios! (does not work anymore.. somehow..still allows for faking internet connection but not for closing CNA automatically)
    server.on("/CONNECT", closeCNAdelay);

    //restart esp after donwloading (hack to make sure captive portal re-opens again when a new user connects to the portal)
    server.on("/RESTART", restartESP);


    server.on("/generate_204", androidRedirect); //Adnroid captive portal, create notification and pops up android cna, but does not connect or go away after...needs closeCNAdelay like handler
    //server.on("/favicon.ico", androidRedirect);    //Another Android captive portal
    server.on("/fwlink", androidRedirect);  //Microsoft captive portal
    
    //if page is not found, redirect traffic to index.html (cheap way to redirect all traffic to our landing page)
    server.onNotFound([]() {
      if(!handleFileRead(server.uri())){
        const char *metaRefreshStr = "<head><meta http-equiv=\"refresh\" content=\"0; url=http://8.8.8.8/index.html\" /></head><body><h1 id='blink' style='font-family:sans-serif'>Loading...</h1><script type='text/javascript'> var blink = document.getElementById(\'blink\'); setInterval(function () blink.style.opacity = (blink.style.opacity == 0 ? 1 : 0);}, 1000); </script></body>";
        server.send(200, "text/html", metaRefreshStr);
      }
    });
  
    server.begin();

  }

  void loop() {
    dnsServer.processNextRequest();
    ArduinoOTA.handle();
    server.handleClient();
    delay(50);
  }


String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".mp4")) return "video/mp4";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  else if(filename.endsWith(".woff2")) return "font/woff2";
  else if(filename.endsWith(".woff")) return "font/woff";
  else if(filename.endsWith(".ttf")) return "font/ttf";
  else if(filename.endsWith(".eot")) return "application/vnd.ms-fontobject";
  else if(filename.endsWith(".json")) return "application/json";
  return "text/plain";
}

//Given a file path, look for it in the SPIFFS file storage. Returns true if found, returns false if not found.
bool handleFileRead(String path){
  if(path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleAppleCaptivePortal() {
  String Page = F("<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
  server.sendHeader("Access-Control-Allow-Origin", "*", 1);
  server.sendHeader("Cache-Control","no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(200, "text/html", Page);
  return;
  }

  
void closeCNAdelay(){ 
  // write to fil or memory that the function has been executed
  server.on("/hotspot-detect.html", handleAppleCaptivePortal); //only for ios, osx asks for random urls, issue: this stays 'open' for ever...also for new clients..
  //delay(1000);
  //ESP.restart();
  return;
}

void restartESP(){ 
  //delay(1000); //for spiffs size < 300kb
  delay(5000);
  ESP.restart();
  return;
}

void androidRedirect(){
  String Page = F("<head><meta http-equiv=\"refresh\" content=\"0; url=http://8.8.8.8/index.html\" /></head><body><p>redirecting...</p></body>");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");   
  server.sendHeader("Pragma", "no-cache");   
  server.sendHeader("Expires", "-1");  
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);  
  server.send ( 200, "text/html", Page );  
}
