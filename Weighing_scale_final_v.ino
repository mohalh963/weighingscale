#include <HX711_ADC.h>
#include <Wire.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h> // Hardware-specific library
#include <Adafruit_STMPE610.h> //touchscreen library
#include <Fonts/FreeSans9pt7b.h> 
#include "config.h" // network & AIO config
#include <BlockNot.h> 

//define display pins
#define STMPE_CS 32
#define TFT_CS   15
#define TFT_DC   33
#define SD_CS    14

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
Adafruit_STMPE610 ts = Adafruit_STMPE610(STMPE_CS);

//Human interface buttons
boolean buttonScaleOn = true;
boolean buttonInfoOn  = true;

//AIO feed
AdafruitIO_Feed *GYARB1 = io.feed("GYARB1");

//define touchscreen coordinates
#define TS_MINX 3800
#define TS_MAXX 100
#define TS_MINY 100
#define TS_MAXY 3750

//pins:
const int HX711_dout = 16; //mcu > HX711 dout pin
const int HX711_sck = 22; //mcu > HX711 sck pin

//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

unsigned long t = 0;

BlockNot AIOSAVE(3000); //In Milliseconds 

void setup() {
  Serial.begin(115200); delay(10);
  Serial.println();
  Serial.println("Starting...");
  
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());

  ts.begin();
  tft.begin();
  tft.setFont(&FreeSans9pt7b);
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  

  tft.drawRect(10,10, 110, 60, ILI9341_GREEN);
  tft.setCursor(17,50);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("Scale");

  tft.drawRect(10, 85, 110, 60, ILI9341_GREEN);
  tft.setCursor(17, 125 );
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("info");

  tft.drawRect(10, 160, 110, 60, ILI9341_GREEN);
  tft.setCursor(17, 200 );
  tft.setTextColor(ILI9341_RED);
  tft.setTextSize(2);
  tft.print("Reset");

  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(130, 100);
  tft.setTextSize(1);
  tft.print(io.statusText());
  tft.setCursor(160, 160);
  tft.setTextSize(1);
  tft.print("Please select a");
  tft.setCursor(180, 180);
  tft.print("function");
  Serial.println("start loop");
  

  LoadCell.begin();
  //LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  }
  else {
    LoadCell.setCalFactor(452.0); // user set calibration value (float), initial value 1.0 may be used for this sketch
    Serial.println("Startup is complete");
  }
}

void loop() {
  static boolean newDataReady = 0;
  const int serialPrintInterval = 0; //increase value to slow down serial print activity

  TS_Point p = ts.getPoint();
  Serial.print("\tX = "); Serial.println(p.x);
  Serial.print("\tY = "); Serial.println(p.y);
  Serial.print("\tPressure = "); Serial.println(p.z);
  Serial.println("\t__________");
  p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
  p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());
  
  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  if (p.z > 10) { //avoid misstouch
    if (p.x > 240 and p.x < 320 and p.y > 0 and p.y < 100) {
      tft.fillRect(125, 0, 195, 230, ILI9341_BLACK);
      buttonScaleOn = false;
      buttonInfoOn  = true;
    }

    else if (p.x > 107 and p.x < 213 and p.y > 0 and p.y < 100){
      tft.fillRect(125, 0, 195, 230, ILI9341_BLACK);
      buttonScaleOn = true;
      buttonInfoOn  = false;
      
    }
    else if (p.x > 0 and p.x < 106 and p.y > 0 and p.y < 100){
    tft.fillScreen(ILI9341_RED);
    tft.setCursor(50, 100);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.print("Resetting in");
    tft.setCursor(80, 150);
    tft.print("10 sec.");
    delay(10000);
    ESP.restart();  
    }
  }
  if (buttonScaleOn == 0) {
      tft.drawRect(130, 160, 175, 60, ILI9341_GREEN);
      tft.setCursor(157, 200 );
      tft.setTextColor(ILI9341_GREEN);
      tft.setTextSize(2);
      tft.print("Set to 0");
      tft.setCursor(150, 50);
      tft.print("WEIGHT");
      tft.setCursor(280, 100);
      tft.print("g"); 
      int i = LoadCell.getData();
      Serial.print("Current weight: ");
      Serial.println(i);
      newDataReady = 0;
      t = millis();
      tft.fillRect(150, 70, 130, 40, ILI9341_BLACK);
      tft.setCursor(150, 100);
      tft.print(i);
      

      if (p.x > 0 and p.x < 106 and p.y > 130 and p.y < 320){
        unsigned long stabilizingtime2 = 0; // no stabilizing time needed here
        boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
        LoadCell.start(stabilizingtime2, _tare);
      
      }  
  }
  else if (buttonInfoOn == 0){
    tft.setCursor(130, 40);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);
    tft.print("Weighing scale by");
    tft.setCursor(130, 60);
    tft.print("Mohammad Alhussein");
    tft.setCursor(130, 80);
    tft.print("Joel Petersson");
    tft.setTextColor(ILI9341_BLUE);
    tft.setCursor(130, 110);
    tft.print("For more visit:");
    tft.setCursor(130, 130);
    tft.print("tinyurl.com/yeywafsn");
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(130, 160);
    tft.print("Pauliskolan 2022");
    tft.setCursor(130, 180);
    tft.print("Axel Mansson");
    tft.setCursor(130, 200);
    tft.print("github @mohalh963");
        
  }
  if (AIOSAVE.TRIGGERED){
    int i = LoadCell.getData();
    GYARB1 -> save(i);
  }
} 
  
