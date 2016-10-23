#include <Wire.h>
#include "Adafruit_TCS34725.h"


Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_101MS, TCS34725_GAIN_1X );

enum indicator_colors_t {GREEN, AMBER, RED, DARK, UNKNOWN};

struct light_sensor_reading_t  {
  uint16_t r;
  uint16_t g;
  uint16_t b;
  uint16_t c;

};

void setup(void) {
  Serial.begin(9600);

  if (tcs.begin()) {
    Serial.println("Found sensor");
  } else {
    Serial.println("No TCS34725 found ... check your connections");
    while (1);
  }

  // Now we're ready to get readings!
}



enum indicator_colors_t readIndicatorColor() {
  light_sensor_reading_t reading = readRawLightSensor();
  printSensorReading(reading);
  indicator_colors_t value = convertSensorReadingToColor(reading);
  return value;
}

enum indicator_colors_t convertSensorReadingToColor(light_sensor_reading_t reading) {
  // We need to distinguish between RED, GREEN and AMBER.
  // For the sake of this limited comparison, we only look at the red and
  // green values provided by the sensor.
  // If the red and green values are approximately balanced, we detect the color as AMBER.
  // Else, either RED or GREEN wins.
  // If the clear value is below 100, we assume the LED is off and DARK.

  // Here are the values taken from my smartphone display with a colorized screen and 
  // Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_101MS, TCS34725_GAIN_1X );

  // Red screen: R: 75 G: 18 B: 17 C: 105
  // Green screen: R: 54 G: 112 B: 41 C: 211
  // Amber screen: R: 79 G: 74 B: 37 C: 191

  // TODO: convert to relative delta
  const int amber_delta = 20;
  const int minimum_value = 50;
 
  if (reading.c < 100) {
    return DARK;
  }
  
  // If blue dominates, we do not want to match anything
  if (reading.b > reading.r && reading.b > reading.g) {
    return UNKNOWN;
  }
  if (reading.r < minimum_value && reading.g < minimum_value) {
    return UNKNOWN;
  }
  const int red_green_difference = abs(reading.r - reading.g);
  if (red_green_difference < amber_delta) {
    return AMBER;
  }
  if (reading.r > reading.g) {
    return RED;
  } else {
    return GREEN;
  }
}

void printSensorReading(light_sensor_reading_t reading) {
  Serial.print("R: "); Serial.print(reading.r, DEC); Serial.print(" ");
  Serial.print("G: "); Serial.print(reading.g, DEC); Serial.print(" ");
  Serial.print("B: "); Serial.print(reading.b, DEC); Serial.print(" ");
  Serial.print("C: "); Serial.print(reading.c, DEC); Serial.print(" ");
  Serial.println(" ");

}

String convertColorToString (indicator_colors_t color) {
  if (color == GREEN) {
    return String("green");
  } else if (color == RED) {
    return String("red");
  } else if (color == AMBER) {
    return String("amber");
  } else if (color == DARK) {
    return String("dark");
  } else if (color == UNKNOWN) {
    return String("unknown");
  } else {
    return String("invalid color code provided!");
  }
}  

light_sensor_reading_t readRawLightSensor() {

  uint16_t r, g, b, c;

  tcs.getRawData(&r, &g, &b, &c);

  light_sensor_reading_t reading;
  reading.r = r;
  reading.g = g;
  reading.b = b;
  reading.c = c;
  return reading;
}


void loop(void) {
  indicator_colors_t color = readIndicatorColor();
  String display_value = convertColorToString(color);
  Serial.println(display_value);
  delay(500);
  
}
