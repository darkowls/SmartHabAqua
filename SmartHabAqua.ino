// HabAqua module:
// LED Lightning via relays
// Measure water temperature
// Measure oustide tenmerature and huimidity
// Heater via relay
// Fan via relay
// RealTime Clock
// 128x64 display

// LEDs are divided in 8 groups of different number of leds and lumens
// Line | Leds | Group
//  1   |  36  | 8
//  2   |  36  | 5
//  3   |  36  | 6
//  4   |  36  | 1
//  5   |  24  | 4
//  6   |  24  | 3
//  7   |  24  | 7
//  8   |  24  | 2
//  9   |  24  | 4
// 10   |  36  | 7
// 11   |  27  | 8
// 12   |  24  | 3
// 13   |  21  | 2
// 14   |  18  | 6
// 15   |  15  | 5
//
// Group | Leds | Wire Tag
// 1     | 36   | 3
// 2     | 45   | 1
// 3     | 48   | 4
// 4     | 48   | 8
// 5     | 51   | 6
// 6     | 54   | 2
// 7     | 60   | 5
// 8     | 63   | 7
//Total  | 405  |


// INCLUDING LIBS
// Including i2C Lib
#include <Wire.h>
// Including display libs
#include "ssd1306.h"
#include "font6x8.h" 
// Including DHT Lib 
#include "DHT.h"
// Inluding OneWire lib for DS18B20
#include <OneWire.h>
// Including Dallas Temperaire lib
#include <DallasTemperature.h>
//Including RTC libs
#include <DS3231.h>
DS3231  rtc(SDA, SCL);

// Defining pins
#define ONE_WIRE_BUS 12
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
#define DHTPIN 13                                                               // DHT-22 pin
byte LEDPinsQty=8;                                                              // Number of pins, used by Leds
byte LEDPin[8]={2,3,4,5,6,7,8,9};                                               // Pins used by Led Relays
bool LEDRelayState[8]={false,false,false,false,false,false,false,false};        // Condition of LED relay pins
byte HeatPin=10;                                                                // Pin of Heater relay
byte FanPin=11;                                                                 // Pin of Fan relay

// INITIALIZING
#define DHTTYPE DHT22
// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);                                                       // Init of DHT sensor

// Settind Global Variables for physical parameters
byte OutHum = 0;                                                                // Humidity in room
byte OutTemp = 0;                                                               // Temperature in room (not accurate, partially getting temperature inside conroller
//float WatTemp = 0;                                                              // Temperature of water
union WatUnion{                                                                 // Union for water temperature
  float WatTempFl;                                                              // Float as a whole
  uint8_t WatTemp8[4];                                                          // Array of bytes for sending
}WatTemp={WatTempFl:20};                                                        // Initially setting water temperature of 20 
// Setting internal variables
byte NumLeds[]={36,45,48,48,51,54,60,63};                                       // Number of LEDs in each group
int TotalLEDs="";                                                               // Total LEDS currently on
byte LedLm = 24;                                                                // Arroximately 24 lumens per LED
String ShortDOW ="";                                                            // 2-Letter Day of Week
byte TimeOfDay=0;                                                               // 0=Dusk(Morning) , 1=Day, 2=Dawn (Evening), 3=Night
byte CurLightLevel=0;                                                           // Currently reached LED light level
bool FanOn=false;                                                               // Global for turning on-off fan
bool HeatOn=false;                                                              // Global for turning on-off heater
bool ReachCenter=false;                                                         // Flag saying to reach center of temp diapasone
byte CorBytesExpected=14;                                                       // Number of bytes expected in transmission

//Setting Aquarium Variables (will be changebale via WiFi MQTT
byte MinTemp=23;                                                                //Minimum desirable temperature
byte MaxTemp=25;                                                                //Maximum desirable temperature
byte DuskHour=14;                                                               // Time for turning on LEDs, after that every group what is enabled is turned on step by step
byte DawnHour=23;                                                               // Time for turning off LEDs,
byte SunChangeLenght=30;                                                        // Length of Dawn or Dusk in minutes, from 10 (fast change) to 59 (1 hour light change)
byte LightLevel=8;                                                              // Light level from 0 (0 LEDs) to 8 (405 Leds) - main parameter for lightning

//Setting timing parameters
uint8_t SimpleTimer;                                                            // Simple timer (0-10 secs) to load-balance internal procidures on time line
uint8_t OldSimpleTimer;                                                         // To track Second Changed Event 
bool SecondChanged=false;                                                       // Flag for second changed event

// VOID SETUP_________________________________________________________________________________________________________________________________________________________________
  void setup() {
  // I2C communications setup
  Wire.begin(8);                                                                // Join i2c bus with address 8
  Wire.onReceive(receiveEvent);                                                 // Register On recieve Event Function
  Wire.onRequest(requestEvent);                                                 // Register On Request Event Function
  // Screen Setup
  ssd1306_128x64_i2c_init();                                                    // Initializing screen
  ssd1306_fillScreen(0x00);                                                     // Clearing screen
  ssd1306_setFixedFont(ssd1306xled_font6x8);                                    // Setting fonts

  // Pin setup
  byte i;
  for (i=0; i<=LEDPinsQty-1; i++){                                              // For all LED relay pins
    pinMode(LEDPin[i],OUTPUT);                                                  // Setting PinMode for LEDs
    digitalWrite(LEDPin[i],HIGH);                                               // Setting HIGH to turn relays OFF and for NO relays LEDs ON
    LEDRelayState[i]= true;                                                     // Altering state of relays
    CurLightLevel=i+1;            // Initial Current Light Level ajustment      // Altering lightlevel to maximum
  }
  // Also setting pins for Fan and Heater
  pinMode(HeatPin,OUTPUT);                                                      // Setting PinMode for Heater relay
  pinMode(FanPin,OUTPUT);                                                       // Setting PinMode for Fan relay
  digitalWrite(HeatPin,HIGH);                                                   // Setting Heater relay HIGH to turn relay OFF and for NO relays Heater On
  FanOn=true;                                                                   // Altering Fan state
  digitalWrite(FanPin,HIGH);                                                    // Setting Fan relay HIGH to turn relay OFF and for NO relays Fan On
  HeatOn=true;                                                                  // Altering Heater state
  dht.begin();                                                                  // Starting DHT
  sensors.begin();                                                              // Starting D18B20
  rtc.begin();                                                                  // Starting RTC

  // UNCOMMENT AND CHANGE ONCE BEFORE COMPILE TO SET TIME!!!
  // The following lines can be uncommented to set the date and time
  //rtc.setDOW(FRIDAY);    // Set Day-of-Week to 
  //rtc.setTime(0,57,0);     // Set the time to 
  //rtc.setDate(23,3,2018);   // Set the date to 

  // LATER - change RTC chip to DS1307 and read last parameters from battery-backed RAM. For now - on reset - hardcoded values are used until reftesh from MQTT/ESP8266 arrives.

  CalcTotalLeds();                                                              // Calculating currently enabled LEDs

// Printing initial parameters
  char line[24];                                                                // Var for line buffer
  sprintf(line,"SmartHab Aqua v1.0");
  ssd1306_printFixed(0,0,"SmartHab Aqua v1.0", STYLE_NORMAL);                   // Printing title
  sprintf(line,"Initial Aqua Params:");
  ssd1306_printFixed(0,8,line,STYLE_NORMAL);                                   // Printing text
  sprintf(line,"Min. Temperature:%1d",MinTemp);
  ssd1306_printFixed(0,16,line,STYLE_NORMAL);                                   // Printing min temp
  sprintf(line,"Max. Temperature:%1d",MaxTemp);
  ssd1306_printFixed(0,24,line,STYLE_NORMAL);                                   // Printing max temp
  sprintf(line,"Day: %02d to %02d hours",DuskHour,DawnHour);
  ssd1306_printFixed(0,32,line,STYLE_NORMAL);                                   // Printing day time
  sprintf(line,"Sunrise/Sunset %02d min",SunChangeLenght);                  
  ssd1306_printFixed(0,40,line,STYLE_NORMAL);                                   // Printing Sunset/Sunrise Duration
  sprintf(line,"Light lvl %1d,%3d LEDs",LightLevel,TotalLEDs);                  
  ssd1306_printFixed(0,48,line,STYLE_NORMAL);                                   // Printing number of LEDs
  sprintf(line,"LED Output %04d lumen",TotalLEDs*LedLm);
  ssd1306_printFixed (0,56,line,STYLE_NORMAL);                                  // Printing lumens
  delay(9000);
  ssd1306_fillScreen(0x00);                                                     // Clearing screen
}

// VOID LOOP__________________________________________________________________________________________________________________________________________________________
void loop() {
  // Calculating simple timer and timing events
  SimpleTimer=(millis()/1000)%10;                                               // Calculating seconds 0-9 from milliseconds
  if (OldSimpleTimer!=SimpleTimer){                                             // Detecting changing every second for ticks and flags
    SecondChanged=true;
    OldSimpleTimer=SimpleTimer;
  }else{
    SecondChanged=false;
  }
  // Placing events on a timeline
  //|  0   |   1   |   2   |   3   |   4   |   5   |   6   |   7   |   8   |   9   |
  //|Humidity      |Air Temp.      |Water Temp.    |Changing Light |Alter heat/fan |    
  float InputVal;                                                               // Defining float for input
  if(SimpleTimer==0 and SecondChanged==true){                                   // Once every 0 second of 10 = get humidity
     InputVal=dht.readHumidity();                                               // Reading temperature or humidity takes about 250 milliseconds!
   OutHum=int(InputVal);                                                        // Getting humidity var
  }
  if(SimpleTimer==2 and SecondChanged==true){                                   // Once every 2 second of 10 = get air temperature
    InputVal = dht.readTemperature();                                           // Read temperature as Celsius (the default)
    OutTemp = InputVal;                                                         // Getting air temp var
  }
  if(SimpleTimer==4 and SecondChanged==true){                                   // Once every 4 second of 10 = get water temperature
    sensors.requestTemperatures();                                              // Reading temperature from D18B20
    InputVal=(sensors.getTempCByIndex(0));
    if((InputVal>1) and (InputVal<60)){                                         // Water temp cannot get THAT cold or warm)
      WatTemp.WatTempFl=InputVal;                                               // Trying round to prevent relay flicker on corrupt data
    }
  }
  if(SimpleTimer==6 and SecondChanged==true){                                   // Once every 6 second of 10 calling LEDs update routine
    ChangeLEDs();                                                               
  }
  if(SimpleTimer==8 and SecondChanged==true){                                   // Once every 8 second of 10 calling temperature changeroutine
    ChangeTemp();                                                                  
  }
  if(SecondChanged=true){                                                       // Once every second calling OLED update
    RefreshOLED();
  }
}

// ROUTINE TO CHANGE OLED SCREEN TEXT________________________________________________________________________________________________________________
void RefreshOLED(){
  // Refreshing display text
  char line[24];                                                                // Var for text output
  (ShortDOW+String(rtc.getDateStr())+" "+String(rtc.getTimeStr())).toCharArray(line, 24);
  ssd1306_printFixed (0,0,line,STYLE_NORMAL);
  sprintf(line,"Air:Hum=%d%% T=%d'C",OutHum,OutTemp);
  ssd1306_printFixed (0, 8,line,STYLE_NORMAL);
  sprintf(line,"T=%02d,%02d'C;Tnorm=%2d-%2d",int(WatTemp.WatTempFl),(int(WatTemp.WatTempFl*100))%100,MinTemp,MaxTemp);
  ssd1306_printFixed (0,16,line,STYLE_NORMAL);
  sprintf(line,"Fan:%1d;Heater:%1d",FanOn,HeatOn);
  ssd1306_printFixed (0,24,line,STYLE_NORMAL);
  int TotalLumens=TotalLEDs*LedLm;
  switch(TimeOfDay){        //Showing string dependant on time of day
    case 0:
    sprintf(line,"Dawning till %02d:%02d",DuskHour,SunChangeLenght);
    ssd1306_printFixed (0,32,line,STYLE_NORMAL);
    break;
    case 1:
    sprintf(line,"Day,%1d/8 Lamps,%4d lm",LightLevel,TotalLumens);
    ssd1306_printFixed (0, 32,line,STYLE_NORMAL);
    break;
    case 2:
    sprintf(line,"Sunset till %02d:%02d",DawnHour,SunChangeLenght);
    ssd1306_printFixed (0, 32,line,STYLE_NORMAL);
    break;
    case 3:
    sprintf(line,"Night,%1d lamps on",CurLightLevel);
    ssd1306_printFixed (0, 32, line, STYLE_NORMAL);
    break;
    default:
    ssd1306_printFixed (0, 32, "TimeOfDay="+TimeOfDay, STYLE_NORMAL);
    break;
  }
  sprintf(line,"Day:%02d:00 to %02d:00",DuskHour,DawnHour);
  ssd1306_printFixed (0, 40,line, STYLE_NORMAL);
  sprintf(line,"Sunset/Sunrise %02d min",SunChangeLenght);
  ssd1306_printFixed (0, 48,line, STYLE_NORMAL);

  //DEBUG
  //Time t;
  //t=rtc.getTime();
  //sprintf(line,"%1d:%04d.%02d.%02d %02d:%02d:%02d",t.dow,t.year,t.mon,t.date,t.hour,t.min,t.sec);
  //ssd1306_printFixed (0, 40,line, STYLE_NORMAL);
  //myOLED.print(DebugStr, LEFT, 56);
//  myOLED.update();
  //Cleaning display buffer
//  myOLED.clrScr();
}

// ROUTINE TO CHANGE NUMBER OF CURRENTLY ENABLED LEDS____________________________________________________________________________________________________________________________
void ChangeLEDs(){
  //Reshaping RTC strings and updating clock variables
  byte i=0;
  byte CurHour=0;  // Current Hour 0-23
  byte CurMin=0;   // Current minute 0-59
  String RawTime = rtc.getTimeStr();         // Getting raw string of time
  CurHour=(RawTime.substring(0,2)).toInt();      // Getting Hours Value string
  CurMin=(RawTime.substring(3,5)).toInt();       // Getting Minutes Value string
  // Getting values only for display
  String DOW = "";
  DOW=String(rtc.getDOWStr());
  ShortDOW=DOW.substring(0,2);
  
  int LEDStepMins = 0;
  if(LightLevel!=1 or LightLevel!=0){                      // Division by zero protection
    LEDStepMins=int(SunChangeLenght/(LightLevel-1));       // Time before turning on or off different groups of leds - calculated from desired brightness and length of dawn/dusk 
  }else if(LightLevel=1){                                  // When we need to power on only one LEd
    LEDStepMins=0;
  }else if(LightLevel=0){
    LEDStepMins=0;                                         // It does not matter - later we have another check for 0 level
  }
  
  //  Setting light to reflect time requirements

  // Detecting Time of Day condition and setting flags
  if (CurHour==DuskHour and (CurMin <= SunChangeLenght)){                                                // GOOD MORNING
    TimeOfDay=0;         // Dusk flag set - Dusk Started, Night ended
  }else if(CurHour==DawnHour and (CurMin <= SunChangeLenght)){                                           // GOOD EVENING
    TimeOfDay=2;         // Dawn flag set - Dawn Started, Day ended
  }else if((CurHour<DawnHour and CurHour>DuskHour) or (CurHour==DuskHour and CurMin > SunChangeLenght)){ // GOOD DAY
    TimeOfDay=1;         // Day flag set - Day Started, Dusk ended
  }else{                                                                                                 // GOOD NIGHT
    TimeOfDay=3;         // Night flag set - Night Started, Dawn ended
  }
  switch(TimeOfDay){  // Altering LED groups according to parameters
    case 0:           // Morning - turning groups 1 by 1 until Light Level reached (CurLightLevel=0 by now)
    if (CurMin==CurLightLevel*LEDStepMins){
    // If we reach minute when relay should be activated then start acting. Known issue - altering LightLevel while in case 0 can lead to incorrect number of LEDs in down - Day will correct it
      if(LightLevel !=0){                           //If no light desired - skip even first step
        digitalWrite(LEDPin[CurLightLevel],HIGH);   // Turning on relay
        LEDRelayState[CurLightLevel]=true;          // Updating corresponding array
        CurLightLevel=CurLightLevel+1;              // Increasing Light Level
//      }
      }
    }
    break;
    case 1:           //Day - checking current level, and altering light if LightLevel changes externally (CurLightLevel can be from 0 to 8 by now)
    if (CurLightLevel!=LightLevel){             //If CurrentLightLevel changed - we just change all relays accordingly
      for(i=0;i<=(LightLevel-1);i++){           // if LightLevel=0 - no leds will turn on, if LightLevel=1 - 0nly 1 relay will go up etc.
        digitalWrite(LEDPin[i],HIGH);
        LEDRelayState[i]=true;
      }
      for(i=LightLevel;i<=LEDPinsQty-1;i++){    // if LightLevel=0 - all relays turn off, if LightLevel=8 - no relays will go down.
        digitalWrite(LEDPin[i],LOW);
        LEDRelayState[i]=false;
      }
      digitalWrite(LEDPin[i],HIGH);
    }
    break;
    case 2:           // Evening - turning off 
    if (CurMin==(SunChangeLenght-(CurLightLevel-1)*LEDStepMins)){
      // If we reach minute when relay should be activated then start acting. Known issue - altering LightLevel while in case 2 can lead to incorrect number of LEDs to turn off - Night will correct it
      if(CurLightLevel !=0){                       //If no light were on during the day - skip even first step
        digitalWrite(LEDPin[CurLightLevel-1],LOW);   // Turning off relay
        LEDRelayState[CurLightLevel-1]=false;        // Updating corresponding array
        CurLightLevel=CurLightLevel-1;             // Decreasing Light Level
      }
    }
    break;
    case 3:           //Night - just making sure every 5 minutes what all relays are off (Pins in HIGH condition)
    if(CurMin%5==0){  // CHANGE to 5
      for(i=0;i<=(LEDPinsQty-1);i++){
        if (LEDRelayState[i]=true){
          digitalWrite(LEDPin[i],LOW);
          LEDRelayState[i]=false;
        }
      CurLightLevel=0;
      }
    }
    break;
    default:
    break;
  }
  //DEBUG
  //DebugStr=String(TimeOfDay)+"|"+String(CurMin)+"|"+String(CurLightLevel)+"|"+String(LEDStepMins)+"|"+String(SunChangeLenght-CurLightLevel*LEDStepMins)+"p"+String(LEDPin[CurLightLevel-1])+"|"+String(ReachCenter);

}


// ROUTINE TO CALCULATE NUMBER OF LEDS CURRENTLY ENABLED___________________________________________________________________________________________________________________
void CalcTotalLeds(){
  TotalLEDs=0;
  byte i=0;
  for(i=0;i<=LightLevel-1;i++){
    TotalLEDs=TotalLEDs+NumLeds[i];
  }
}

// ROUTINE TO CHANGE TEMPERATURE OF WATER BY ENGAGING FAN OR HEATER__________________________________________________________________________________________________________
void ChangeTemp(){            
  if (WatTemp.WatTempFl<MinTemp){                                         // Too cold, enabling heater, disabling cooler (should not be enabled), setting flag
    ReachCenter=true;
    digitalWrite(HeatPin,HIGH);
    digitalWrite(FanPin,LOW);
    HeatOn=true;
    FanOn=false;
  }else if(WatTemp.WatTempFl>MaxTemp){                                    // Too hot, enabling fan, disabling heater (should not be enabled), setting flag
    ReachCenter=true;
    digitalWrite(FanPin,HIGH);
    digitalWrite(HeatPin,LOW);
    FanOn=true;
    HeatOn=false;
  }else if(abs(WatTemp.WatTempFl-(MaxTemp+MinTemp)/2)<=0.5 and ReachCenter==true){
    ReachCenter=false;
    digitalWrite(FanPin,LOW);
    digitalWrite(HeatPin,LOW);
    FanOn=false;
    HeatOn=false;
  }else if(WatTemp.WatTempFl<=MaxTemp and WatTemp.WatTempFl>=MinTemp and ReachCenter==false){     //Normal temperature, disabling both heater and cooler
    digitalWrite(FanPin,LOW);
    digitalWrite(HeatPin,LOW);
    FanOn=false;
    HeatOn=false;
  }else if(WatTemp.WatTempFl>MaxTemp and HeatOn==true){                    // Safe measure - if water values are "jumped" over reaching center by any hw glitch
    ReachCenter=true;                                                     // Reversing heater and cooler
    digitalWrite(FanPin,HIGH);
    digitalWrite(HeatPin,LOW);
    FanOn=true;
    HeatOn=false;
  }else if(WatTemp.WatTempFl<MinTemp and FanOn==true){                    // Safe measure - if water values are "jumped" over reaching center by any hw glitch
    ReachCenter=true;
    digitalWrite(FanPin,HIGH);
    digitalWrite(HeatPin,LOW);
    FanOn=true;
    HeatOn=false;
  }
}
// ROUTINE TO RECIEVE CORRECTIONS FROM WiFi BOARD AND UPDATE LOCAL VALUES TO IT_______________________________________________________________________________________________
void receiveEvent(int howMany) {
  // Variables for values accepted via I2C
  uint8_t CorDoW=1;                                                   // Correct Day of Week (1 bytes)
  uint16_t CorYear;                                                   // Correct Year
  uint8_t CorYearH;                                                   // Correct Year High Byte (1 byte)
  uint8_t CorYearL;                                                   // Correct Year Low Byte (1 byte)
  uint8_t CorMon;                                                     // Correct Month (1 byte)
  uint8_t CorDay;                                                     // Correct Day (1 byte)
  uint8_t CorHour;                                                    // Correct Hour (1 byte)
  uint8_t CorMin;                                                     // Correct Minute (1 byte)
  uint8_t CorSec;                                                     // Correct Second (1 byte)
  uint8_t MaxTempCor;                                                 // Maximum temperature corrected
  uint8_t MinTempCor;                                                 // Minimum temperature corrected
  uint8_t LightLevelCor;                                              // Desired light level corrected
  uint8_t DayStartCor;                                                // Desired start of day corrected
  uint8_t DayEndCor;                                                  // Desired end of day corrected
  uint8_t SunChangeLengthCor;                                         // Desired sunrise/sunset length corrected

  if (CorBytesExpected==Wire.available()){                            // We expect 14 bytes, no more no less
    CorDoW=Wire.read();                                               // Getting Correct DoW as number
    CorYearH=Wire.read();                                             // Getting Correct Year High Byte
    CorYearL=Wire.read();                                             // Getting Correct Year Low Byte
    CorYear=uint16_t(CorYearH,CorYearL);                              // Reassembling Correct Year
    CorMon=Wire.read();                                               // Getting Correct Month 
    CorDay=Wire.read();                                               // Getting Correct Correct Day
    CorHour=Wire.read();                                              // Getting Correct Correct Hour
    CorMin=Wire.read();                                               // Getting Correct Correct Minute
    CorSec=Wire.read();                                               // Getting Correct Correct Second
    MinTempCor=Wire.read();                                           // Getting Minimum temperature corrected
    MaxTempCor=Wire.read();                                           // Getting Maximum temperature corrected
    LightLevelCor=Wire.read();                                        // Getting Desired light level corrected
    DayStartCor=Wire.read();                                          // Getting Desired start of day corrected
    DayEndCor=Wire.read();                                            // Getting Desired end of day corrected
    SunChangeLengthCor=Wire.read();                                   // Getting Corrected Sunset Change Length
    // Transmission ended.
    Time t;
    t=rtc.getTime();
  //sprintf(line,"%1d:%04d.%02d.%02d %02d:%02d:%02d",t.dow,t.year,t.mon,t.date,t.hour,t.min,t.sec);

    if(t.dow!=CorDoW){                                                // No point in writing to chip if DoW is the same
      switch (CorDoW){
        case 0:                                                       // Cant be
        break;
        case 1:
        rtc.setDOW(MONDAY);                                             
        break;
        case 2:
        rtc.setDOW(TUESDAY);
        break;
        case 3:
        rtc.setDOW(WEDNESDAY);
        break;
        case 4:
        rtc.setDOW(THURSDAY);
        break;
        case 5:
        rtc.setDOW(FRIDAY);
        break;
        case 6:
        rtc.setDOW(SATURDAY);
        break;
        case 7:
        rtc.setDOW(SUNDAY);
        break;
        default:                                                      // Cant be
        break;
      }
    }
    if(t.hour!=CorHour or t.min!=CorMin){                             // No point in writing to chip if hh:mm is the same (ignoring seconds on check, bc as of now, on time change we trensfer ss=0, may add [or t.sec!=CorSec] later
      rtc.setTime(CorHour,CorMin,CorSec);                             // Setting time - anly if we really transfer new hours or minutes, in that case we set 0 seconds. 
    }
    if(t.year!=CorYear or t.mon!=CorMin or t.date!=CorDay){           // No point in writing to chip if date is the same
      rtc.setDate(CorDay,CorMon,CorSec);                              // Set date 
    }
    if(15<=MinTempCor<=35){                                           // Checking for borders
      MinTemp=MinTempCor;                                             // Setting Minimum temp
      if(MinTemp>=MaxTemp){
        MaxTemp=MinTemp+1;                                            // Auto-Altering Maximum temp to reflect Minimum change
      }
    }
    if(15<=MaxTempCor<=35){                                           // Checking for borders and not being larger then maximum
      MaxTemp=MaxTempCor;                                             // Setting maximum temp
      if(MinTemp>=MaxTemp){
        MinTemp=MaxTemp-1;                                            // Auto-Altering Minimum Temp to reflect Maximum change
      }
    }
    if(0<=LightLevelCor<=8){                                          // Checking for bogus Level values
      LightLevel=LightLevelCor;                                       // Settinf Light Level value
    }
    if(1<=DayStartCor<=22){                                           // Checking for bogus Day Start value
      DuskHour=DayStartCor;                                           // Setting day start hour
      if(DuskHour>=DawnHour){
        DawnHour=DuskHour+1;                                          // Auto adjusting Day End to Day Start
      }
    }
    if(2<=DayEndCor<=23){                                             // Checking for bogus Day End value
      DawnHour=DayEndCor;                                             // Setting day end hour
      if(DawnHour>=DuskHour){
        DuskHour=DawnHour-1;                                          // Auto Adjusting Day Start to Day End
      }
    }
    if(0<=SunChangeLengthCor<=59){                                    // Checking for bogus Sunrise/Sunset Duration value
      SunChangeLenght=SunChangeLengthCor;                             // Setting Sunrise/Sunset Duration
    }
//      WriteWaterValuesToRAM();                                        // Writing current values to RAM on RTC
  }
}

// ROUTINE ANSWER TO DATA REQUEST FROM WIFI AND SEND LOCAL VALUES_________________________________________________________________________________________________________________
void requestEvent(){                                                    // Routine to answer to WiFi chip with updated values 21 Bytes Total
  Time t;
  t=rtc.getTime();
  Wire.write(t.dow);                                                    // Sending Day of Week (1 bytes, 1=Monday, 7 = Sunday etc)
  Wire.write(highByte(t.year));                                         // Sending year (1 of 2 bytes)
  Wire.write(lowByte(t.year));                                          // Sending year (2 of 2 bytes)
  Wire.write(t.mon);                                                    // Sending months (1 byte)
  Wire.write(t.date);                                                   // Sending days (1 byte)
  Wire.write(t.hour);                                                   // Sending hours (1 byte)
  Wire.write(t.min);                                                    // Sending minutes (1 byte)
  Wire.write(t.sec);                                                    // Sending seconds (1 byte) 8 by now
  Wire.write(DuskHour);                                                 // Sending day start (1 byte)
  Wire.write(DawnHour);                                                 // Sending day end (1 byte)
  Wire.write(SunChangeLenght);                                          // Sending Light Level (day normal)(1 byte)
  Wire.write(LightLevel);                                               // Sending Light Level (day normal)(1 byte)
  Wire.write(CurLightLevel);                                            // Sending Current Light Level (1 byte)
  Wire.write(OutTemp);                                                  // Sending Air Temperature (1 byte)
  Wire.write(OutHum);                                                   // Sending Air Humidity (1 byte) 15 by now
  for(int i=0;i<=3;i++){
    Wire.write(WatTemp.WatTemp8[i]);                                    // Sending each byte of Water Temp (4 bytes total)  
  }
  Wire.write(FanOn);                                                    // Sending Fan status (0 or 1) (1 byte)
  Wire.write(HeatOn);                                                   // Sending Heater status (0 or 1) (1 byte)
}
 

