
#ifdef ESP32
#pragma message(THIS EXAMPLE IS FOR ESP8266 ONLY!)
#error Select ESP8266 board.
#endif

#include "dk9mbs_tools.h"
#include <ESP8266WiFi.h>
#include <FS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"

#define ROT_AZI_STEP 12
#define ROT_AZI_DIR 13
#define ROT_AZI_ENABLED 14
#define ROT_AZI_LIMIT_SWITCH 2
#define SW1PIN 4
#define SW2PIN 5
#define LIMIT_SWITCH_OFFSET 100

enum {START, MOVING, INIT,INITOFFSET,INITZERO, HOME,HOMEOFFSET};
enum {FORWARD, BACKWARD, STOPPED};

class Rotor {
  private:
    int _state;
    int _newPos;
    int _currentPos;
    int _maxSteps;
    boolean _initRequest;
    boolean _homeRequest;
       
  public:
    Rotor();

    boolean getInitRequest();
    void setInitRequest(boolean value);
    boolean getHomeRequest();
    void setHomeRequest(boolean value);
    
    int getState();
    void setState(int value);
  
    int getNewPos();
    void setNewPos(int value);

    int getCurrentPos();
    void setCurrentPos(int value);
    void incrementCurrentPos();
    void decrementCurrentPos();

    void incrementMaxSteps();
    int getMaxSteps();
    void setMaxSteps(int value);

    int getCurrentPosDeg();
    void setNewPosDeg(int deg);

};
Rotor::Rotor() {
  Rotor::_state=START;
  Rotor::_newPos=0;
  Rotor::_currentPos=0;
  Rotor::_maxSteps=0;
  Rotor::_initRequest=false;
}
int Rotor::getNewPos() {
  return Rotor::_newPos;
}
void Rotor::setNewPos(int value) {
  Rotor::_newPos=value;
}
int Rotor::getCurrentPos(){
  return Rotor::_currentPos;
}
void Rotor::setCurrentPos(int value){
  Rotor::_currentPos=value;
}
void Rotor::incrementCurrentPos(){
  Rotor::_currentPos++;
}
void Rotor::decrementCurrentPos(){
  Rotor::_currentPos--;
}
void Rotor::incrementMaxSteps(){
  Rotor::_maxSteps++;
}
int Rotor::getMaxSteps() {
  return Rotor::_maxSteps;
}
void Rotor::setMaxSteps(int value){
  Rotor::_maxSteps=value;
}
void Rotor::setInitRequest(boolean init) {
  Rotor::_initRequest=init;
}
boolean Rotor::getInitRequest() {
  return Rotor::_initRequest;
}

void Rotor::setHomeRequest(boolean init) {
  Rotor::_homeRequest=init;
}
boolean Rotor::getHomeRequest() {
  return Rotor::_homeRequest;
}


int Rotor::getState() {
  return Rotor::_state;
}
void Rotor::setState(int value){
  Rotor::_state=value;
}

int Rotor::getCurrentPosDeg() {
  float deg;
  deg=((float)360/(float)Rotor::_maxSteps)*(float)Rotor::_currentPos;
  return (int)deg;
}

void Rotor::setNewPosDeg(int deg) {
  int steps;
  steps=((float)Rotor::_maxSteps/(float)360)*(float)deg;
  Rotor::_newPos=(int)steps;
}

WiFiClient espClient;
ESP8266WebServer httpServer(80);
HTTPClient http;
LiquidCrystal_I2C lcd(0x27, 16, 2);
Rotor rotor;
boolean runSetup=false;


void setup()
{
  Serial.begin(115200);
  lcd.begin();

  disableStepper(rotor);
  printLcd(lcd, 0,1, "init",1);
  
  setupIo();
  printLcd(lcd, 5,1, ".",0);
  setupFileSystem();
  printLcd(lcd, 6,1, ".",0);

  Serial.print("Setup:");
  Serial.println(digitalRead(SW1PIN));
  Serial.print("adminpwd: ");
  Serial.println(readConfigValue("adminpwd"));
  Serial.print("Steps for 360 degree: ");
  Serial.println(readConfigValue("rotorsteps"));
  printLcd(lcd, 7,1, ".",0);
  delay (1000);  

  if(runSetup) {
    setupWifiAP();
    setupHttpAdmin();
  } else {
    setupHttpAdmin();
    if (!setupWifiSTA(readConfigValue("ssid").c_str(), readConfigValue("password").c_str(), readConfigValue("mac").c_str())) {
      // Fallback start the AP mode!
      printLcd(lcd, 0,0, "No WLAN! AP mode",0);
      setupWifiAP();
      delay(2500);
    }

    Serial.println("Waiting for commands over http...");
    delay(1000);

    rotor.setMaxSteps(readMaxStepsFromSetup());
    if(rotor.getMaxSteps()==0){
      Serial.println("No data for stepper found!");
      rotor.setInitRequest(true);
    } else {
      Serial.println("Move stepper to home position");
      rotor.setHomeRequest(true);
    }
  } 
}

void loop()
{
  static unsigned long lastSw1=0;
  static unsigned long lastSw2=0;

  httpServer.handleClient(); 
  rotorStepperStateMaschine(rotor);

  if(digitalRead(SW1PIN)==HIGH && millis() > lastSw1+500) {
    //Serial.println("SW1");
    lastSw1=millis();
  }

  if(digitalRead(SW2PIN)==HIGH && millis() > lastSw2+500) {
    //Serial.println("SW2");
    lastSw2=millis();
  }

}

void rotorStepperStateMaschine(Rotor &rotor) {
  int state;
  static int lastEntryState=-1;
  static unsigned long lastLoop=0;
  static unsigned long lastLcd=0;
  
  state=rotor.getState();
  
  int newPos=rotor.getNewPos();
  int currentPos=rotor.getCurrentPos();
  int rotorDir;

  rotorDir=calculaterotorDirection(rotor);

  switch (state) {
    case HOME:
      if (state!=lastEntryState){
        rotor.setHomeRequest(false);
        enableStepper(rotor);
        lastEntryState=state;
        Serial.println("Entry point HOME");
        clearLcdLine(lcd,1);
        printLcd(lcd, 0,1, "go home",0);
      }else if(isLimitSwitchPressed(rotor)==true) {
        rotor.setCurrentPos(-LIMIT_SWITCH_OFFSET);
        rotor.setNewPos(0);
        disableStepper(rotor);
        state=MOVING;
      } else {
        if (millis() > lastLoop + 1) {
          doStep(BACKWARD, rotor);
          lastLoop=millis();    
        }
      }      
      break;
    case START:
      if (state!=lastEntryState){
          lastEntryState=state;

          if(rotor.getHomeRequest()) state=HOME;
          if(rotor.getInitRequest()) state=INIT;
          Serial.println("Entry point START");
          clearLcdLine(lcd,1);
          printLcd(lcd, 0,1, "Pos:"+String(rotor.getCurrentPosDeg())+"\337",0); //col, row

      } else if(newPos!=currentPos){
          Serial.println(newPos);
          Serial.println("Statemaschine is starting...");
          enableStepper(rotor);
          state=MOVING;
      } else {
        // no do actions in this state!!!
      }
      break;
    case INIT:
      if (state!=lastEntryState){
        rotor.setInitRequest(false);
        enableStepper(rotor);
        lastEntryState=state;
        Serial.println("Entry point INIT");
        clearLcdLine(lcd,1);
        printLcd(lcd, 0,1, "move to max",0);
      }

      if(isLimitSwitchPressed(rotor)==true) {
        rotor.setNewPos(rotor.getCurrentPos()-LIMIT_SWITCH_OFFSET);
        state=INITOFFSET;
      } else {
        if (millis() > lastLoop + 1) {
          doStep(FORWARD, rotor);
          lastLoop=millis();    
        }
      }
      break;

    case INITOFFSET:
      if (state!=lastEntryState){
          lastEntryState=state;
          Serial.println("Entry point INITOFFSET");
          clearLcdLine(lcd,1);
          printLcd(lcd, 0,1, "move to offset",0);
      }

      if(newPos==currentPos){
        disableStepper(rotor);
        state=INITZERO;
      } else {
        if (millis() > lastLoop + 1) {
          doStep(BACKWARD, rotor);
          lastLoop=millis();
        }
      }
      break;    
    case INITZERO:
      if (state!=lastEntryState){
          enableStepper(rotor);
          rotor.setMaxSteps(0);
          lastEntryState=state;
          Serial.println("Entry point INITZERO");
          clearLcdLine(lcd,1);
          printLcd(lcd, 0,1, "move home",0);
      }
      
      if(isLimitSwitchPressed(rotor)==true) {
        rotor.setCurrentPos(LIMIT_SWITCH_OFFSET*-1);
        rotor.setNewPos(0);
        Serial.println("Steps required for 360 degree:"+(String)rotor.getMaxSteps());
        saveConfigValue("rotorsteps", (String)(rotor.getMaxSteps()-LIMIT_SWITCH_OFFSET));
        state=MOVING;
      } else {
        if (millis() > lastLoop + 1) {
          if(doStep(BACKWARD, rotor)==true){
            rotor.incrementMaxSteps();
          }
          lastLoop=millis();    
        }
      }
      break;

    case MOVING:
      if (state!=lastEntryState){
        enableStepper(rotor);          
        lastEntryState=state;
        Serial.println("Entry point MOVING");
        clearLcdLine(lcd,1);
        printLcd(lcd, 0,1, "moving...",0); 
      }
    
      if(newPos==currentPos){
        disableStepper(rotor);
        state=START;
      } else {
        if (millis() > lastLoop + 10) {
          doStep(rotorDir, rotor);
          lastLoop=millis();
        }

      }
      
      break;
  }

  rotor.setState(state);
}

/*
 * 
 * Helper functions
 * 
 */

void enableStepper(Rotor &rotor) {
  digitalWrite(ROT_AZI_ENABLED, LOW);
}

void disableStepper(Rotor &rotor) {
  digitalWrite(ROT_AZI_ENABLED, HIGH);
}

boolean isLimitSwitchPressed(Rotor &rotor) {
  if(digitalRead(ROT_AZI_LIMIT_SWITCH)==LOW) {
    return true;
  } else {
    return false;
  }
}

boolean doStep(int rotorDir, Rotor &rotor) {
  boolean dirPin=LOW;
  boolean result=false;
  
  if (rotorDir==BACKWARD) {
    dirPin=HIGH;
    //dirPin=LOW;
  }
  
  digitalWrite(ROT_AZI_DIR, dirPin);

  if (digitalRead(ROT_AZI_STEP)){
      digitalWrite(ROT_AZI_STEP,LOW);
      //increment or decrement the current pos only at every low high pass
      result=true;
      if (rotorDir==BACKWARD) {
        rotor.decrementCurrentPos();
      } else {
        rotor.incrementCurrentPos();
      }
    } else {
    digitalWrite(ROT_AZI_STEP, HIGH);
  }

  return result;
}

int calculaterotorDirection(Rotor &rotor) {
  int rotorDir;
  rotorDir=STOPPED;
  
  if (rotor.getNewPos()<rotor.getCurrentPos()) {
    rotorDir=BACKWARD;
  } else {
    rotorDir=FORWARD;
  }

  return rotorDir;
}

int readMaxStepsFromSetup(){
  String tmp;
  tmp=readConfigValue("rotorsteps");
  if(tmp=="" || tmp=="0"){
    return 0;
  }

  return tmp.toInt();
}

/*
   getCommandValue
   Return a part of a complete Stepper command.
   Example:
   getCommandValue("MF 100", ' ',2)
   Return 100
*/
String getCommandValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}


String createIoTDeviceAddress(String postfix) {
  String address=String(readConfigValue("mac")+"."+postfix);
  address.replace("-","");
  return address;  
}

void setupHttpAdmin() {
  httpServer.on("/",handleHttpSetup);
  httpServer.on("/api",handleHttpApi);
  httpServer.onNotFound(handleHttp404);
  httpServer.begin();
}

void handleHttpApi() {
  String cmd=httpServer.arg("command");
  String steps=httpServer.arg("steps");
  String deg=httpServer.arg("deg");
  String line0=httpServer.arg("line0");
  String responseBody="api";
  
  if(cmd=="ping") {
    responseBody="pong";
  } else if(cmd=="MOVE") {

    if(rotor.getState()==INIT || rotor.getState()==INITOFFSET || rotor.getState()==INITZERO){
      httpServer.send(500, "text/html", "Rotor zeroinit in progress!"); 
      return;
      
    }
    //rotor.setNewPos(steps.toInt());
    rotor.setNewPosDeg(deg.toInt());
    Serial.println("New pos:"+((String)rotor.getNewPos()));
    Serial.println("Current pos:"+((String)rotor.getCurrentPos()));
  } else if (cmd=="GETCURRENTPOS") {
    responseBody=(String)rotor.getCurrentPos();
  } else if (cmd =="SETDISPLAY"){
    clearLcdLine(lcd,0);
    printLcd(lcd, 0,0, line0,0); //col, row
    responseBody=String(line0);
  }

  httpServer.send(200, "text/html", responseBody); 
}

void handleHttpSetup() {
    String pwd = readConfigValue("adminpwd");
    if (!httpServer.authenticate("admin", pwd.c_str())) {
      return httpServer.requestAuthentication();
    }
      
    if(httpServer.hasArg("CMD")) {
      Serial.println(httpServer.arg("CMD"));
      handleSubmit();
    }

    if(httpServer.hasArg("FORMATFS")) {
      Serial.println("Format FS");
      handleFormat();
    }
    if(httpServer.hasArg("RESET")) {
      Serial.println("Reset ...");
      handleReset();
    }

    String html =
    "<!DOCTYPE HTML>"
    "<html>"
    "<head>"
    "<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">"
    "<title>DK9MBS-ROTOR-OS</title>"
    "<style>"
    "\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\""
    "</style>"
    "</head>"
    "<body>"
    "<h1>DK9MBS-Rotor-OS Setup</h1>"
    "<FORM action=\"/\" method=\"post\">"
    "<P>Wlan:"
    "<INPUT type=\"hidden\" name=\"CMD\" value=\"SAVE\"><BR>"
    "<div style=\"border-style: solid; border-width:thin; border-color: #000000;padding: 2px;margin: 1px;\"><div>ssid</div><INPUT style=\"width:99%;\" type=\"text\" name=\"SSID\" value=\""+ readConfigValue("ssid") +"\"></div>"
    "<div style=\"border-style: solid; border-width:thin; border-color: #000000;padding: 2px;margin: 1px;\"><div>Password</div><INPUT style=\"width:99%;\" type=\"text\" name=\"PASSWORD\" value=\""+ readConfigValue("password") +"\"></div>"
    "<div style=\"border-style: solid; border-width:thin; border-color: #000000;padding: 2px;margin: 1px;\"><div>MAC (A4-CF-12-DF-69-00)</div><INPUT style=\"width:99%;\" type=\"text\" name=\"MAC\" value=\""+ readConfigValue("mac") +"\"></div>"
    "</P>"
    "<P>Network:"
    "<div style=\"border-style: solid; border-width:thin; border-color: #000000;padding: 2px;margin: 1px;\"><div>Hostname</div><INPUT style=\"width:99%;\" type=\"text\" name=\"HOSTNAME\" value=\""+ readConfigValue("hostname") +"\"></div>"
    "</P>"
    "<P>Admin portal"
    "<div style=\"border-style: solid; border-width:thin; border-color: #000000;padding: 2px;margin: 1px;\"><div>Admin Password</div><INPUT style=\"width:99%;\" type=\"text\" name=\"ADMINPWD\" value=\""+ readConfigValue("adminpwd") +"\"></div>"
    "</P>"
    "<P>Rotor:"
    "<div style=\"border-style: solid; border-width:thin; border-color: #000000;padding: 2px;margin: 1px;\"><div>Steps for 360 degree</div><INPUT style=\"width:99%;\" type=\"text\" name=\"ROTORSTEPS\" value=\""+ readConfigValue("rotorsteps") +"\"></div>"
    "</P>"
    "<div>"
    "<INPUT type=\"submit\" value=\"Save\">"
    "<INPUT type=\"submit\" name=\"RESET\" value=\"Save and Reset\">"
    "<INPUT type=\"submit\" name=\"FORMATFS\" value=\"!!! Format fs !!!\">"
    "</div>"
    "</FORM>"
    "</body>"
    "</html>";
    httpServer.send(200, "text/html", html); 
}

void handleSubmit() {
  saveConfigValue("mac", httpServer.arg("MAC"));
  saveConfigValue("ssid", httpServer.arg("SSID"));
  saveConfigValue("password", httpServer.arg("PASSWORD"));
  saveConfigValue("adminpwd", httpServer.arg("ADMINPWD"));
  saveConfigValue("hostname", httpServer.arg("HOSTNAME"));
  saveConfigValue("rotorsteps", httpServer.arg("ROTORSTEPS"));
}

void handleReset() {
  httpServer.send(200, "text/plain", "restart ..."); 
  ESP.restart();
}

void handleFormat() {
  Serial.print("Format fs ... ");
  SPIFFS.format();
  setupFileSystem();
  Serial.println("ready");
}

void handleHttp404() {
  httpServer.send(404, "text/plain", "404: Not found"); 
}

void setupIo() {
  ESP.eraseConfig();
  WiFi.setAutoConnect(false);

  pinMode(ROT_AZI_STEP, OUTPUT);
  pinMode(ROT_AZI_DIR, OUTPUT);
  pinMode(ROT_AZI_ENABLED, OUTPUT);
  pinMode(ROT_AZI_LIMIT_SWITCH,INPUT_PULLUP);
  pinMode(SW1PIN, INPUT);
  pinMode(SW2PIN, INPUT);
  //pinMode(DISPLAY_SDA,OUTPUT);
  //pinMode(DISPLAY_SCL,OUTPUT);
  
}

void setupFileSystem() {
  if(!SPIFFS.begin()) {
    SPIFFS.format();
    SPIFFS.begin(); 
  }

  
  if(!SPIFFS.exists(getConfigFilename("mac"))) {
    String mac=WiFi.macAddress();
    mac.replace(":","-");
    saveConfigValue("mac", mac);
  }
  if(!SPIFFS.exists(getConfigFilename("ssid"))) saveConfigValue("ssid", "wlan-ssid");
  if(!SPIFFS.exists(getConfigFilename("password"))) saveConfigValue("password", "wlan-password");
  if(!SPIFFS.exists(getConfigFilename("adminpwd"))) saveConfigValue("adminpwd", "123456789ff");
  if(!SPIFFS.exists(getConfigFilename("hostname"))) saveConfigValue("hostname", "node");
  if(!SPIFFS.exists(getConfigFilename("pubtopic"))) saveConfigValue("pubtopic", "temp/sensor");
  if(!SPIFFS.exists(getConfigFilename("rotorsteps"))) saveConfigValue("rotorsteps", "0");

}

void setupWifiAP(){
  Serial.println("Setup shell is starting ...");
  String pwd=readConfigValue("adminpwd");

  if(pwd==""){
    Serial.println("Use default password!");
    pwd="0000";
  }
  
  Serial.print("Password for AP:");
  Serial.println(pwd);
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP("rotor.dk9mbs.de", pwd);

  Serial.println("AP started");
  
  printLcd(lcd, 0,0, WiFi.softAPIP(),1);
  printLcd(lcd, 0,1, pwd,0);

}

boolean setupWifiSTA(const char* ssid, const char* password, const char* newMacStr) {
  int timeoutCounter=0;
  int timeout=20;
  
  uint8_t mac[6];
  byte newMac[6];
  parseBytes(newMacStr, '-', newMac, 6, 16);

  clearLcdLine(lcd,0);
  printLcd(lcd, 0,0, "Connecting WLAN",0);

  WiFi.setAutoReconnect(true);

  if(newMacStr != "") {
    wifi_set_macaddr(0, const_cast<uint8*>(newMac));
    Serial.println("mac address is set");
  }
  
  wifi_station_set_hostname(readConfigValue("hostname").c_str());
  Serial.print("Hostname ist set: ");
  Serial.println(readConfigValue("hostname"));
  
  delay(10);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  Serial.print("Password:");
  Serial.println("***********");
  
  WiFi.begin(ssid, password);
  
  Serial.println("after WiFi.begin():");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    timeoutCounter++;
    clearLcdLine(lcd,1);
    printLcd(lcd, 0,1, String(timeoutCounter)+"/"+String(timeout),0);
    if (timeoutCounter>timeout) return false;
    
  }

  WiFi.macAddress(mac);
  
  Serial.println("WiFi connected");
  Serial.print("IP address:");
  Serial.println(WiFi.localIP());
  Serial.printf("Mac address:%02x:%02x:%02x:%02x:%02x:%02x\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  Serial.printf("Mac address:%s\n", WiFi.macAddress().c_str());
  Serial.print("Subnet mask:");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway:");
  Serial.println(WiFi.gatewayIP());

  printLcd(lcd, 0,0, WiFi.localIP(),1);
  printLcd(lcd, 0,1, "AG5ZL Rotor",0);

  return true;
}

/*
 * Display
 */
void clearLcdLine(LiquidCrystal_I2C& lcdDisplay,int line){
  for(int n = 0; n < 16; n++) {  // 20 indicates symbols in line. For 2x16 LCD write - 16
    lcdDisplay.setCursor(n,line);
    lcdDisplay.print(" ");
  }
  lcdDisplay.setCursor(0,line);             // set cursor in the beginning of deleted line
}
 
void printLcd(LiquidCrystal_I2C& lcdDisplay,int column, int row, IPAddress text, int clear) {
  if(clear==1) lcdDisplay.clear();
  
  lcdDisplay.setCursor(column, row); // Spalte, Zeile
  lcdDisplay.print(text);
} 
void printLcd(LiquidCrystal_I2C& lcdDisplay,int column, int row, String text, int clear) {
  if(clear==1) {
    lcdDisplay.clear();
  }
  
  lcdDisplay.setCursor(column, row); // Spalte, Zeile
  lcdDisplay.print(text);
}

//class StepperRequest {
//  public:
//    static int _newPos;
//    static int _currentPos;
//    static int _maxSteps;
//    static boolean _init;
//    
//    static int getNewPos();
//    static void setNewPos(int value);
//
//    static int getCurrentPos();
//    static void setCurrentPos(int value);
//    static void incrementCurrentPos();
//    static void decrementCurrentPos();
//
//    static void incrementMaxSteps();
//    static int getMaxSteps();
//
//    static void setInit(boolean init);
//    static boolean getInit();
//    
//};
//
//int StepperRequest::_newPos=0;
//
//int StepperRequest::getNewPos() {
//  return StepperRequest::_newPos;
//}
//void StepperRequest::setNewPos(int value) {
//  StepperRequest::_newPos=value;
//}
//
//int StepperRequest::_currentPos=0;
//int StepperRequest::getCurrentPos() {
//   return StepperRequest::_currentPos;
//}
//void StepperRequest::setCurrentPos(int value) {
//   StepperRequest::_currentPos=value;
//}
//void StepperRequest::incrementCurrentPos() {
//   StepperRequest::_currentPos++;
//}
//void StepperRequest::decrementCurrentPos() {
//   StepperRequest::_currentPos--;
//}
//
//
//int StepperRequest::_maxSteps=0;
//void StepperRequest::incrementMaxSteps() {
//  StepperRequest::_maxSteps++;
//}
//int StepperRequest::getMaxSteps() {
//  return StepperRequest::_maxSteps;
//}
//
//boolean StepperRequest::_init=false;
//void StepperRequest::setInit(boolean value) {
//  StepperRequest::_init=value;
//}
//boolean StepperRequest::getInit(){
//  return StepperRequest::_init;
//}
