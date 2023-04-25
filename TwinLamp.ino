//import the libraries

#include "config.h"
#include <Adafruit_NeoPixel.h>
#include <WiFiManager.h>

#define N_LEDS 24
#define LED_PIN 5
#define BOT 12 // capacitive sensor pin

//////////////////LAMP ID////////////////////////////////////////////////////////////
int lampID = 1;
/////////////////////////////////////////////////////////////////////////////////////

Adafruit_NeoPixel strip(N_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Adafruit inicialization
AdafruitIO_Feed * lamp = io.feed("Lamp"); // Change to your feed

int recVal  {0};
int sendVal {0};

const int max_intensity = 255; //  Max intensity

int selected_color = 0;  //  Index for color vector
int selected_mode = 0; // Index for mode

int i_breath;

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

int state = 0;

// Time vars
unsigned long RefMillis;
unsigned long ActMillis;
const int send_selected_color_time = 3000;
const int answer_time_out          = 900000;
const int on_time                  = 900000;

// Disconection timeout
unsigned long currentMillis;
unsigned long previousMillis = 0;
const unsigned long conection_time_out = 600000; // 5 minutos

// Long press detection
const int long_press_time = 2000;
int lastState = LOW;  // the previous state from the input pin
int currentState;     // the current reading from the input pin
unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;

void setup() {
  Serial.begin(115200); //Start the serial monitor for debugging and status
  strip.begin(); // Activate neopixels
  strip.show(); // Initialize all pixels to 'off'
  wificonfig(); // Connect to WiFi
  pinMode(BOT, INPUT); // Declares button

  //  Set ID values
  if (lampID == 1) {
    recVal = 20;
    sendVal = 10;
  }
  else if (lampID == 2) {
    recVal = 10;
    sendVal = 20;
  }

  //start connecting to Adafruit IO
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
  // Animation
  rainbow(5);  turn_off(); delay(50);
  flash(1); turn_off(); delay(100);
  flash(1); turn_off(); delay(50);

  //get the status of our value in Adafruit IO
  lamp -> get();
  sprintf(msg, "L%d: connected", lampID);
  lamp -> save(msg);
}

void loop() {
    currentMillis = millis();
    io.run();
    // State machine
    switch (state) {

      case 0: // Wait for color/mode pair or message
        currentState = digitalRead(BOT);
        if(lastState == LOW && currentState == HIGH)  // Button is pressed
        {
          pressedTime = millis();
        }
        else if(currentState == HIGH) {
          releasedTime = millis();
          long pressDuration = releasedTime - pressedTime;
          if( pressDuration > long_press_time )
          {
              state = 1;
          }
        }
        lastState = currentState;
        break;
        
      case 1: // Start color selection
        selected_color = 0;
        light_half_intensity(selected_color);
        state = 2;
        RefMillis = millis();
        while(digitalRead(BOT) == HIGH){
          delay(50);
        }
        break;
        
      case 2: // Color selection
        if (digitalRead(BOT) == HIGH) {
          selected_color++;
          if (selected_color > 9)
            selected_color = 0;
          while (digitalRead(BOT) == HIGH) {
            delay(50);
          }
          light_half_intensity(selected_color);
          // Reset timer each time it is touched
          RefMillis = millis();
        }
        // If a color is selected more than a time, it is interpreted as the one selected
        ActMillis = millis();
        if (ActMillis - RefMillis > send_selected_color_time) {
          if (selected_color == 9) //  Cancel msg
            state = 8;
          else
            state = 25;
            selected_mode = 0;
            RefMillis = millis();
        }
        break;
        
      case 25: // Select mode
        if (selected_mode == 0){
          pulse(selected_color);
          ActMillis = millis();
          if (digitalRead(BOT) == HIGH) {
              turn_off();
              state = 3;
              break;
          } else if (ActMillis - RefMillis > 3000){
            selected_mode = 1;
            RefMillis = millis();
          }
          break;
        }
        if (selected_mode = 1){
          fire(selected_color);
          ActMillis = millis();
          if (digitalRead(BOT) == HIGH) {
              turn_off();
              state = 3;
              break;
          } else if (ActMillis - RefMillis > 3000){
            selected_mode = 0;
            RefMillis = millis();
          }
        }
        break;
        
      case 3: // Publish message
        sprintf(msg, "L%d: color send", lampID);
        lamp -> save(msg);
        lamp -> save( (selected_mode*100) + selected_color + sendVal);
        Serial.print( (selected_mode*100) + selected_color + sendVal);
        state = 4;
        flash(selected_color);
        light_half_intensity(selected_color);
        delay(100);
        flash(selected_color);
        light_half_intensity(selected_color);
        delay(500);
        break;
        
      case 4: // Set timer
        RefMillis = millis();
        state = 5;
        i_breath = 0;
        break;
        
      case 5: // Wait for answer
        heartBeat(0);          
        ActMillis = millis();
        if (ActMillis - RefMillis > answer_time_out) {
          turn_off();
          flash(0); turn_off(); delay(100);
          flash(0); turn_off(); delay(50);
          lamp -> save("L%d: Answer time out", lampID);
          lamp -> save(0);
          state = 8;
          break;
        }
        break;
        
      case 6: // Answer received
        light_full_intensity(selected_color);
        RefMillis = millis();
        sprintf(msg, "L%d: connected", lampID);
        lamp -> save(msg);
        lamp -> save(0);
        if (selected_mode == 0){
          state = 7;
        }
        else{
          state = 75;
        }
        break;
        
      case 7: // Turned on
        // aumentar tempo de toque        
        ActMillis = millis();
        //  Send pulse
        if (digitalRead(BOT) == HIGH) {
          lamp -> save(420 + sendVal);
          pulse(selected_color);
        }
        if (ActMillis - RefMillis > on_time) {
          turn_off();
          lamp -> save(0);
          state = 8;
        }
        break;
        
      case 75:
        ActMillis = millis();
        fire(selected_color);
        //  Send pulse
        if (digitalRead(BOT) == HIGH) {
          lamp -> save(420 + sendVal);
          pulseFire(selected_color);
        }
        if (ActMillis - RefMillis > on_time) {
          turn_off();
          lamp -> save(0);
          state = 8;
        }
        break;

      case 8: // Reset before state 0
        turn_off();
        state = 0;
        break;
        
      case 9: // Msg received
        sprintf(msg, "L%d: msg received", lampID);
        lamp -> save(msg);
        RefMillis = millis();
        state = 10;
        break;
        
      case 10: // Send answer wait
        heartBeat(0);
        if (digitalRead(BOT) == HIGH) {
          state = 11;
          break;
        }
        ActMillis = millis();
        if (ActMillis - RefMillis > answer_time_out) {
          turn_off();
          flash(0); turn_off(); delay(100);
          flash(0); turn_off(); delay(50);
          sprintf(msg, "L%d: answer time out", lampID);
          lamp -> save(msg);
          state = 8;
          break;
        }
        break;
        
      case 11: // Send answer
        light_full_intensity(selected_color);
        RefMillis = millis();
        sprintf(msg, "L%d: answer sent", lampID);
        lamp -> save(msg);
        lamp -> save(1);
        if (selected_mode == 0){
          state = 7;
        }
        else{
          state = 75;
        }
        break;
      default:
          state = 0;
        break;
      }
      if ((currentMillis - previousMillis >= conection_time_out)) {
          if (WiFi.status() != WL_CONNECTED)
            ESP.restart();
          previousMillis = currentMillis;
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
      } else if (reading == 420 + recVal) {
        sprintf(msg, "L%d: pulse received", lampID);
        lamp -> save(msg);
        lamp -> save(0);
        if (selected_mode == 0){
          pulse(selected_color);
        }
        else{
          pulseFire(selected_color);
        }
      } else if (reading != 0 && reading / 10 != lampID) {
        // Is it a color msg?
        if (state == 0 && reading != 1) {
          state = 9;
          if (reading < 100) {
            selected_mode = 0;
            selected_color = reading - recVal;
          }
          else{
            selected_mode = 1;
            selected_color = reading - 100 - recVal;
          }
        }
        // Is it an answer?
        if (state == 5 && reading == 1)
          state = 6;
      }
    }

    void turn_off() {
      strip.setBrightness(max_intensity);
      for (int i = 0; i < N_LEDS; i++) {
        strip.setPixelColor(i, black);
      }
      strip.show();
    }

    // 50% intensity
    void light_half_intensity(int ind) {
      strip.setBrightness(max_intensity / 2);
      for (int i = 0; i < N_LEDS; i++) {
        strip.setPixelColor(i, colors[ind]);
      }
      strip.show();
    }

    // 100% intensity
    void light_full_intensity(int ind) {
      strip.setBrightness(max_intensity);
      for (int i = 0; i < N_LEDS; i++) {
        strip.setPixelColor(i, colors[ind]);
      }
      strip.show();
    }

    void pulse(int ind) {
      int i;
      int i_step = 5;
      for (i = max_intensity; i > 80; i -= i_step) {
        strip.setBrightness(i);
        for (int i = 0; i < N_LEDS; i++) {
          strip.setPixelColor(i, colors[ind]);
          strip.show();
          delay(1);
        }
      }
      delay(20);
      for (i = 80; i < max_intensity; i += i_step) {
        strip.setBrightness(i);
        for (int i = 0; i < N_LEDS; i++) {
          strip.setPixelColor(i, colors[ind]);
          strip.show();
          delay(1);
        }
      }
    }

    void pulseFire(int ind) {
      int i;
      int i_step = 5;
      for (i = max_intensity; i > 80; i -= i_step) {
        strip.setBrightness(i);
        fire(ind);
      }
      delay(20);
      for (i = 80; i < max_intensity; i += i_step) {
        strip.setBrightness(i);
        fire(ind);
      }
    }

    //The code that creates the gradual color change animation in the Neopixels (thank you to Adafruit for this!!)
    void spin(int ind) {
      strip.setBrightness(max_intensity);
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

    // Inspired by Jason Yandell
    void breath(int ind, int i) {
      float MaximumBrightness = max_intensity / 2;
      float SpeedFactor = 0.02;
      float intensity;
      if (state == 25)
        intensity = MaximumBrightness / 2.0 * (1 + cos(SpeedFactor * i));
      else
        intensity = MaximumBrightness / 2.0 * (1 + sin(SpeedFactor * i));
      strip.setBrightness(intensity);
      for (int ledNumber = 0; ledNumber < N_LEDS; ledNumber++) {
        strip.setPixelColor(ledNumber, colors[ind]);
        strip.show();
        delay(1);
      }
    }

    //code to flash the Neopixels when a stable connection to Adafruit IO is made
    void flash(int ind) {
      strip.setBrightness(max_intensity);
      for (int i = 0; i < N_LEDS; i++) {
        strip.setPixelColor(i, colors[ind]);
      }
      strip.show();
      delay(200);
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

    void heartBeat(int ind) {  
      // all pixels show the same color:
      turn_off();
      light_full_intensity(0);
          
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
          strip.setBrightness(max_intensity);
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
      RefMillis = millis();
      while (digitalRead(BOT) == HIGH) {
        delay(50);
      }
      ActMillis = millis();
      if (ActMillis - RefMillis > 5000) {
        wifiManager.resetSettings();// uncomment to forget previous wifi manager settings
      }
      std::vector<const char *> menu = {"wifi","info"};
      wifiManager.setMenu(menu);
      // set dark theme
      wifiManager.setClass("invert");

      bool res;
      wifiManager.setAPCallback(configModeCallback);
      res = wifiManager.autoConnect("TwinLamp", "password"); // password protected ap

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