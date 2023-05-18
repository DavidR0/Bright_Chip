#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <WebSocketsServer.h>
#include <EEPROM_Rotate.h>
#include <AutoConnect.h>

/*__________________________________________________________Configurable Variables__________________________________________________________*/

#define relayPin   0//4-leds  0-main  //D2
#define buttonPin  5  //D1

//Setting on what port the server to run, 81 for deployment(port 8080 for testing)
ESP8266WebServer server(81);
// create a websocket server on port 82 (port 8081 for testing)
WebSocketsServer webSocket(82);  

//Password for OTA
const char* passwordOTA = "1243";

//MDNS name
const char* mdnsName = "Bright_Lights";

/*__________________________________________________________Nonconfigurable Variables__________________________________________________________*/

//Counters for logistics
unsigned long timesLightsSwitched = 0;
unsigned long lastTimeLightTurnedON = 0;
unsigned long totalLightsOnTime = 0;

//States for current settings
bool LightOn = true;
bool cpyLightOn = LightOn;

//Security STUFF
//This bool is used to control device lockout 
bool lock = false; 

//anchars will be explained below. username will be compared with 'user' and loginPassword with 'pass' when login is done
String anchars = "abcdefghijklmnopqrstuvwxyz0123456789", username = "admin", loginPassword = "potolyourself"; 

//First 2 timers are for lockout and last one is inactivity timer
unsigned long logincld = millis(), reqmillis = millis(), tempign = millis(); 

// i is used for for index, trycount will be our buffer for remembering how many false entries there were
uint8_t i, trycount = 0; 
//this is cookie buffer
String sessioncookie; 


//Variables for deboucing
int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin

unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 15;    // the debounce time; increase if the output flickers

//EEPROM STUFF
EEPROM_Rotate EEPROMr;
#define DATA_OFFSET     10

//WebSocket STUFF
struct WebSocketconnectionInfo
{
  bool connected = false; //See if WebSocket connection is online
  unsigned long time;     //Time connection was made
};

WebSocketconnectionInfo webSocketconnection[5];

int NbOnlineWebsockets = 0;

//Wifi setup STUFF
// const IPAddress apIP(192, 168, 1, 1);

const char* ssid     = "Telekom-ahQXIv";         // The SSID (name) of the Wi-Fi network you want to connect to
const char* password = "f4kuhmk28bjv";     // The password of the Wi-Fi network

boolean settingMode;
String ssidList;

DNSServer dnsServer;
//AutoConnect      Portal(server);

/*__________________________________________________________Security_FUNCTIONS__________________________________________________________*/

void gencookie() {

  sessioncookie = "";
  //Using randomchar from anchars string generate 32-bit cookie
  for( i = 0; i < 32; i++) sessioncookie += anchars[random(0, anchars.length())]; 
}

bool is_authentified() { //This function checks for Cookie header in request, it compares variable c to sessioncookie and returns true if they are the same
  if (server.hasHeader("Cookie")){
    String cookie = server.header("Cookie"), authk = "c=" + sessioncookie;
    if (cookie.indexOf(authk) != -1) return true;
  }
  return false;
}

/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/

boolean restoreConfig() {
  Serial.println("Reading EEPROMr...");
  String ssid = "";
  String pass = "";
  if (EEPROMr.read(0 + DATA_OFFSET) != 0) {
    for (int i = 0; i < 32; ++i) {
      ssid += char(EEPROMr.read(i + DATA_OFFSET));
    }
    Serial.print("SSID: ");
    Serial.println(ssid);
    for (int i = 32; i < 96; ++i) {
      pass += char(EEPROMr.read(i + DATA_OFFSET));
    }
    Serial.print("Password: ");
    Serial.println(pass);
    WiFi.begin(ssid.c_str(), pass.c_str());
    return true;
  }
  else {
    Serial.println("Config not found.");
    return false;
  }
}

boolean checkConnection() {
  int count = 0;
  Serial.print("Waiting for Wi-Fi connection");
  while ( count < 30 ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.println("Connected!");
      return (true);
    }
    delay(500);
    Serial.print(".");
    count++;
  }
  Serial.println("Timed out.");
  return false;
}

String makePage(String title, String contents) {
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += contents;
  s += "</body></html>";
  return s;
}

String urlDecode(String input) {
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}

String getContentType(String filename){
  if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void sendStats(uint8_t num) {

  if(LightOn) { 
    totalLightsOnTime += millis() - lastTimeLightTurnedON;
    lastTimeLightTurnedON = millis();//We need to reset the value so we don't overlap with the time that was added in the totalLightsOnTime
  }

  String msgStatsChange = "Stats";
  String minutesSinceReboot = (String)(millis() / 60000);
  String LightsOnTime = (String)(totalLightsOnTime / 60000);

  //Send a websocket message with the new stats (because they changed)
  webSocket.sendTXT(num, msgStatsChange + LightsOnTime + "/" + timesLightsSwitched + "/" + minutesSinceReboot);
  webSocket.sendTXT(num,"Number of connected clients" + (String)NbOnlineWebsockets);
}

void pinHandler() {
  //If button is pressed
  // if(buttonState.phase == 2) {
  //   LightOn = !LightOn;  
  // }

  //Once btn is pressed check if it was held for at least 5 sec to reset the chip
  // if(buttonState.pressedTime >= 5000)
  // {
  //   digitalWrite(relayPin,HIGH);
  //   delay(1000);
  //   digitalWrite(relayPin,LOW);
  //   delay(1000);
  //   digitalWrite(relayPin,HIGH);
  //   delay(1000);
  //   ESP.reset();
  // }

  if(LightOn != cpyLightOn) {
    cpyLightOn = LightOn;
    ++timesLightsSwitched;

    String msgLightChange = "Ligh";

    //Send websocket message that the light state changed
    if(NbOnlineWebsockets > 0) {
      if(LightOn) { 
        msgLightChange += "1";
      } else {
        msgLightChange += "0";
      }
      //Send the change to all the connected clients
      for(int i = 0; i < 5; ++i) {
        if(webSocketconnection[i].connected)
        {
          webSocket.sendTXT(i,msgLightChange);
        }
      }
    }
    
    if(LightOn){
      digitalWrite(relayPin,LOW);
      //Save the current time when the lights are turned on
      lastTimeLightTurnedON = millis();
    } else {
      digitalWrite(relayPin,HIGH);
      //Write the total time the lights were on
      totalLightsOnTime += millis() - lastTimeLightTurnedON;
    }
    //Send the new stats to all the connected clients
    for(int i = 0; i < 5; ++i) {
      if(webSocketconnection[i].connected)
      {
        sendStats(i);
      }
    }
  }
}


//Negative Logic on the btn (0 is pressed)
void debounceBtn() {
  // read the state of the switch into a local variable:
  int reading = digitalRead(buttonPin);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;

      // only toggle the LIGHT if the new button state is HIGH
      if (buttonState == HIGH) {
        LightOn = !LightOn;
      }
    }
  }

  // save the reading. Next time through the loop, it'll be the lastButtonState:
  lastButtonState = reading;
}


/*__________________________________________________________WEBSOCKET_FUNCTIONALITY__________________________________________________________*/

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
        Serial.printf("[%u] Disconnected!\n", num);
        webSocketconnection[num].connected = false;// Mark the connection as inactive/offline
        NbOnlineWebsockets = webSocket.connectedClients(true);
      break;

    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
          webSocketconnection[num].connected = true;//Mark the connection as active/online
          webSocketconnection[num].time = millis();
          NbOnlineWebsockets = webSocket.connectedClients(true);
          
          //Make room for other clients, if too many clients are connected we remove the one with the longest connection time
          if(NbOnlineWebsockets == 5) {

            uint8_t longestConnection = 0;

            for(int i = 1; i < 5; ++i){
              if(webSocketconnection[i].time < webSocketconnection[longestConnection].time){
                longestConnection = i;
              }
              webSocket.disconnect(longestConnection);
            }
          }
      }
      break;

    // if new text data is received
    case WStype_TEXT:                     

      Serial.printf("[%u] get Text: %s\n", num, payload);

      //REQUEST COMMANDS
      /*
            -- State
        Req 
            -- Stats
      */
      if(payload[0] == 'R' && payload[1] == 'e' && payload[2] == 'q') {

        if(payload[3] == 'S' && payload[4] == 't' && payload[5] == 'a' && payload[6] == 't' && payload[7] == 's') {

          sendStats(num);

        } else if(payload[3] == 'S' && payload[4] == 't' && payload[5] == 'a' && payload[6] == 't' && payload[7] == 'e') {

          if(LightOn) {
            webSocket.sendTXT(num,"Ligh1");
          } else {
            webSocket.sendTXT(num,"Ligh0");
          }

        }
        //RESET COMMANDS
        /*
              -- Stats
          Res -- Fact
        */
      } else if(payload[0] == 'R' && payload[1] == 'e' && payload[2] == 's') {

        if(payload[3] == 'S' && payload[4] == 't' && payload[5] == 'a' && payload[6] == 't' && payload[7] == 's') {
           webSocket.sendTXT(num,"Reset the stats");

        } else if(payload[3] == 'F' && payload[4] == 'a' && payload[5] == 'c' && payload[6] == 't' && payload[7] == 'r') {
           webSocket.sendTXT(num,"Did a factory Reset");

        }
        ///SET COMMANDS
        /*
              -- Ligh(0/1)
          Set -- Wake(0/1)
              -- To do login change
        */
      } else if(payload[0] == 'S' && payload[1] == 'e' && payload[2] == 't') {

        if (payload[3] == 'L' && payload[4] == 'i' && payload[5] == 'g' && payload[6] == 'h') {            
          if (payload[7] == '1') {                      // the browser sends an Ligh1 when the light is enabled
            LightOn = true;
          } else if (payload[7] == '0') {                      // the browser sends an Ligh0 when the light is disabled
            LightOn = false;
          }
        }
      }
       
    break;
  }
}

/*__________________________________________________________SERVER_HANDLERS__________________________________________________________*/

bool handleFileRead(String path) { // send the right file to the client (if it exists)

  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void handleResetWebSocket() {

  webSocket.disconnect();
  handleFileRead("/removeWebSocket.html");
}

void handleLogin() {

  String msg; //This is our buffer that we will add to the login html page when headers are wrong or device is locked
  if (server.hasHeader("Cookie")){   
    String cookie = server.header("Cookie"); //Copy the Cookie header to this buffer
  }

  if (server.hasArg("user") && server.hasArg("pass")){ //If user posted with these arguments
    if (server.arg("user") == username &&  server.arg("pass") == loginPassword && !lock){ //Check if login details are good and dont allow it if device is in lockdown
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: c=" + sessioncookie + "\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n"; //If above values are good, send 'Cookie' header with variable c, with format 'c=sessioncookie'
      server.sendContent(header);
      trycount = 0; //With good headers in mind, reset the trycount buffer
      return;
    }
  
    msg = "<center><br>";
    if (trycount != 10 && !lock)trycount++; //If system is not locked up the trycount buffer
    if (trycount < 10 && !lock){ //We go here if systems isn't locked out, we give user 10 times to make a mistake after we lock down the system, thus making brute force attack almost imposible
      msg += "Wrong username/password<p></p>";
      msg += "You have ";
      msg += (10 - trycount);
      msg += " tries before system temporarily locks out.";
      logincld = millis(); //Reset the logincld timer, since we still have available tries
    }
    
    if (trycount == 10){ //If too many bad tries
      if(lock){
        msg += "Too many invalid login requests, you can use this device in ";
        msg += 5 - ((millis() - logincld) / 60000); //Display lock time remaining in minutes
        msg += " minutes.";
      }
      else{
        logincld = millis();
        lock = true;  
        msg += "Too many invalid login requests, you can use this device in 5 minutes."; //This happens when your device first locks down
      }
      
    }
  }
  handleFileRead("/login.html");
}

void handleNotFound() {

if (!is_authentified()){ //This here checks if your cookie is valid in header and it redirects you to /login if not, if ok send html file
    String header = "HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    return;
  } else if(!handleFileRead(server.uri())){ 
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET)?"GET":"POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i=0; i<server.args(); i++){
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
  }
}


/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/

void startMDNS() {  // Start the mDNS responder
   if (MDNS.begin(mdnsName)) {
    Serial.println("MDNS responder started");
  }
}

void startOTA() { // Start the OTA service
  
  //OTA handling
  ArduinoOTA.onStart([]() {
    //Commit data to a safe location in the memory so that the ota doesn't overwrite it
    EEPROMr.rotate(false);
    EEPROMr.commit();

    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    
    EEPROMr.rotate(true);
  });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

void startServer() { // Start a HTTP server with a file read handler and an upload handler
 if (settingMode) {
    Serial.print("Starting Web Server at ");
    Serial.println(WiFi.softAPIP());
    server.on("/settings", []() {
      String s = "<h1>Wi-Fi Settings</h1><p>Please enter your password by selecting the SSID.</p>";
      s += "<form method=\"get\" action=\"setap\"><label>SSID: </label><select name=\"ssid\">";
      s += ssidList;
      s += "</select><br>Password: <input name=\"pass\" length=64 type=\"password\"><input type=\"submit\"></form>";
      server.send(200, "text/html", makePage("Wi-Fi Settings", s));
    });
    server.on("/setap", []() {
      for (int i = 0; i < 96; ++i) {
        EEPROMr.write(i + DATA_OFFSET, 0);
      }
      String ssid = urlDecode(server.arg("ssid"));
      Serial.print("SSID: ");
      Serial.println(ssid);
      String pass = urlDecode(server.arg("pass"));
      Serial.print("Password: ");
      Serial.println(pass);
      Serial.println("Writing SSID to EEPROMr...");
      for (int i = 0; i < ssid.length(); ++i) {
        EEPROMr.write(i + DATA_OFFSET, ssid[i]);
      }
      Serial.println("Writing Password to EEPROMr...");
      for (int i = 0; i < pass.length(); ++i) {
        EEPROMr.write(32 + i + DATA_OFFSET, pass[i]);
      }
      EEPROMr.commit();
      Serial.println("Write EEPROMr done!");
      String s = "<h1>Setup complete.</h1><p>device will be connected to \"";
      s += ssid;
      s += "\" after the restart.";
      server.send(200, "text/html", makePage("Wi-Fi Settings", s));
      ESP.restart();
    });
    server.onNotFound([]() {
      String s = "<h1>AP mode</h1><p><a href=\"/settings\">Wi-Fi Settings</a></p>";
      server.send(200, "text/html", makePage("AP mode", s));
    });
  }
  else {
  //Handling page requests
  server.on("/login", handleLogin);
  server.on("/removeWebSocket",handleResetWebSocket);
  server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound'

                            
  server.on("/reset", []() {
      for (int i = 0; i < 96; ++i) {
        EEPROMr.write(i + DATA_OFFSET, 0);
      }
      EEPROMr.commit();
      String s = "<h1>Wi-Fi settings was reset.</h1><p>Reseting device.</p>";
      server.send(200, "text/html", makePage("Reset Wi-Fi Settings", s));
      ESP.restart();
    });
  }
  Serial.println("HTTP server started.");
  server.begin();  // start the HTTP server
}

void startWebSocket() { // Start a WebSocket server
 
  webSocket.begin();                          // start the websocket server
  webSocket.onEvent(webSocketEvent);          // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
  
}

void startSPIFFS() { // Start the SPIFFS and list all contents
  SPIFFS.begin();                             // Start the SPI Flash File System (SPIFFS)
  Serial.println("SPIFFS started. Contents:");
}

/*__________________________________________________________SETUP__________________________________________________________*/

void setup(void) {

  Serial.begin(9600);
  //AutoConnectConfig(apSSID, wifiPassword);

  pinMode(relayPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  // ArduinoOTA.setPassword(passwordOTA);
  EEPROMr.size(3);
  EEPROMr.begin(512);
  delay(10);
  
//  if (Portal.begin()) {
//    Serial.println("WiFi connected: " + WiFi.localIP().toString());
//  }
  WiFi.begin(ssid, password);             // Connect to the network
    int i = 0;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i); Serial.print(' ');
  }

  if (true) {
    if (true) {
      settingMode = false;
        //Start the mDNS responder
        startMDNS();
        //Start the OTA service
        startOTA();
        //Start the SPIFFS and list all contents
        startSPIFFS();
        //Start a websocket server
        startWebSocket();
        //Start a http server
        startServer();
        //generate new cookie on device start
        gencookie(); 

        //WiFi.mode(WIFI_STA);//Turn off the soft access point

        const char * headerkeys[] = {"User-Agent","Cookie"} ;
        size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
        server.collectHeaders(headerkeys, headerkeyssize ); //These 3 lines tell esp to collect User-Agent and Cookie in http header when request is made
      return;
    }
  } 
  settingMode = false;
  //setupMode();
}

/*__________________________________________________________LOOP__________________________________________________________*/

unsigned long last = 0;

void loop(void) {
 
 if (settingMode) {
    dnsServer.processNextRequest();
  } else {
    // Portal.handleClient();
    webSocket.loop();
    ArduinoOTA.handle();
    pinHandler();
    debounceBtn();
  }
     pinHandler();
    debounceBtn();
  server.handleClient();

  if(millis() - last >= 200) {
    Serial.println(digitalRead(buttonPin));
    last = millis();
  }

  if(lock && abs(millis() - logincld) > 300000){
    lock = false;
    trycount = 0;
    logincld = millis(); //After 5 minutes is passed unlock the system
  }
    
  if(!lock && abs(millis() - logincld) > 60000){
    trycount = 0;
    logincld = millis();
    //After one minute is passed without bad entries, reset trycount
  }

}
