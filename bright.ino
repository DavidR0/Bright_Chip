#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <WebSocketsServer.h>
// #include <EEPROM_Rotate.h>

/*__________________________________________________________Configurable Variables__________________________________________________________*/

#define relayPin   4  //D2
#define buttonPin  5  //D1


//Configure your network details here
const char* ssid = "HUAWEI-Ex9S";
const char* password = "4Rmatp45";
//  const char* ssid = "OnePlus5";
//  const char* password = "12345678";

//Setting on what port the server to run, 81 for deployment(port 8080 for testing)
ESP8266WebServer server(81);
// create a websocket server on port 82 (port 8081 for testing)
WebSocketsServer webSocket(82);  

// // Domain name for the mDNS responder
const char* mdnsName = "NodeMCU-Light"; 
//Password for OTA
const char* passwordOTA = "1243";

/*__________________________________________________________Nonconfigurable Variables__________________________________________________________*/

//Counters for logistics
int32_t timesLightsSwitched = 0;
unsigned long lastTimeLightTurnedON = 0;
unsigned long totalLightsOnTime = 0;

unsigned long btnResetTimer;

//States for current settings
bool LightOn = true;
bool cpyLightOn = LightOn;

//This bool is used to control device lockout 
bool lock = false; 

//anchars will be explained below. username will be compared with 'user' and loginPassword with 'pass' when login is done
String anchars = "abcdefghijklmnopqrstuvwxyz0123456789", username = "admin", loginPassword = "root"; 

//First 2 timers are for lockout and last one is inactivity timer
unsigned long logincld = millis(), reqmillis = millis(), tempign = millis(); 

// i is used for for index, trycount will be our buffer for remembering how many false entries there were
uint8_t i, trycount = 0; 
//this is cookie buffer
String sessioncookie; 

//Button debouncing
struct button_S
{
  int phase = 0;
  unsigned long  timeStamp = millis();
};
button_S buttonState;

#define debounceTime 15

// EEPROM_Rotate EEPROMr;

//Copy of WebSocket connection number
uint8_t WebSocketnum = -1;

/*__________________________________________________________SETUP__________________________________________________________*/

void setup(void) {

  Serial.begin(115200);

  pinMode(relayPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  gencookie(); //generate new cookie on device start

  const char * headerkeys[] = {"User-Agent","Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  server.collectHeaders(headerkeys, headerkeyssize ); //These 3 lines tell esp to collect User-Agent and Cookie in http header when request is made
  // ArduinoOTA.setPassword(passwordOTA);

  WiFi.setAutoConnect(true);
  
  //Start the wifi and connect to configured network
  startWiFi();
  // Start the mDNS responder
  startMDNS();
  // Start the OTA service
  startOTA();
  //Start the SPIFFS and list all contents
  startSPIFFS();
  //Start a websocket server
  startWebSocket();
  //Start a http server
  startServer();
   
}

/*__________________________________________________________LOOP__________________________________________________________*/

void loop(void) {

  server.handleClient();
  ArduinoOTA.handle();
  pinHandler();
  webSocket.loop();
  debounceBtn();
 

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
    
    
  if(abs(millis() - tempign) > 120000){
    gencookie();
    tempign = millis();
    //if there is no activity from loged on user, change the generate a new cookie. This is more secure than adding expiry to the cookie header
  }  

}

/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/

void startMDNS() {  // Start the mDNS responder
   if (MDNS.begin(mdnsName)) {
    Serial.println("MDNS responder started");
  }
}

void startWiFi() {  // Start a Wi-Fi access point, and try to connect
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  } 
    
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void startOTA() { // Start the OTA service
  
  //OTA handling
  ArduinoOTA.onStart([]() {
    //Commit data to a safe location in the memory so that the ota doesn't overwrite it
    // EEPROMr.rotate(false);
    // EEPROMr.commit();

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
    
    // EEPROMr.rotate(true);
  });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

void startServer() { // Start a HTTP server with a file read handler and an upload handler
 
  //Handling page requests
  server.on("/login", handleLogin);
  server.on("/logoff",handleLogoff);
  server.on("/refresh",handleRefresh);
  server.on("/removeWebSocket",handleResetWebSocket);
  server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound'

  server.begin();                             // start the HTTP server
  Serial.println("HTTP server started.");
}

void startWebSocket() { // Start a WebSocket server
 
  webSocket.begin();                          // start the websocket server
  webSocket.onEvent(webSocketEvent);          // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
  
}

void startSPIFFS() { // Start the SPIFFS and list all contents
  SPIFFS.begin();                             // Start the SPI Flash File System (SPIFFS)
  Serial.println("SPIFFS started. Contents:");
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {                      // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }
}

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

void handleRebootREQ() {

  ESP.restart();
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

void handleRefresh() {

  if(is_authentified()){ //This is for reseting the inactivity timer, it covers everything explained above
    tempign = millis();
    String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header); 
  }
  else{
    String header = "HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
  }
}

/*__________________________________________________________WEBSOCKET_FUNCTIONALITY__________________________________________________________*/

int connectedClients(){
  return webSocket.connectedClients(false);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", num);
      WebSocketnum = -1;///Reset the number so that we can't sent messages with no connection
      break;

    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        WebSocketnum = num;///Set the number so that we can send messages

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

          sendStats();

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
              -- Chipp 
        */
      } else if(payload[0] == 'R' && payload[1] == 'e' && payload[2] == 's') {

        if(payload[3] == 'S' && payload[4] == 't' && payload[5] == 'a' && payload[6] == 't' && payload[7] == 's') {
           webSocket.sendTXT(num,"Reset the stats");

        } else if(payload[3] == 'F' && payload[4] == 'a' && payload[5] == 'c' && payload[6] == 't' && payload[7] == 'r') {
           webSocket.sendTXT(num,"Did a factory Reset");

        } else if(payload[3] == 'C' && payload[4] == 'h' && payload[5] == 'i' && payload[6] == 'p' && payload[7] == 'p') {
          webSocket.sendTXT(num,"RebootOK");
          delay(200);
          ESP.restart();
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

/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/

String formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
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

void sendStats() {

  if(LightOn) { 
    totalLightsOnTime += millis() - lastTimeLightTurnedON;
    lastTimeLightTurnedON = millis();//We need to reset the value so we don't overlap with the time that was added in the totalLightsOnTime
  }

  String msgStatsChange = "Stats";
  String minutesSinceReboot = (String)(millis() / 60000);
  String LightsOnTime = (String)(totalLightsOnTime / 60000);

  //Send a websocket message with the new stats (because they changed)
  webSocket.sendTXT(WebSocketnum, msgStatsChange + LightsOnTime + "/" + timesLightsSwitched + "/" + minutesSinceReboot);
}

void pinHandler() {
  //If button is pressed
  if(buttonState.phase == 2) {
    LightOn = !LightOn;
    btnResetTimer = millis();    
  }

  //Once btn is pressed check if it was held for at least 5 sec to reset the chip
  // if(buttonState.phase == 3)
  // {
  //   if(millis() - btnResetTimer >= 5000) {
  //     digitalWrite(relayPin,HIGH);
  //     ESP.reset();
  //   }
    
  // }

  if(LightOn != cpyLightOn) {
    cpyLightOn = LightOn;
    ++timesLightsSwitched;

    String msgLightChange = "Ligh";

    //Send websocket message that the light state changed
    if(WebSocketnum != -1){
      if(LightOn) { 
        msgLightChange += "1";
      } else {
        msgLightChange += "0";
      }
      webSocket.sendTXT(WebSocketnum,msgLightChange);
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

    sendStats();
  }
}

void debounceBtn() {

  switch (buttonState.phase) {

  case 0:
    if (digitalRead(buttonPin))
    {
      buttonState.phase = 1;
    }
    break;

  case 1:
    if (!digitalRead(buttonPin))
    {
      buttonState.timeStamp = millis();
      buttonState.phase = 0;
    }
    else if ((millis()- buttonState.timeStamp) >= debounceTime)
    {
      buttonState.phase = 2;
    }
    break;

  case 2:
    buttonState.phase = 3;
    break;

  case 3:
    if (!digitalRead(buttonPin))
    {
      buttonState.phase = 4;
    }
    break;

  case 4:
    if (digitalRead(buttonPin))
    {
      buttonState.phase = 3;
      buttonState.timeStamp = millis();
    }

    else if (((millis() - buttonState.timeStamp) >= debounceTime) && !digitalRead(buttonPin))
    {
      buttonState.phase = 5;
    }
    break;

  case 5:
    buttonState.phase = 0;
    break;
  }
}