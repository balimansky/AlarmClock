#include <TimeLib.h>
#include <Wire.h>                                                                //include the Wire library (necessary for I2C and SPI protocols)
#include <ADXL345.h>                                                             //include the ADXL345 library (must be downloaded from: https://dl.dropbox.com/u/43421685/Website%20Content/ADXL345_library.zip and placed in Arduino --> libraries). Contains register addresses and functions.
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "images.h"
#include "SSD1306.h"
#include "OLEDDisplayUi.h"
#include "ThingSpeak.h"
#include "TickerScheduler.h"


char ssid[] = "FiOS-KNRXW";    //  your network SSID (name) 
char pass[] = "flux2383gap9255hug";   // your network password
int status = WL_IDLE_STATUS;
WiFiClient  client;

unsigned long weatherStationChannelNumber = 203041;
const char* myReadAPIKey = "RBFRKX8OP1LBN84S";
String cEveWake, cBellaWake, cWeather, cTime, cHighTemp, cLowTemp;
TickerScheduler ts(1);

//See datasheet for tuning instructions. I found the following settings to work well (for my application). Everything set to 0 is not being used.

#define THRESH_TAP 0x30                                                          //Tap threshold value
#define OFSX 0                                                                   //X-axis offset value  
#define OFSY 0                                                                   //Y-axis offset valuevalue
#define OFSZ 0                                                                   //Z-axis offset value
#define DUR 0x30                                                                 //Tap duration value
#define LATENT 0x40                                                              //Tap Latency value
#define WINDOW 0xFF                                                              //Tap window value
#define THRESH_ACT 0                                                             //Activity threshold value
#define THRESH_INACT 0                                                           //Inactivity threshold value
#define TIME_INACT 0                                                             //Inactivity time value
#define ACT_INACT_CTL 0                                                          //Axis enable control for activity and inactivity detection value
#define THRESH_FF 0                                                              //Free-fall threshold value
#define TIME_FF 0                                                                //Free-fall time value
#define TAP_AXES B00000111                                                       //Axis control for single tap/double tap value 
#define BW_RATE 0                                                                //Data rate and power mode control value
#define POWER_CTL B00001001                                                      //Power-saving features control value
#define INT_ENABLE B01100000                                                     //Interrupt enable control value
#define INT_MAP B00100000                                                        //Interrupt mapping control value
#define DATA_FORMAT B00001010                                                    //Data format control value
#define FIFO_CTL 0                                                               //FIFO control value

byte X0, X1;                                                                     //variables to store incoming data
byte Y0, Y1;
byte Z0, Z1;
byte int_source;

boolean singleTap = false;                                                        //declare and initialize boolean variables to track tap type
boolean doubleTap = false;
ADXL345 myACC = ADXL345();                                                        //create an instance of class ADXL345() named myACC
SSD1306  display(0x3c, D2, D1);
OLEDDisplayUi ui     ( &display );

void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
//  display->drawString(128, 0, String(millis()));

  time_t t = now();
  String ct = String(hour(t)) + ":" + String(minute(t)) + ":" + String(second(t));
  display->drawString(128,0,ct);
}

void drawWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawStringMaxWidth(0 + x, 10 + y, 128, cWeather);
}

void drawClothing(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  char str[] = "This is a really long clothes string and will be displayed with lots of detail";
  display->drawStringMaxWidth(0 + x, 10 + y, 128, str);
}

void drawMessage(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  char str[] = "This is a really long message string and will be displayed with lots of detail";
  display->drawStringMaxWidth(0 + x, 10 + y, 128, str);
}

FrameCallback frames[] = { drawWeather, drawClothing, drawMessage};
int frameCount = 3;
OverlayCallback overlays[] = { msOverlay };
int overlaysCount = 1;

#include <SoftwareSerial.h>
#define ARDUINO_RX 2//should connect to TX of the Serial MP3 Player module D4
#define ARDUINO_TX 0 //connect to RX of the module D3
SoftwareSerial mySerial(ARDUINO_RX, ARDUINO_TX);
static int8_t Send_buf[8] = {0} ;

#define CMD_SEL_DEV 0X09
#define DEV_TF 0X02
#define CMD_PLAY_W_VOL 0X22
#define CMD_PLAY 0X0D
#define CMD_PAUSE 0X0E
#define CMD_PREVIOUS 0X02
#define CMD_NEXT 0X01

void setup()
{
  Serial.begin(9600);
  WiFi.begin(ssid, pass);

  ui.setTargetFPS(30);
  ui.setActiveSymbol(activeSymbol);
  ui.setInactiveSymbol(inactiveSymbol);
  ui.setIndicatorPosition(BOTTOM);
  ui.setIndicatorDirection(LEFT_RIGHT);
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.setFrames(frames, frameCount);
  ui.setOverlays(overlays, overlaysCount);
  ui.init();
  display.flipScreenVertically();
  
  display.setFont(ArialMT_Plain_24);
  display.drawString(0, 26, "Connecting to WiFi");
  display.display();
  while (WiFi.status() != WL_CONNECTED)
  {
    display.drawString(0, 40, "*");
    delay(500);
    Serial.print(".");
    display.drawString(0, 40, "  ");
    display.display();
  }
  Serial.println("WiFi connected");
  ThingSpeak.begin(client); 

  // This sets up a simple polling to execute the channel read every minute or so
  
  ts.add(0, 60000, readThingSpeakChannel, true); 
  
  attachInterrupt(12, interrupt0, RISING);                                        //attach an interrupt on digital pin 2 (interrupt 0) for rising signal  
  attachInterrupt(13, interrupt1, RISING);                                        //attach an interrupt on digital pin 3 (interrupt 1) for rising signal
  pinMode(14, OUTPUT);                                                            //set LED pins to output
  // pinMode(5, OUTPUT);
  myACC.setTHRESH_TAP(THRESH_TAP);                                               //set registers on the ADXL345 to the above defined values (see datasheet for tuning and explanations)
  myACC.setDUR(DUR);
  myACC.setLATENT(LATENT);
  myACC.setWINDOW(WINDOW);
  myACC.setTAP_AXES(TAP_AXES);
  myACC.setPOWER_CTL(POWER_CTL);
  myACC.setINT_MAP(INT_MAP);
  myACC.setDATA_FORMAT(DATA_FORMAT);
  myACC.setINT_ENABLE(INT_ENABLE);

  myACC.setDATA_FORMAT(0x01);
  myACC.setPOWER_CTL(0x08);

  mySerial.begin(9600);
  delay(500);//Wait chip initialization is complete
  sendCommand(CMD_SEL_DEV, DEV_TF);//select the TF card 
  delay(200);//wait for 200ms
}

void loop()                                                                      //main loop
{
  myACC.readDATAX(&X0, &X1);                                                     //use readDATA functions (see library) and print values
  myACC.readDATAY(&Y0, &Y1);
  myACC.readDATAZ(&Z0, &Z1);

  int x = ((int)X1 << 8) | X0;   
  int y = ((int)Y1 << 8) | Y0;
  int z = ((int)X1 << 8) | Z0;
//  Serial.print("x: ");
//  Serial.print( x );
//  Serial.print(" y: ");
//  Serial.print( y );
//  Serial.print(" z: ");
//  Serial.println( z );

//  Serial.print(X0 + (X1 << 8));
//  Serial.print("\t");
//  Serial.print(Y0 + (Y1 << 8));
//  Serial.print("\t");
//  Serial.println(Z0 + (Z1 << 8));

  myACC.readINT_SOURCE(&int_source);                                             //read the interrupt source register (Important! If this register is not read every iteration the interrupts on the ADXL345 will not reset, and therefore not function)
  //Serial.println(int_source, BIN);
  
  if(singleTap)                                                                  //if single tap is detected
  {
    delay(100);                                                                  //wait half a second to see if there is another tap and if not, procedd to blink pin 4 (which will light the LED red)
    if(!doubleTap)
    {
      //Serial.println("Single Tap!");
      digitalWrite(14, HIGH);
      delay(100);
      digitalWrite(14, LOW);
      sendCommand(CMD_NEXT, 0X01);
    }
    else                                                                         //if a second tap is deceted during the half second wait, preceed to blink pin 5 (which will light the LED green)
    {
      //Serial.println("Double Tap");
      digitalWrite(14, HIGH);
      delay(100);
      digitalWrite(14, LOW);
      delay(100);
      digitalWrite(14, HIGH);
      delay(100);
      digitalWrite(14, LOW);
      sendCommand(CMD_PREVIOUS, 0X02);
    }
    singleTap = false;                                                          //reset tap values to ensure the lights stop blinking until the next tap sets either singleTap or doubleTap to true (see below)
    doubleTap = false;
  }

  int remainingTimeBudget = ui.update();
  if (remainingTimeBudget > 0) {
    delay(remainingTimeBudget);
  }

  ts.update();
}

void interrupt0()                                                              //a RISING on interrupt pin 2 (interrupt 0) would mean a single tap (from how we have wired and configured the ADXL345), therefore, set singleTap to true
{
  singleTap = true;
}

void interrupt1()                                                              //a RISING on interrupt pin 3 (interrupt 1) would mean a double tap (again, from how we have wired and configured the ADXL345), therefore, set doubleTap to true
{
  doubleTap = true;
}

void sendCommand(int8_t command, int16_t dat)
{
  delay(20);
  Send_buf[0] = 0x7e; //starting byte
  Send_buf[1] = 0xff; //version
  Send_buf[2] = 0x06; //the number of bytes of the command without starting byte and ending byte
  Send_buf[3] = command; //
  Send_buf[4] = 0x00;//0x00 = no feedback, 0x01 = feedback
  Send_buf[5] = (int8_t)(dat >> 8);//datah
  Send_buf[6] = (int8_t)(dat); //datal
  Send_buf[7] = 0xef; //ending byte
  for(uint8_t i=0; i<8; i++)//
  {
    mySerial.write(Send_buf[i]) ;
  }
} 

void readThingSpeakChannel()
{
  Serial.println("reading channel now");
  
  cWeather = ThingSpeak.readStringField(weatherStationChannelNumber, 1, myReadAPIKey);
  cHighTemp = ThingSpeak.readStringField(weatherStationChannelNumber, 2, myReadAPIKey);
  cLowTemp = ThingSpeak.readStringField(weatherStationChannelNumber, 3, myReadAPIKey);    
  cEveWake = ThingSpeak.readStringField(weatherStationChannelNumber, 4, myReadAPIKey);
  cBellaWake = ThingSpeak.readStringField(weatherStationChannelNumber, 5, myReadAPIKey);
  cTime = ThingSpeak.readStringField(weatherStationChannelNumber, 6, myReadAPIKey);

  Serial.println(cWeather);
  Serial.println(cHighTemp);
  Serial.println(cLowTemp);
  Serial.println(cEveWake);
  Serial.println(cBellaWake);
  Serial.println(cTime);
  
  String h = cTime.substring(0,2);
  String m = cTime.substring(3,5);
  String s = cTime.substring(6,8);
  String dd = cTime.substring(9,11);
  String mm = cTime.substring(12,14);
  String yyyy = cTime.substring(15);

  Serial.println(h + ":" + m + ":" + s + ":" + dd + ":" + mm + ":" + yyyy);
  setTime(h.toInt(), m.toInt(), s.toInt(), dd.toInt(), mm.toInt(), yyyy.toInt());

}

