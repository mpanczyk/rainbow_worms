#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <cmath>
#include <ctgmath>
#include <Adafruit_NeoPixel.h>

#include "credentials.h"

#define MIN(x,y) (((x)>(y))?(y):(x))
#define ABS(x) ((x)<0?(-(x)):(x))

#define MQTT_PUB_DELAY "door/delay"
#define MQTT_PUB_FUNCTION "door/function_number"


AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;


#define TAIL_LENGTH 7
uint32_t color_tail_coords[TAIL_LENGTH] = {0};
uint8_t color_tail_colors[TAIL_LENGTH] = {255, 128, 44, 12, 10, 2, 1};
uint32_t tail_pointer = 4;
uint8_t function_number = 0;

uint8_t fade[] = {255, 160, 100, 60, 30, 10, 4, 1, 0};
uint32_t array[PIXELS_NUMBER];
uint8_t red_array[PIXELS_NUMBER];
uint8_t green_array[PIXELS_NUMBER];
uint8_t blue_array[PIXELS_NUMBER];
uint32_t rainbow_worms_time;
uint32_t loop_delay;

#define WIDTH 12
#define HEIGHT 7

#define WIDTH_1 (WIDTH-1)
#define HEIGHT_1 (HEIGHT-1)
uint32_t coords_mapping[HEIGHT][WIDTH] = {
    {72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83},
    {71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60},
    {48, 49, 50, 51, 52, 53, 53, 55, 56, 57, 58, 59},
    {47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36},
    {24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35},
    {23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}
};

uint32_t my_abs(int32_t value){
  if(value < 0)
    return -value;
  return value;
}

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  mqttClient.subscribe(MQTT_PUB_DELAY, 1);
  mqttClient.subscribe(MQTT_PUB_FUNCTION, 1);
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttMessage(
    char* topic,
    char* payload,
    AsyncMqttClientMessageProperties properties,
    size_t len,
    size_t index,
    size_t total
) {
    Serial.printf("message - topic: %s, payload: %s", topic, payload);
    if(!strcmp(topic, MQTT_PUB_DELAY)){
        sscanf(payload, "%d", &loop_delay);
    } else if(!strcmp(topic, MQTT_PUB_FUNCTION)){
        sscanf(payload, "%d", &function_number);
    }
}


Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXELS_NUMBER, PIN, NEO_GRB + NEO_KHZ800);

// IMPORTANT: see a warning about technical best practices on ESP8266 when connecting
// WS2812B controller:
// https://github.com/adafruit/Adafruit_NeoPixel/blob/master/examples/strandtest_wheel/strandtest_wheel.ino

uint8_t luminosity(float peek_distance, uint8_t base_value){
    if(!base_value){
        return 0;
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
  add_waive(time, 0.033, 255, 0, 0); // red
  add_waive(time, 0.143, 0, 255, 0); // green
  add_waive(time, 0.13, 0, 0, 255); // blue
  //add_waive(time, 0.21, 255, 255, 0); // yellow
  add_waive(time, -0.08, 160, 32, 240); // purple
  //add_waive(time, -0.31, 255, 124, 0); // orange
  //add_waive(time, -1.0, 176, 0, 40); // cherry
  //add_waive(time, -1.01, 176, 0, 40); // cherry
  for(uint32_t i = 0; i < PIXELS_NUMBER; ++i){
    array[i] = strip.Color(red_array[i], green_array[i], blue_array[i]);
  }
}


void rainbow_worms(){
  fill_array(rainbow_worms_time);
  for(uint32_t pixel_number = 0; pixel_number < PIXELS_NUMBER; ++pixel_number)
    strip.setPixelColor(pixel_number, array[pixel_number]);
  strip.show();
  ++rainbow_worms_time;
}

void setup() {
  loop_delay = 5000;
  rainbow_worms_time = 0;
  strip.begin();
  strip.setBrightness(50);

  Serial.begin(115200);
  Serial.println();

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  //mqttClient.onSubscribe(onMqttSubscribe);
  //mqttClient.onUnsubscribe(onMqttUnsubscribe);
  //mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCredentials(MQTT_USER, MQTT_PASSWORD);
  connectToWifi();
}

void ping(){
  uint32_t x, y;
  tail_pointer = (2*TAIL_LENGTH+tail_pointer-1) % TAIL_LENGTH;
  strip.setPixelColor(color_tail_coords[tail_pointer], 0, 0, 0);

  x = my_abs(WIDTH_1-(int32_t)((int32_t)(1.9*rainbow_worms_time)%(2*WIDTH_1)));
  y = my_abs(HEIGHT_1-(int32_t)(rainbow_worms_time%(2*HEIGHT_1)));
  color_tail_coords[tail_pointer] = coords_mapping[y][x];

  for(uint8_t i = 0; i<TAIL_LENGTH; ++i){
      strip.setPixelColor(color_tail_coords[(i+tail_pointer)%TAIL_LENGTH], color_tail_colors[i], 0, 0);
  }
  ++rainbow_worms_time;
  strip.show();
}

void off(){
    strip.clear();
    strip.show();
    delay(10);
}

typedef void (*func)();
func functions[] = {
    &off,
    &ping,
    &rainbow_worms
};

void loop() {
  delay(loop_delay);
  functions[function_number%(sizeof(functions)/sizeof(functions[0]))]();
}
