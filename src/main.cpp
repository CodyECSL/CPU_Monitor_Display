#include <Arduino.h>
#include <ArduinoJson.h> //https://arduinojson.org/v6/assistant/
#include <MillisTimer.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <config.h>
#include <map>
#include <vector>
#include <Fonts/Free_Fonts.h>
#include <Fonts/FuturaLT_Bold24pt7b.h>
#include <Fonts/FuturaLT_Bold14pt7b.h>
#include <Gifs/EKWB.h>
#include <Gifs/HelloThere.h>


#define LED_ON HIGH
#define LED_OFF LOW

#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN 0x10
#endif

#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS 5
#define TFT_DC 16
#define TFT_RST 23

#define TFT_BL 4 // Display backlight control pin
#define ADC_EN 14
#define ADC_PIN 34
#define BUTTON_1 35
#define BUTTON_2 0

// Supposed dimensions
#define SCREEN_HEIGHT 135
#define SCREEN_WIDTH 240

struct SENSOR_DATA {
    String label;
    String value;
    String unit;
};
SENSOR_DATA sensorDataDummyStruct = {};

// auto evgaMainGif = { EvgaMainGif_0, EvgaMainGif_1, EvgaMainGif_2, EvgaMainGif_3, EvgaMainGif_4, EvgaMainGif_5, EvgaMainGif_6, EvgaMainGif_7, EvgaMainGif_8, EvgaMainGif_9, EvgaMainGif_10, EvgaMainGif_11, EvgaMainGif_12, EvgaMainGif_13, EvgaMainGif_14, EvgaMainGif_15 };
// auto evgaIntroGif = { EvgaIntro_0, EvgaIntro_1, EvgaIntro_2, EvgaIntro_3, EvgaIntro_4, EvgaIntro_5, EvgaIntro_6, EvgaIntro_7, EvgaIntro_8, EvgaIntro_9, EvgaIntro_10, EvgaIntro_11, EvgaIntro_12, EvgaIntro_13, EvgaIntro_14, EvgaIntro_15, EvgaIntro_16, EvgaIntro_17, EvgaIntro_18, EvgaIntro_19, EvgaIntro_20, EvgaIntro_21, EvgaIntro_22 };
// auto nvidiaGif = { NvidiaRtx_0, NvidiaRtx_1, NvidiaRtx_2, NvidiaRtx_3, NvidiaRtx_4, NvidiaRtx_5, NvidiaRtx_6, NvidiaRtx_7, NvidiaRtx_8, NvidiaRtx_9, NvidiaRtx_10, NvidiaRtx_11, NvidiaRtx_12, NvidiaRtx_13 };
// auto KnacklesGif = {knackles001, knackles002, knackles003, knackles004, knackles005, knackles006, knackles007, knackles008};
auto EKWBGif = { EKWB_00,EKWB_01,EKWB_02,EKWB_03,EKWB_04,EKWB_05,EKWB_06,EKWB_07,EKWB_08,EKWB_09,EKWB_10,EKWB_11 };
auto HelloThereGif = { HelloThere_00,HelloThere_01,HelloThere_02,HelloThere_03,HelloThere_04,HelloThere_05,HelloThere_06,HelloThere_07,HelloThere_08,HelloThere_09,HelloThere_10,HelloThere_11,HelloThere_12,HelloThere_13,HelloThere_14,HelloThere_15,HelloThere_16,HelloThere_17,HelloThere_18,HelloThere_19,HelloThere_20,HelloThere_21,HelloThere_22,HelloThere_23,HelloThere_24,HelloThere_25,HelloThere_26,HelloThere_27,HelloThere_28,HelloThere_29,HelloThere_30,HelloThere_31,HelloThere_32,HelloThere_33,HelloThere_34 };

// Update Status every 1 second
MillisTimer statusTimer = MillisTimer(250);
// Update Status every 2.5 seconds
MillisTimer gifTimer = MillisTimer(2500);
// Update Status every 10 seconds
MillisTimer interstitialGifTimer = MillisTimer(20000);

TaskHandle_t ReadSerialTask;

TFT_eSPI tft = TFT_eSPI(SCREEN_HEIGHT, SCREEN_WIDTH);
TFT_eSprite tftSprite = TFT_eSprite(&tft);

const size_t capacity = JSON_ARRAY_SIZE(221) + 221 * JSON_OBJECT_SIZE(6) + 18290;
DynamicJsonDocument doc(capacity);
DynamicJsonDocument docCopy(capacity);
bool firstIteration = true;
bool confirmedSerialConnection = false;
bool isStandardViewActive = true;

////////////////////////////////////////////////////////  Function Declerations  //////////////////////////////////////////////////////// 

void SetTextDisplayDefaults();
void StartTimers();
void StopTimers();
void DrawEKWB();
void DrawDisplayEvent(MillisTimer &mt);
void AnimateGifEvent(MillisTimer &mt);
void InterstitialGifEvent(MillisTimer &mt);
void DrawInterstitialGif();
void ReadSerial(void * parameter);
void DrawJsonDataToDisplay();
void CreateAsyncSerialTask();

////////////////////////////////////////////////////////  Setup & Loop  //////////////////////////////////////////////////////// 

void setup()
{

    Serial.end();
    Serial.begin(256000);

    SetTextDisplayDefaults();

    CreateAsyncSerialTask();    
    
    tft.fillScreen(TFT_BLACK);
    StartTimers();    
}

void loop()
{
    if (isStandardViewActive) {
        statusTimer.run();
        gifTimer.run();
        interstitialGifTimer.run();
    }
    
}

////////////////////////////////////////////////////////  Functions Implementations //////////////////////////////////////////////////////// 

void SetTextDisplayDefaults() 
{
    tft.init();
    // tft.setRotation(1); // USB Port on Right
    tft.setRotation(3); // USB Port on Left
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);

    tftSprite.setColorDepth(8);
    tftSprite.createSprite(120,135);
    tftSprite.fillSprite(TFT_BLACK);
    tftSprite.setTextColor(TFT_WHITE);
    tftSprite.setTextDatum(TL_DATUM);
}

void CreateAsyncSerialTask()
{
    // Set data fetching on Core 0 which leaves UI updates on Core 1
    xTaskCreatePinnedToCore(
      ReadSerial, /* Function to implement the task */
      "ReadSerialTaskOnCore0", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &ReadSerialTask,  /* Task handle. */
      0); /* Core where the task should run */
}

void StartTimers() {
    statusTimer.expiredHandler(DrawDisplayEvent);
    statusTimer.start();
    gifTimer.expiredHandler(AnimateGifEvent);
    gifTimer.start();
    interstitialGifTimer.expiredHandler(InterstitialGifEvent);
    interstitialGifTimer.start();
}

void StopTimers() {
    statusTimer.stop();
    gifTimer.stop();
    interstitialGifTimer.stop();
}

void DrawDisplayEvent(MillisTimer &mt) {
    DrawJsonDataToDisplay();
}

void AnimateGifEvent(MillisTimer &mt) {
    DrawEKWB();
}

void InterstitialGifEvent(MillisTimer &mt) {
    isStandardViewActive = false;
    StopTimers();
    DrawInterstitialGif();
    StartTimers();    
    tft.fillScreen(TFT_BLACK);
    DrawEKWB();
    DrawJsonDataToDisplay();
    isStandardViewActive = true;
}

void DrawInterstitialGif() {
    for (int i = 0; i < 3 ; i++) {
        for (auto it = begin(HelloThereGif); it != end(HelloThereGif); ++it)
        {
            tft.pushImage(0, 7, 240, 120, *it);
            delay(20);
        }
    }
}

void ReadSerial(void * parameter)
{
    while (true)
    {
        if (Serial.available() > 0)
        {
            String serialData = Serial.readStringUntil('\n');
            DeserializationError deserializationError = deserializeJson(doc, serialData);
            if (deserializationError)
            {
                // tft.drawString("DESERIALIZATION ERROR",0,0);
            }
            else
            {
                // Create a copy of the doc object so there is no contention of the same
                // object between cores
                confirmedSerialConnection = true;
                docCopy = doc;
                // Serial.println(serialData);
            }
                   
        } else {
            // tft.drawString("NO SERIAL",0,0);
        }
    }
    
}

void DrawJsonDataToDisplay () 
{
    size_t sizeOfJsonDoc = docCopy["r"].size();

    for (int i = 0 ; i < sizeOfJsonDoc ; i++ ) {
        
        // This is parsing the JSON data provided by remotehwinfo.exe
        // r = JSON root object 
        // a = Custom Label from Provided HWInfo Sensor
        // b = Measurement Unit
        // c = Measurement Value
        sensorDataDummyStruct = {
            docCopy["r"][i]["a"],
            docCopy["r"][i]["c"],
            docCopy["r"][i]["b"],
        };
        
        String label = sensorDataDummyStruct.label;
        String valueWithUnit =  String(sensorDataDummyStruct.value.toInt()) + " " + sensorDataDummyStruct.unit;
        // tft.setTextPadding(tft.textWidth(valueWithUnit) + 5);

        switch(i) {
            // case 0:
            //     if (firstIteration)
            //     {
            //         tft.setFreeFont(&GothamBook12pt7b);
            //         tft.drawString(label, 50, 5);
            //     }
            //     tft.setFreeFont(&FreeSans24pt7b);
            //     tft.drawString(valueWithUnit, 20, 50);
            //     break;
            case 0:
                if (firstIteration)
                {
                    // tft.setFreeFont(&GothamBook12pt7b);
                    // tft.setFreeFont(&FuturaLT_Bold14pt7b);
                    // tft.drawString(label, 140, 25);                    
                }
                // tft.setFreeFont(&FuturaLT_Bold14pt7b);
                // tft.drawString(label, 140, 25);          
                // // tft.setFreeFont(&Futura_Light24pt7b);
                // tft.setFreeFont(&FuturaLT_Bold24pt7b);
                // tft.drawString(valueWithUnit, 115, 65);


                // Using a Sprite for drawing text as this bypasses any text flickering issues.
                tftSprite.fillSprite(TFT_BLACK);
                tftSprite.setTextPadding(tftSprite.textWidth(label) + 5);
                tftSprite.setFreeFont(&FuturaLT_Bold14pt7b);
                tftSprite.drawString(label, 25, 25); 
                tftSprite.setTextPadding(tftSprite.textWidth(valueWithUnit) + 5);
                tftSprite.setFreeFont(&FuturaLT_Bold24pt7b);
                tftSprite.drawString(valueWithUnit, 0, 65); 
                tftSprite.pushSprite(115,0);

                break;
                // case 1:
            //     if (firstIteration)
            //     {
            //         tft.setFreeFont(&GothamBook12pt7b);
            //         tft.drawString(label, 150, 5);                    
            //     }
            //     tft.setFreeFont(&FreeSans24pt7b);
            //     tft.drawString(valueWithUnit, 135, 50);
            //     break;
            // case 2:
            //     if (firstIteration)
            //     {
            //         tft.drawString(label, 1, 45);    
            //     }
            //     tft.drawString(valueWithUnit, 1, 65);
            //     break;
            // case 3:
            //     if (firstIteration)
            //     {
            //         tft.drawString(label, 140, 45);
            //     }
            //     tft.drawString(valueWithUnit, 140, 65);
            //     break;
        }

        // This delay is here as you're fighting with the
        // display's refresh rate and refershing text too
        // quickly causes flickering on the screen. 
        // Delay time was increased until a majority of 
        // flickering was no longer observed.
        // if (!firstIteration)
        // {
        //     delay(350);
        // }
    }

    if (firstIteration && sizeOfJsonDoc > 0)
    {
        Serial.println("Finished first iteration");
        firstIteration = false;
    } 
}

void DrawEKWB()
{
    for (auto it = begin(EKWBGif); it != end(EKWBGif); ++it)
    {
        tft.pushImage(10, 27, 80, 80, *it);
        delay(30);
    }
}