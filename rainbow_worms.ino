#include <cmath>
#include <ctgmath>
#include <Adafruit_NeoPixel.h>

#define MIN(x,y) (((x)>(y))?(y):(x))
#define ABS(x) ((x)<0?(-(x)):(x))

#define PIXELS_NUMBER 90
#define PIN 12

Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXELS_NUMBER, PIN, NEO_GRB + NEO_KHZ800);

// IMPORTANT: see a warning about technical best practices on ESP8266 when connecting
// WS2812B controller:
// https://github.com/adafruit/Adafruit_NeoPixel/blob/master/examples/strandtest_wheel/strandtest_wheel.ino

uint8_t fade[] = {255, 160, 100, 60, 30, 10, 4, 1, 0};
uint32_t array[PIXELS_NUMBER];
uint8_t red_array[PIXELS_NUMBER];
uint8_t green_array[PIXELS_NUMBER];
uint8_t blue_array[PIXELS_NUMBER];

uint8_t luminosity(float peek_distance, uint8_t base_value){
    if(!base_value){
        return;
    }
    peek_distance = ABS(peek_distance);
    if(peek_distance < (sizeof(fade)/sizeof(fade[0])-1)){
        float integral_dist, fractional_dist;
        uint32_t index;
        fractional_dist = modf(peek_distance, &integral_dist);
        index = integral_dist;
        return (fade[index]*(1-fractional_dist)+fade[index+1]*fractional_dist)*base_value/255;
    }
    return 0;
}

void add_waive(uint32_t time, float speed, uint8_t red, uint8_t green, uint8_t blue){
  float peek = fmodf(time*speed, PIXELS_NUMBER);
  if(peek < 0.0){
    peek = PIXELS_NUMBER + peek;
  }
  for(uint32_t i = 0; i < PIXELS_NUMBER; ++i){
    red_array[i] = MIN(255, red_array[i]+luminosity(peek-i, red));
    green_array[i] = MIN(255, green_array[i]+luminosity(peek-i, green));
    blue_array[i] = MIN(255, blue_array[i]+luminosity(peek-i, blue));
  }
}

void fill_array(uint32_t time){
  for(uint32_t i = 0; i < PIXELS_NUMBER; ++i){
    red_array[i] = green_array[i] = blue_array[i] = 0;
  }
  add_waive(time, 0.33, 255, 0, 0); // red
  add_waive(time, 0.43, 0, 255, 0); // green
  add_waive(time, 0.13, 0, 0, 255); // blue
  add_waive(time, 0.21, 255, 255, 0); // yellow
  add_waive(time, -0.08, 160, 32, 240); // purple
  add_waive(time, -0.31, 255, 124, 0); // orange
  add_waive(time, -1.0, 176, 0, 40); // cherry
  add_waive(time, -1.01, 176, 0, 40); // cherry
  for(uint32_t i = 0; i < PIXELS_NUMBER; ++i){
    array[i] = strip.Color(red_array[i], green_array[i], blue_array[i]);
  }
}

void setup() {
  strip.begin();
  strip.setBrightness(50);
}

void loop() {
  for(uint32_t time = 0; ; ++time){
    fill_array(time);
    for(uint32_t pixel_number = 0; pixel_number < PIXELS_NUMBER; ++pixel_number)
      strip.setPixelColor(pixel_number, array[pixel_number]);
    strip.show();
    delay(5);
  }
}
