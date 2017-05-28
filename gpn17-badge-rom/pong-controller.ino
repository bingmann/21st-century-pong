#include <ESP8266WiFi.h>
#include <SPI.h>
#include <TFT_ILI9163C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <IRremoteESP8266.h>
#include "url-encode.h"
extern "C" {
#include "user_interface.h"
}
#include <FS.h>
#include "rboot.h"
#include "rboot-api.h"

#include <vector>

#include <Fonts/FreeSans12pt7b.h>

#define BNO055_SAMPLERATE_DELAY_MS (100)

#define VERSION 2

#define USEWIFI
//#define USEIR

#define GPIO_LCD_DC 0
#define GPIO_TX     1
#define GPIO_WS2813 4
#define GPIO_RX     3
#define GPIO_DN     2
#define GPIO_DP     5

#define GPIO_BOOT   16
#define GPIO_MOSI   13
#define GPIO_CLK    14
#define GPIO_LCD_CS 15
#define GPIO_BNO    12

#define MUX_JOY 0
#define MUX_BAT 1
#define MUX_LDR 2
#define MUX_ALK 4
#define MUX_IN1 5

#define VIBRATOR 3
#define MQ3_EN   4
#define LCD_LED  5
#define IR_EN    6
#define OUT1     7

#define UP      790
#define DOWN    630
#define RIGHT   530
#define LEFT    1024
#define OFFSET  30

#define I2C_PCA 0x25

#define NUM_LEDS    4

#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

TFT_ILI9163C tft = TFT_ILI9163C(GPIO_LCD_CS, GPIO_LCD_DC);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_LEDS, GPIO_WS2813, NEO_GRB + NEO_KHZ800);
Adafruit_BNO055 bno = Adafruit_BNO055(BNO055_ID, BNO055_ADDRESS_B);

WiFiClient client;

byte portExpanderConfig = 0; //stores the 74HC595 config

uint16_t color = 0xFFFF;

uint32_t endTime = 0;
uint32_t startTime = 0;

uint8_t joystick = 0; //Jostick register
bool isDirty = 1; //redraw if is dirty
int menu = 1; //menucounter
int screen = 1; //screencounter
int iconcolor = 0xFFFF; //textcolor

// Pong Server Address
IPAddress pong_server;

// Pong Score
uint16_t score = 0;

void setup() {
  initBadge();
  Serial.println("Starting Pong Controller!"); //useful debug message
  tft.setTextSize(1);
  tft.setTextColor(iconcolor);
  rboot_config rboot_config = rboot_get_config();

  // write our app name into the FS
  SPIFFS.begin();
  File f = SPIFFS.open("/rom"+String(rboot_config.current_rom),"w");
  f.println("Pong Controller\n");

  connectBadge();
}

double Gyrostick_getUsableX(double x) {
  if (x >= 180.0) {
    // x geht nach rechts
    // x geht von 360 nach 180 (negativ)
    // 360 = 1, 359 = 2, 358 = 3)
    x = (x - 360) * (-1);
  }
  else { // x < 180.0)
    // x geht nach links
    // x geht von 0 nach 180 (positiv)
    x = x * (-1);
  }

  x = x + 180.0;

  return x;
}

imu::Vector<3> Gyrostick_coords() {
  imu::Vector<3> euler;
  euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);

  double x = Gyrostick_getUsableX(euler.x());
  double y = constrain(euler.y() * 8.0, -180.0, 180.0) + 180.0;
  // holding device in front of you means "centered"
  double z = constrain((euler.z() - 40.0) * 6.0, -180.0, 180.0) + 180.0;

  y = (y / 360.0) * 128.0;
  z = (z / 360.0) * 128.0;

  return imu::Vector<3>(y, z, x);
}

void loop() {

    if (!client.connected()) {
        tft.fillScreen(BLACK);  //make screen black
        tft.setTextSize(1);
        tft.setCursor(0, 0);
        tft.println("connecting to Pong...");
        tft.print(pong_server);
        tft.println(":1234");
        tft.writeFramebuffer();

        if (!client.connect(pong_server, 1234)) {
            tft.println("connection FAILED!");
            tft.writeFramebuffer();
            delay(5000);
            return;
        }
        else {
            tft.println("connection SUCCEEDED!");
            tft.writeFramebuffer();
            delay(50);
        }
    }

  joystick = getJoystick();

  /* Serial.print("joystick: "); */
  /* Serial.println(joystick); */

  // joystick: 0 = no button, 1 = up, 2 = down, 3 = right, 4 = left, 5 = press
  // pixels 0 = up (LED1), 1 = left (LED2), 2 = right, 3 = down.

  imu::Vector<3> jog = Gyrostick_coords();

  float x = jog.x();
  float y = jog.y();
  float z = jog.y();

  pixels.setPixelColor(0, 2 * x, 0, 0);
  pixels.setPixelColor(1, 0, 2 * y, 0);
  pixels.setPixelColor(2, 0, 0, 2 * z);
  pixels.setPixelColor(3, 0, 0, 0);

  pixels.show();

  // write to Pong
  client.write(reinterpret_cast<const char*>(&x), sizeof(x));
  client.write(reinterpret_cast<const char*>(&y), sizeof(y));
  client.write(reinterpret_cast<const char*>(&joystick), sizeof(joystick));

  // read from Pong
  if (client.available() >= 2) {
      client.read(
          reinterpret_cast<uint8_t*>(&score), sizeof(score));

      Serial.println(score);
      setGPIO(VIBRATOR, 1);
      delay(60);
      setGPIO(VIBRATOR, 0);
  }

  if (joystick == 5) {
    menu++;
    menu = menu % 2;
    isDirty = 1;
    int buttonCounter = 0;
    while (getJoystick() == 5) {
      delay(1);
      buttonCounter++;
      if (buttonCounter >= 1000) {
        screen++;
        screen = screen % 2;
        menu = 0;
        setGPIO(VIBRATOR, 1);
        delay(60);
        setGPIO(VIBRATOR, 0);
        while (getJoystick() == 5) delay(0);
      }
    }
  }

  tft.fillScreen(BLACK);  //make screen black

  tft.setTextSize(2);
  tft.setCursor(58, 58);
  tft.print(score);

  tft.drawCircle(x, y, 3, iconcolor);
  tft.writeFramebuffer();
}

int getJoystick() {
  uint16_t adc = analogRead(A0);

  if (adc < UP + OFFSET && adc > UP - OFFSET)             return 1;
  else if (adc < DOWN + OFFSET && adc > DOWN - OFFSET)    return 2;
  else if (adc < RIGHT + OFFSET && adc > RIGHT - OFFSET)  return 3;
  else if (adc < LEFT + OFFSET && adc > LEFT - OFFSET)    return 4;
  if (digitalRead(GPIO_BOOT) == 1) return 5;
  return 0;
}

void setGPIO(byte channel, boolean level) {
  bitWrite(portExpanderConfig, channel, level);
  Wire.beginTransmission(I2C_PCA);
  Wire.write(portExpanderConfig);
  Wire.endTransmission();
}

void setAnalogMUX(byte channel) {
  portExpanderConfig = portExpanderConfig & 0b11111000;
  portExpanderConfig = portExpanderConfig | channel;
  Wire.beginTransmission(I2C_PCA);
  Wire.write(portExpanderConfig);
  Wire.endTransmission();
}

uint16_t getBatLvl() {
  if (portExpanderConfig != 33) {
    setAnalogMUX(MUX_BAT);
    delay(20);
  }
  uint16_t avg = 0;
  for (byte i = 0; i < 10; i++) {
    avg += analogRead(A0);
  }
  return (avg / 10);
}

uint16_t getBatVoltage() { //battery voltage in mV
  return (getBatLvl() * 4.4);
}

uint16_t getLDRLvl() {
  if (portExpanderConfig != 34) {
    setAnalogMUX(MUX_LDR);
    delay(20);
  }
  uint16_t avg = 0;
  for (byte i = 0; i < 10; i++) {
    avg += analogRead(A0);
  }
  return (avg / 10);
}

uint16_t getALKLvl() {
  if (portExpanderConfig != 36) {
    setAnalogMUX(MUX_ALK);
    delay(20);
  }
  uint16_t avg = 0;
  for (byte i = 0; i < 10; i++) {
    avg += analogRead(A0);
  }
  return (avg / 10);
}

void initBadge() { //initialize the badge

  Serial.begin(115200);

#ifdef USEWIFI
  // Next 2 line seem to be needed to connect to wifi after Wake up
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(20);
#endif

  pinMode(GPIO_BOOT, INPUT_PULLDOWN_16);  // settings for the leds
  pinMode(GPIO_WS2813, OUTPUT);

  pixels.begin(); //initialize the WS2813
  pixels.clear();
  pixels.show();

  Wire.begin(9, 10); // Initalize i2c bus
  Wire.beginTransmission(I2C_PCA);
  Wire.write(0b00000000); //...clear the I2C extender to switch off vibrator and backlight
  Wire.endTransmission();

  delay(100);

  tft.begin(); //initialize the tft. This also sets up SPI to 80MHz Mode 0
  tft.setRotation(2); //turn screen
  tft.scroll(32); //move down by 32 pixels (needed)
  tft.fillScreen(BLACK);  //make screen black

  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.print("GPN17 Pong Controller");
  tft.writeFramebuffer();

  setGPIO(LCD_LED, HIGH);

  // initialize BNO
  bno.begin();
  delay(300);

  // clear the WS2813 another time, in case they catched up some noise
  pixels.clear();
  pixels.show();
}

void connectBadge() {
  Serial.println("Connecting to WiFi...");

  tft.setCursor(2, 8);
  tft.println("Connecting to WiFi...");
  tft.writeFramebuffer();

  const char *ssid, *password;

  if (SPIFFS.exists("/wifi.conf")) {
      WiFi.mode(WIFI_STA);
      delay(30);
      Serial.println("Loading config.");

      File wifiConf = SPIFFS.open("/wifi.conf", "r");
      String configString;
      while (wifiConf.available()) {
          configString += char(wifiConf.read());
      }
      wifiConf.close();
      UrlDecode confParse(configString.c_str());
      Serial.println(configString);
      configString = String();
      ssid = confParse.getKey("ssid");
      password = confParse.getKey("pw");
  }
  else {
      ssid = "pong-ap";
      password = "";
  }

  tft.setCursor(2, 16);
  tft.print("SSID ");
  tft.println(ssid);
  tft.writeFramebuffer();

  Serial.printf("Connecting to wifi '%s' with password '%s'...\n", ssid, password);
  unsigned long startTime = millis();
  WiFi.begin(ssid, password);

  uint8_t cnt = 0;
  tft.print(".");
  tft.writeFramebuffer();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    cnt++;
    if (cnt % 4 == 0) {
        tft.print(".");
        tft.writeFramebuffer();
    }
    delay(250);
  }

  tft.println("");
  tft.println("WiFi connected!");
  tft.print("IP ");
  tft.println(WiFi.localIP());
  tft.writeFramebuffer();

  // connect to Pong server in local Wifi at address x.x.x.42 port 1234 !
  pong_server = WiFi.localIP();
  pong_server[3] = 42;
}
