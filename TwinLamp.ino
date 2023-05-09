//import the libraries

#include "config.h"
#include <Adafruit_NeoPixel.h>
#include <WiFiManager.h>
#include <time.h>
#include <stdlib.h>

#define N_LEDS 24
#define LED_PIN 5
#define BOT 12 // capacitive sensor pin

//////////////////LAMP ID////////////////////////////////////////////////////////////
int lampID = 2;
/////////////////////////////////////////////////////////////////////////////////////

Adafruit_NeoPixel strip(N_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Adafruit inicialization
AdafruitIO_Feed * lamp = io.feed("Lamp"); // Adafruit Feed Title

// Variables

int myValue  {0}; // Value of this lamp sent in the message
int otherValue {0}; // Value of the other lamp receieved in the message

const int maxIntensity = 255; //  Max intensity
int state = 0; // State
int selectedColor = 0;  //  Index for color vector
int selectedMode = 0; // Index for mode
int iBreath; // Breath mode variable

char msg[50]; //  Custom messages for Adafruit IO

//  Color definitions
uint32_t red = strip.Color(255, 0, 0);
uint32_t green = strip.Color(0, 255, 0);
uint32_t blue = strip.Color(0, 0, 255);
uint32_t magenta = strip.Color(255, 0, 255);
uint32_t cian = strip.Color(0, 255, 255);
uint32_t yellow = strip.Color(255, 255, 0);
uint32_t white = strip.Color(255, 255, 255);
uint32_t pink = strip.Color(255, 20, 30);
uint32_t orange = strip.Color(255, 50, 0);
uint32_t black = strip.Color(0, 0, 0);

uint32_t colors[] = {
  red,
  green,
  blue,
  magenta,
  cian,
  yellow,
  white,
  pink,
  orange,
  black
};


// Time vars
unsigned long referenceMillisecond;
unsigned long currentMillisecond;
unsigned long inactivityTime; // Variable to turn off when in selection mode
const int maximumInactivityTime = 120000; // 2 Minutes of maximum time on but inactive
const int nextColorTime = 2000; // 2 Seconds to change to the next color
const int nextModeTime = 5000; // 5 Seconds to change to the next mode
const int answerTimeOut = 900000; // 15 Minutes of waiting for an answer
const int connectionTime = 900000; // 15 Minutes of connection

// Disconection timeout
unsigned long currentWifiMilliseconds;
unsigned long previousWifiMilliseconds = 0;
const unsigned long wifiTimeOut = 600000; // Time to try and connect to configured WiFi

// Long press detection
const int minimumTouchTime = 1000; // 1 Second of touch to activate
int lastState = LOW;  // the previous state from the input pin
int currentState;     // the current reading from the input pin
unsigned long pressedTime  = 0; // When it is pressed
unsigned long releasedTime = 0; // When it is released

void setup() {
  Serial.begin(115200); //Start the serial monitor for debugging and status
  strip.begin(); // Activate neopixels
  strip.show(); // Initialize all pixels to 'off'
  wificonfig(); // Connect to WiFi
  pinMode(BOT, INPUT); // Declares button
  srand(time(0)); // Random generator

  //Start connecting to Adafruit IO
  Serial.printf("\nConnecting to Adafruit IO with User: %s Key: %s.\n", IO_USERNAME, IO_KEY);
  AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, "", "");
  io.connect();
  lamp -> onMessage(handle_message);

  while (io.status() < AIO_CONNECTED) { // While not connected, spin red
    Serial.print(".");
    spin(0);
    delay(500);
  }

  turn_off();
  Serial.println();
  Serial.println(io.statusText());

  // Successfull connection animation
  rainbow(5);  turn_off(); delay(50);
  flash(1); turn_off(); delay(100);
  flash(1); turn_off(); delay(50);

  //  Set ID values //
  if (lampID == 1) {
    myValue = 20;
    otherValue = 10;
  }
  else if (lampID == 2) {
    myValue = 10;
    otherValue = 20;
  }
  //  Set ID values //

  //Get the status of our value in Adafruit IO
  lamp -> get();
  sprintf(msg, "L%d: connected", lampID);
  lamp -> save(msg);
}

void loop() {
    currentWifiMilliseconds = millis();

    io.run();

    // State machine
    switch (state) {
      case 0: // Waiting for activation
        currentState = digitalRead(BOT);

        // Button is pressed
        if(lastState == LOW && currentState == HIGH)  
        {
          pressedTime = millis();
        }
        else if(currentState == HIGH) {
          releasedTime = millis();
          long pressDuration = releasedTime - pressedTime;
          if( pressDuration > minimumTouchTime )
          {
              state = 1;
          }
        }
        lastState = currentState;
        break;
        
      case 1: // Activated
        atYourCommand(1); // Green signal
        turn_off();
        state = 2;
        waitSignalLow(); // Advances only when not pressing anymore
        referenceMillisecond = millis();
        inactivityTime = millis();
        break;
        
      case 2: // Select mode
        if (selectedMode == 0){ // Breath mode
          for (iBreath = 0; iBreath <= 314; iBreath++) {
            currentMillisecond = millis();

            breath(8, iBreath);

            if (digitalRead(BOT) == HIGH) { // Touch to select
              turn_off();
              state = 25;
              waitSignalLow(); // Advances only when not pressing anymore
              inactivityTime = millis();
              break;
            } else if (currentMillisecond - referenceMillisecond > nextModeTime){ // After 5 seconds changes mode
              selectedMode = 1;
              referenceMillisecond = millis();
              break;
            }
          }
          break;
        }
        if (selectedMode == 1){ // Fire mode
          currentMillisecond = millis();

          strip.setBrightness(255);
          fire(8);

          if (digitalRead(BOT) == HIGH) { // Touch to select
              turn_off();
              waitSignalLow(); // Advances only when not pressing anymore
              inactivityTime = millis();
              state = 25;
              break;
          } else if (currentMillisecond - referenceMillisecond > nextModeTime){ // After 5 seconds changes mode
            selectedMode = 2;
            referenceMillisecond = millis();
          }
          break;
        }
        if (selectedMode == 2){ // Simple mode
          currentMillisecond = millis();

          light_full_intensity(8);

          if (digitalRead(BOT) == HIGH) { // Touch to select
              turn_off();
              waitSignalLow(); // Advances only when not pressing anymore
              state = 25;
              inactivityTime = millis();
              break;
          } else if (currentMillisecond - referenceMillisecond > nextModeTime){ // After 5 seconds changes mode
            selectedMode = 3;
            referenceMillisecond = millis();
          }
          break;
        }
        if (selectedMode == 3){ // Spin mode
          currentMillisecond = millis();

          while (currentMillisecond - referenceMillisecond < nextModeTime){
            currentMillisecond = millis();
            
            spin(8);

            if (digitalRead(BOT) == HIGH) { // Touch to select
                turn_off();
                waitSignalLow(); // Advances only when not pressing anymore
                state = 25;
                inactivityTime = millis();
                break;
            }
          }
          
          selectedMode = 4;
          referenceMillisecond = millis();
            
          break;
        }
        if (selectedMode == 4){ // Comet mode
          currentMillisecond = millis();

          while (currentMillisecond - referenceMillisecond < nextModeTime){
            currentMillisecond = millis();

            comet(8);

            if (digitalRead(BOT) == HIGH) { // Touch to select
                turn_off();
                waitSignalLow(); // Advances only when not pressing anymore
                state = 25;
                inactivityTime = millis();
                break;
            }
          }

          selectedMode = 5;
          referenceMillisecond = millis();

          break;
        }
        if (selectedMode == 5){ // Rainbow mode
          currentMillisecond = millis();

          rainbow(5);
          
          if (digitalRead(BOT) == HIGH) { // Touch to select
              turn_off();
              waitSignalLow(); // Advances only when not pressing anymore
              state = 25;
              inactivityTime = millis();
              break;
          } else if (currentMillisecond - referenceMillisecond > nextModeTime){ // After 5 seconds changes mode
            selectedMode = 6;
            referenceMillisecond = millis();
          }
          break;
        }
        if (selectedMode == 6){ // Disco mode
          currentMillisecond = millis();

          disco();

          if (digitalRead(BOT) == HIGH) { // Touch to select
              turn_off();
              waitSignalLow(); // Advances only when not pressing anymore
              state = 3;
              inactivityTime = millis();
              break;
          } else if (currentMillisecond - referenceMillisecond > nextModeTime){ // After 5 seconds changes mode
            selectedMode = 0;
            referenceMillisecond = millis();
          }
        }
        if (currentMillisecond - inactivityTime > maximumInactivityTime){
          fadeIn(0);
          fadeOut(0);
          state = 8;
        }
        break;
      case 25: // Select Color
        referenceMillisecond = millis();

        selectedColor++;
        
        if (selectedColor > 9){
          selectedColor = 0;
        }
        
        fadeIn(selectedColor);
        currentMillisecond = millis();
        
        while(currentMillisecond - referenceMillisecond < nextColorTime){
          currentMillisecond = millis();
          if (digitalRead(BOT) == HIGH) { // Touch to select
            waitSignalLow(); // Advances only when not pressing anymore
            state = 3;
            break;
          }
        }

        fadeOut(selectedColor);
        turn_off();
        
        if (currentMillisecond - inactivityTime > maximumInactivityTime){state = 8;}

        break;
      case 3: // Publish message
        sprintf(msg, "L%d: color send", lampID);
        lamp -> save(msg);
        lamp -> save( (selectedMode*100) + selectedColor + otherValue);
        
        state = 4;

        flash(selectedColor);
        light_half_intensity(selectedColor);
        delay(100);
        flash(selectedColor);
        light_half_intensity(selectedColor);
        fadeOut(selectedColor);
        delay(500);

        break;
        
      case 4: // Set timer
        referenceMillisecond = millis();
        state = 5;
        break;
        
      case 5: // Wait for answer
        currentMillisecond = millis();

        heartBeat(0);
        
        if (currentMillisecond - referenceMillisecond > answerTimeOut) {
          fadeIn(0);
          cancel(); turn_off(); delay(100);
          fadeIn(0);
          cancel(); turn_off(); delay(50);

          lamp -> save("L%d: Answer time out", lampID);
          lamp -> save(0);
          state = 8;
          break;
        }
        break;
        
      case 6: // Answer received
        referenceMillisecond = millis();
        
        light_full_intensity(selectedColor);
        
        sprintf(msg, "L%d: connected", lampID);
        lamp -> save(msg);
        lamp -> save(0);
        
        switch(selectedMode){
          case 0:
            state = 7;
            break;
          case 1:
            state = 71;
            break;
          case 2:
            state = 72;
            break;
          case 3:
            state = 73;
            break;
          case 4:
            state = 74;
            break;
          case 5:
            state = 75;
            break;
          case 6:
            state = 76;
            break;
          default:
            break;
        }
      case 7: // Established Beath Mode  
        currentMillisecond = millis();

        for (iBreath = 0; iBreath <= 314; iBreath++) {
          breath(selectedColor, iBreath);
          currentMillisecond = millis();
          
          if (digitalRead(BOT) == HIGH) { // Send Pulse
            lamp -> save(420 + otherValue);
            breathPulse(selectedColor);
          }
        }
        
        if (currentMillisecond - referenceMillisecond > connectionTime) {state = 8;}

        break;
      case 71: // Established Fire Mode
        currentMillisecond = millis();
        fire(selectedColor);
       
        if (digitalRead(BOT) == HIGH) { //  Send pulse
          lamp -> save(420 + otherValue);
          firePulse(selectedColor);
        }

        if (currentMillisecond - referenceMillisecond > connectionTime) {state = 8;}

        break;
      case 72: // Established Simple Mode
        currentMillisecond = millis();
        light_full_intensity(selectedColor);
       
        if (digitalRead(BOT) == HIGH) { //  Send pulse
          lamp -> save(420 + otherValue);
          pulse(selectedColor);
        }

        if (currentMillisecond - referenceMillisecond > connectionTime) {state = 8;}

        break;
      case 73: // Established Spin Mode
        currentMillisecond = millis();
        
        while(currentMillisecond - referenceMillisecond < connectionTime){
          currentMillisecond = millis();
          spin(selectedColor);
         
          if (digitalRead(BOT) == HIGH) { //  Send pulse
            lamp -> save(420 + otherValue);
            spinPulse(selectedColor);
          }
        }

        if (currentMillisecond - referenceMillisecond > connectionTime) {state = 8;}

        break;
      case 74: // Establiched Comet Mode
        currentMillisecond = millis();
        
        while(currentMillisecond - referenceMillisecond < connectionTime){
          currentMillisecond = millis();
          comet(selectedColor);
         
          if (digitalRead(BOT) == HIGH) { //  Send pulse
            lamp -> save(420 + otherValue);
            cometPulse(selectedColor);
          }
        }

        if (currentMillisecond - referenceMillisecond > connectionTime) {state = 8;}

        break;
      case 75: // Established Rainbow Mode
        currentMillisecond = millis();
        
        while(currentMillisecond - referenceMillisecond < connectionTime){
          currentMillisecond = millis();
          rainbow(5);
          
          if (digitalRead(BOT) == HIGH) { //  Send pulse
            lamp -> save(420 + otherValue);
            rainbowPulse(5);
          }
        }
        
        if (currentMillisecond - referenceMillisecond > connectionTime) {state = 8;}
        
        break;
      case 76: // Established Disco Mode
        currentMillisecond = millis();
        disco();
       
        if (digitalRead(BOT) == HIGH) { //  Send pulse
          lamp -> save(420 + otherValue);
          discoPulse();
        }
        if (currentMillisecond - referenceMillisecond > connectionTime) {state = 8;}
        
        break;
      case 8: // Reset before state 0
        turn_off();
        lamp -> save(0);
        state = 0;
        break;
      case 9: // Msg received
        sprintf(msg, "L%d: msg received", lampID);
        lamp -> save(msg);
        referenceMillisecond = millis();
        state = 10;
        break;
        
      case 10: // Send answer wait
        heartBeat(0);
        if (digitalRead(BOT) == HIGH) {
          state = 11;
          break;
        }
        currentMillisecond = millis();
        if (currentMillisecond - referenceMillisecond > answerTimeOut) {
          fadeIn(0);
          cancel(); turn_off(); delay(100);
          fadeIn(0);
          cancel(); turn_off(); delay(50);
          sprintf(msg, "L%d: answer time out", lampID);
          lamp -> save(msg);
          state = 8;
          break;
        }
        break;
        
      case 11: // Send answer
        light_full_intensity(selectedColor);
        referenceMillisecond = millis();
        sprintf(msg, "L%d: answer sent", lampID);
        lamp -> save(msg);
        lamp -> save(1);
        switch(selectedMode){
          case 0:
            state = 7;
            break;
          case 1:
            state = 71;
            break;
          case 2:
            state = 72;
            break;
          case 3:
            state = 73;
            break;
          case 4:
            state = 74;
            break;
          case 5:
            state = 75;
            break;
          case 6:
            state = 76;
            break;
          default:
            break;
        }
        break;
      default:
          state = 0;
        break;
      }
      if ((currentWifiMilliseconds - previousWifiMilliseconds >= wifiTimeOut)) {
          if (WiFi.status() != WL_CONNECTED)
            ESP.restart();
          previousWifiMilliseconds = currentWifiMilliseconds;
        }
}

    //code that tells the ESP8266 what to do when it recieves new data from the Adafruit IO feed
    void handle_message(AdafruitIO_Data * data) {

      //convert the recieved data to an INT
      int reading = data -> toInt();
      if (reading == 66) {
        sprintf(msg, "L%d: rebooting", lampID);
        lamp -> save(msg);
        lamp -> save(0);
        ESP.restart();
      } else if (reading == 100) {
        sprintf(msg, "L%d: ping", lampID);
        lamp -> save(msg);
        lamp -> save(0);
      } else if (reading == 420 + myValue) { // Received pulse
        sprintf(msg, "L%d: pulse received", lampID);
        lamp -> save(msg);
        lamp -> save(0);
        switch(selectedMode){
          case 0:
            breathPulse(selectedColor);
            break;
          case 1:
            firePulse(selectedColor);
            break;
          case 2:
            pulse(selectedColor);
            break;
          case 3:
            spinPulse(selectedColor);
            break;
          case 4:
            cometPulse(selectedColor);
            break;
          case 5:
            rainbowPulse(5);
            break;
          case 6:
            discoPulse();
            break;
          default:
            break;
        }
      } else if (reading != 0 && reading / 10 != lampID) { // Message received
        // If it's on standby and receives message
        if (state == 0 && reading != 1) {
          state = 9;
          if (reading < 100) {
            selectedMode = 0;
            selectedColor = reading - myValue;
          }
          else{
            selectedMode = reading / 100;
            selectedColor = reading - (selectedMode*100) - myValue;
          }
        }
        // If it's waiting for an answer
        if (state == 5 && reading == 1)
          state = 6;
      }
    }

    //code to flash the Neopixels when a stable connection to Adafruit IO is made
    void flash(int ind) {
      strip.setBrightness(maxIntensity);
      for (int i = 0; i < N_LEDS; i++) {
        strip.setPixelColor(i, colors[ind]);
      }
      strip.show();
      delay(200);
    }

    void atYourCommand(int ind) {
      int i;
      int i_step = 10;
      for (i = 10; i < maxIntensity; i += i_step) {
        strip.setBrightness(i);
        for (int i = 0; i < N_LEDS; i++) {
          strip.setPixelColor(i, colors[ind]);
          strip.show();
          delay(1);
        }
      }
      delay(20);
      for (i = maxIntensity; i > 10; i -= i_step) {
        strip.setBrightness(i);
        for (int i = 0; i < N_LEDS; i++) {
          strip.setPixelColor(i, colors[1]);
          strip.show();
          delay(1);
        }
      }
    }

    void waitSignalLow(){
      while(digitalRead(BOT) == HIGH){ // Advances only when not pressing anymore
          delay(50);
        }
    }

    void fadeIn(int ind) {
      int i;
      int i_step = 10;
      for (i = 10; i < 125; i += i_step) {
        strip.setBrightness(i);
        for (int i = 0; i < N_LEDS; i++) {
          strip.setPixelColor(i, colors[ind]);
          strip.show();
          delay(1);
        }
      }
      delay(10);
    }

    void fadeOut(int ind) {
      int i;
      int i_step = 10;
      for (i = 125; i > 10; i -= i_step) {
        strip.setBrightness(i);
        for (int i = 0; i < N_LEDS; i++) {
          strip.setPixelColor(i, colors[ind]);
          strip.show();
          delay(1);
        }
      }
      delay(10);
    }

    void turn_off() {
      strip.setBrightness(maxIntensity);
      for (int i = 0; i < N_LEDS; i++) {
        strip.setPixelColor(i, black);
      }
      strip.show();
    }

    void cancel() {
      int i;
      int i_step = 5;
      for (i = 200; i > 10; i -= i_step) {
        strip.setBrightness(i);
        for (int i = 0; i < N_LEDS; i++) {
          strip.setPixelColor(i, colors[0]);
          strip.show();
          delay(1);
        }
      }
      delay(10);
    }

    // 50% intensity
    void light_half_intensity(int ind) {
      strip.setBrightness(maxIntensity / 2);
      for (int i = 0; i < N_LEDS; i++) {
        strip.setPixelColor(i, colors[ind]);
      }
      strip.show();
    }

    // 100% intensity
    void light_full_intensity(int ind) {
      strip.setBrightness(maxIntensity);
      for (int i = 0; i < N_LEDS; i++) {
        strip.setPixelColor(i, colors[ind]);
      }
      strip.show();
    }

    void pulse(int ind) {
      int i;
      int i_step = 5;
      for (i = maxIntensity; i > 10; i -= i_step) {
        strip.setBrightness(i);
        for (int i = 0; i < N_LEDS; i++) {
          strip.setPixelColor(i, colors[ind]);
          strip.show();
          delay(1);
        }
      }
      delay(20);
      for (i = 10; i < maxIntensity; i += i_step) {
        strip.setBrightness(i);
        for (int i = 0; i < N_LEDS; i++) {
          strip.setPixelColor(i, colors[ind]);
          strip.show();
          delay(1);
        }
      }
    }

    // Inspired by Jason Yandell
    void breath(int ind, int i) {
      float MaximumBrightness = maxIntensity / 2;
      float SpeedFactor = 0.04;
      float intensity;
      if (state == 25)
        intensity = MaximumBrightness * (1 + cos(SpeedFactor * i));
      else
        intensity = MaximumBrightness * (1 + sin(SpeedFactor * i));
      strip.setBrightness(intensity);
      for (int ledNumber = 0; ledNumber < N_LEDS; ledNumber++) {
        strip.setPixelColor(ledNumber, colors[ind]);
        strip.show();
        delay(1);
      }
    }

    void breathPulse(int ind){
      int initial_brightness = strip.getBrightness();
      int i;

      for (i = initial_brightness; i > 125; i -= 5) {
        strip.setBrightness(i);
        for (int i = 0; i < N_LEDS; i++) {
          strip.setPixelColor(i, colors[ind]);
          strip.show();
          delay(1);
        }
      }

      int brightness = strip.getBrightness();

      for (i = brightness; i < 255; i += 5) {
        strip.setBrightness(i);
        for (int i = 0; i < N_LEDS; i++) {
          strip.setPixelColor(i, colors[ind]);
          strip.show();
          delay(1);
        }
      }
      
      delay(2500);

      for (i = 255; i > initial_brightness; i -= 5) {
        strip.setBrightness(i);
        for (int i = 0; i < N_LEDS; i++) {
          strip.setPixelColor(i, colors[ind]);
          strip.show();
          delay(1);
        }
      }

    }

    void fire(int ind) {
      uint8_t r = (colors[ind] >> 16) & 0xFF;
      uint8_t g = (colors[ind] >> 8) & 0xFF;
      uint8_t b = colors[ind] & 0xFF;
      for(int i=0; i<strip.numPixels(); i++) {
        int flicker = random(0,200);
        int r1 = r-flicker;
        int g1 = g-flicker;
        int b1 = b-flicker;
        if(g1<0) g1=0;
        if(r1<0) r1=0;
        if(b1<0) b1=0;
        uint32_t c = strip.Color(r1,g1,b1);
        strip.setPixelColor(i,c);
      }
      strip.show();

      //  Adjust the delay here, if you'd like.  Right now, it randomizes the 
      //  color switch delay to give a sense of realism
      delay(random(10,113));
    }

    void firePulse(int ind) {
      int i;
      int i_step = 5;
      for (i = maxIntensity; i > 80; i -= i_step) {
        strip.setBrightness(i);
        fire(ind);
      }
      delay(20);
      for (i = 80; i < maxIntensity; i += i_step) {
        strip.setBrightness(i);
        fire(ind);
      }
    }

    //The code that creates the gradual color change animation in the Neopixels (thank you to Adafruit for this!!)
    void spin(int ind) {
      strip.setBrightness(maxIntensity);
      for (int i = 0; i < N_LEDS; i++) {
        strip.setPixelColor(i, colors[ind]);
        strip.show();
        delay(30);
      }
      for (int i = 0; i < N_LEDS; i++) {
        strip.setPixelColor(i, black);
        strip.show();
        delay(30);
      }
    }

    void spinPulse(int ind) {
      int brightness = maxIntensity;
      strip.setBrightness(brightness);
      for (int i = 0; i < N_LEDS; i++) {
        brightness -= 8;
        strip.setBrightness(brightness);
        strip.setPixelColor(i, colors[ind]);
        strip.show();
        delay(30);
      }
      for (int i = 0; i < N_LEDS; i++) {
        brightness -= 2;
        strip.setBrightness(brightness);
        strip.setPixelColor(i, black);
        strip.show();
        delay(30);
      }
      for (int i = 0; i < N_LEDS; i++) {
        brightness += 5;
        strip.setBrightness(brightness);
        strip.setPixelColor(i, colors[ind]);
        strip.show();
        delay(30);
      }
      for (int i = 0; i < N_LEDS; i++) {
        brightness += 5;
        strip.setBrightness(brightness);
        strip.setPixelColor(i, black);
        strip.show();
        delay(30);
      }
    }

    void comet(int ind){
      uint8_t r = (colors[ind] >> 16) & 0xFF;
      uint8_t g = (colors[ind] >> 8) & 0xFF;
      uint8_t b = colors[ind] & 0xFF;
      float multiplier = 0;
      int cometLength = 14; // length of the comet tail
      int cometSpeed = 20; // speed of the comet animation

      for (int i = 0; i < N_LEDS; i++) {
        turn_off();
        for (int j = 0; j < cometLength; j++) {
          multiplier = pow((float)j/14, 3);
          int r1 = round((float)r*multiplier);
          int g1 = round((float)g*multiplier);
          int b1 = round((float)b*multiplier);
          uint32_t c = strip.Color(r1,g1,b1);
          strip.setPixelColor(((i+j) % N_LEDS), c);
        }
        strip.show(); // update the LEDs
        delay(cometSpeed);
      }
    }

    void cometPulse(int ind){
      uint8_t r = (colors[ind] >> 16) & 0xFF;
      uint8_t g = (colors[ind] >> 8) & 0xFF;
      uint8_t b = colors[ind] & 0xFF;
      float multiplier = 0;
      int cometLength = 14; // length of the comet tail
      int cometSpeed = 20; // speed of the comet animation

      for (int n = 0; n < 6; n++){
        for (int i = 0; i < N_LEDS; i++) {
          turn_off();
          for (int j = 0; j < cometLength-n; j++) {
            multiplier = pow((float)j/14, 3);
            int r1 = round((float)r*multiplier);
            int g1 = round((float)g*multiplier);
            int b1 = round((float)b*multiplier);
            uint32_t c = strip.Color(r1,g1,b1);
            strip.setPixelColor(((i+j) % N_LEDS), c);
          }
          strip.show(); // update the LEDs
          delay(cometSpeed);
        }
      }
      for (int n = 0; n < 6; n++){
        for (int i = 0; i < N_LEDS; i++) {
          turn_off();
          for (int j = 0; j < cometLength-6+n; j++) {
            multiplier = pow((float)j/14, 3);
            int r1 = round((float)r*multiplier);
            int g1 = round((float)g*multiplier);
            int b1 = round((float)b*multiplier);
            uint32_t c = strip.Color(r1,g1,b1);
            strip.setPixelColor(((i+j) % N_LEDS), c);
          }
          strip.show(); // update the LEDs
          delay(cometSpeed);
        }
      }
    }

    void rainbow(int wait) {
      for(long firstPixelHue = 0; firstPixelHue < 3*65536; firstPixelHue += 256) {
        for(int i=0; i<strip.numPixels(); i++) {
          int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
          strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
        }
        strip.show(); // Update strip with new contents
        delay(wait);  // Pause for a moment
      }
    }

    void rainbowPulse(int wait) {
      uint8_t brightness = 255;
      float operation = 255.0;
        for(long firstPixelHue = 0; firstPixelHue < 3*65536; firstPixelHue += 256) {
          for(int i=0; i<strip.numPixels(); i++) {
            int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
            strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
          }
          operation -= 0.33;
          brightness = (uint8_t)round(operation);
          strip.setBrightness(brightness);
          strip.show(); // Update strip with new contents
          delay(wait);  // Pause for a moment
        }
        for(long firstPixelHue = 0; firstPixelHue < 3*65536; firstPixelHue += 256) {
          for(int i=0; i<strip.numPixels(); i++) {
            int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
            strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
          }
          operation += 0.33;
          brightness = (uint8_t)round(operation);
          strip.setBrightness(brightness);
          strip.show(); // Update strip with new contents
          delay(wait);  // Pause for a moment
        }
    }

    void disco() {  
      // all pixels show the same color:
      turn_off();
      int color = rand()%8;
      light_full_intensity(color);
          
      int x = 6;
      for (int ii = 3 ; ii < 252 ; ii = ii + x){
        strip.setBrightness(ii);
        strip.show();              
        delay(3);
        }
      
      x = 6;
      for (int ii = 255 ; ii > 1 ; ii = ii = ii - x){
        strip.setBrightness(ii);
        strip.show();              
        delay(4);  
        }
      delay(10);
    }

    void discoPulse() {
      for(int x = 6; x > 0; x--){
        turn_off();
        int color = rand()%8;
        light_full_intensity(color);
        for (int ii = 255 ; ii > 1 ; ii = ii = ii - x){
          strip.setBrightness(ii);
          strip.show();              
          delay(4);
          }
        for (int ii = 3 ; ii < 252 ; ii = ii + x){
          strip.setBrightness(ii);
          strip.show();              
          delay(3);  
        }
        delay(10);
      }
      for(int x = 1; x < 7; x++){
        turn_off();
        int color = rand()%8;
        light_full_intensity(color);
        for (int ii = 3 ; ii < 252 ; ii = ii + x){
          strip.setBrightness(ii);
          strip.show();              
          delay(3);
          }
        for (int ii = 255 ; ii > 1 ; ii = ii = ii - x){
          strip.setBrightness(ii);
          strip.show();              
          delay(4);  
        }
        delay(10);
      }
    }

    void heartBeat(int ind) {  
      // all pixels show the same color:
      turn_off();
      light_full_intensity(ind);
          
      int x = 6;
      for (int ii = 3 ; ii < 252 ; ii = ii + x){
        strip.setBrightness(ii);
        strip.show();              
        delay(3);
        }
      
      x = 6;
      for (int ii = 255 ; ii > 1 ; ii = ii = ii - x){
        strip.setBrightness(ii);
        strip.show();              
        delay(4);  
        }
      delay(10);
      
      x = 6;
      for (int ii = 1 ; ii < 255 ; ii = ii + x){
        strip.setBrightness(ii);
        strip.show();              
        delay(3);
      } 
      
      x = 3;
      for (int ii = 252 ; ii > 3; ii = ii = ii - x){
        strip.setBrightness(ii);
        strip.show();              
        delay(5);
        }
      delay(400);  
    }

    // Waiting connection led setup
    void wait_connection() {
      strip.setBrightness(maxIntensity);
      for (int i = 0; i < 3; i++) {
        strip.setPixelColor(i, yellow);
      }
      strip.show();
      delay(50);
      for (int i = 3; i < 6; i++) {
        strip.setPixelColor(i, red);
      }
      strip.show();
      delay(50);
      for (int i = 6; i < 9; i++) {
        strip.setPixelColor(i, blue);
      }
      strip.show();
      delay(50);
      for (int i = 9; i < 12; i++) {
        strip.setPixelColor(i, green);
      }
      strip.show();
      delay(50);
    }

    void configModeCallback(WiFiManager * myWiFiManager) {
      Serial.println("Entered config mode");
      wait_connection();
    }

    void wificonfig() {
      WiFi.mode(WIFI_STA);
      WiFiManager wifiManager;
      referenceMillisecond = millis();
      while (digitalRead(BOT) == HIGH) { // Timmer to reset wifi configuration when pluged in
        delay(50);
      }
      currentMillisecond = millis();
      if (currentMillisecond - referenceMillisecond > 5000) {
        wifiManager.resetSettings();// uncomment to forget previous wifi manager settings
      }
      std::vector<const char *> menu = {"wifi","info"};
      wifiManager.setMenu(menu);
      // set dark theme
      wifiManager.setClass("invert");

      bool res;
      wifiManager.setAPCallback(configModeCallback);
      res = wifiManager.autoConnect("Lamp", "password"); // password protected ap

      if (!res) {
        spin(0);
        delay(50);
        turn_off();
      } else {
        //if you get here you have connected to the WiFi
        spin(3);
        delay(50);
        turn_off();
      }
      Serial.println("Ready");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }