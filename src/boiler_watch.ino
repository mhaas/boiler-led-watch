#include "wlan_secret.h"

#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define MQTT_HOST "mqtt"

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_101MS, TCS34725_GAIN_1X );
WiFiClient wifiClient;
PubSubClient client(wifiClient, MQTT_HOST);


enum indicator_colors_t {GREEN, AMBER, RED, DARK, UNKNOWN};
const indicator_colors_t COLORS[] {GREEN, AMBER, RED, DARK, UNKNOWN};

const int measurement_interval_ms = 10 * 1000;
const int sampling_interval_ms = 101;
const int samples_per_measurement = 10;
const int measurement_duration = samples_per_measurement * sampling_interval_ms;

int color_frequency_per_measurement[5];

int lastSample = 0;
int numberOfSamples = 0;

struct light_sensor_reading_t  {
  uint16_t r;
  uint16_t g;
  uint16_t b;
  uint16_t c;

};

struct measurement_interpretation_t {
  bool is_valid;
  int dark_duty_cycle;
  indicator_colors_t other_color;
};

void connectToWifi() {


  if (WiFi.status() != WL_CONNECTED) {
    delay(10);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(200);
      delay(50);

    }
  }
  
  if (client.connect("boiler-watch")) {
    Serial.println("Connected to MQTT server");
  } else {
    Serial.println("Could not connect to MQTT server!");
  }

}

void setup(void) {
  Serial.begin(9600);
  connectToWifi();
  if (tcs.begin()) {
    Serial.println("Found sensor");
  } else {
    Serial.println("No TCS34725 found ... check your connections");
    while (1);
  }
}

bool canBeginMeasurement() {
  if (lastSample + measurement_interval_ms <= millis()) {
    return true;
  }
  return false;
}

void beginMeasurement() {
  numberOfSamples = 0;
  color_frequency_per_measurement[GREEN] = 0;
  color_frequency_per_measurement[AMBER] = 0;
  color_frequency_per_measurement[RED] = 0;
  color_frequency_per_measurement[DARK] = 0;
  color_frequency_per_measurement[UNKNOWN] = 0;

}

bool isMeasurementDone() {
  if (numberOfSamples == samples_per_measurement) {
    return true;
  }
  return false;
}

bool isMeasurementInProgress()  {
  return numberOfSamples < samples_per_measurement;
}

bool canRecordNewSample() {
  if (lastSample + sampling_interval_ms <= millis()) {
    return true;
  }
  return false;
}


void recordSample() {
  indicator_colors_t color = readIndicatorColor();
  color_frequency_per_measurement[color]++;
  lastSample = millis();
  numberOfSamples++;
}

measurement_interpretation_t interpretMeasurement()  {
  // We care about one thing: the duty cycle of the LED. Per measurement, we allow
  // only one color + DARK. Multiple colors are not allowed, only DARK is allowed.

  int dark_frequency = color_frequency_per_measurement[DARK];
  indicator_colors_t observed_color = UNKNOWN;

  measurement_interpretation_t interpretation;


  for (int i = 0; i < sizeof(COLORS) / sizeof(indicator_colors_t); i++) {
    indicator_colors_t color = COLORS[i];
    if (color == DARK) {
      continue;
    }
    int color_frequency = color_frequency_per_measurement[color];
    if (color_frequency > 0) {
      observed_color = color;
    }
  }

  // We only allow at most one color other than dark
  // The check below even works if only dark was observed, since the frequency for UNKNOWN will then be 0
  if (dark_frequency + color_frequency_per_measurement[observed_color] != samples_per_measurement) {
    interpretation.is_valid = false;
  } else if (color_frequency_per_measurement[UNKNOWN] > 0) {
    interpretation.is_valid = false;
  } else {
    interpretation.is_valid = true;
    interpretation.other_color = observed_color;
    interpretation.dark_duty_cycle = (dark_frequency * 100) / samples_per_measurement;
  }

  return interpretation;


}

enum indicator_colors_t readIndicatorColor() {
  light_sensor_reading_t reading = readRawLightSensor();
  //printSensorReading(reading);
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

void publishMeasurementInterpretation(measurement_interpretation_t interpretation) {



  DynamicJsonBuffer  jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  root["valid"] = interpretation.is_valid;

  if (interpretation.is_valid) {
    root["dark_duty_cycle"] = interpretation.dark_duty_cycle;
    root["color"] = convertColorToString(interpretation.other_color);
  }
  
  char json[1024];
  root.printTo(json, 1024);

  Serial.println("Publishing result!");
  Serial.println(json);
  client.publish("/reijoo2Ooquaibah/", json);

}

void loop(void) {
  if (! isMeasurementInProgress() && canBeginMeasurement()) {
    Serial.println("Measurement not in progress. Beginning one!");
    beginMeasurement();
  }

  if (isMeasurementInProgress()) {
    if (canRecordNewSample()) {
      Serial.println("Can record new sample. Recording one!");
      recordSample();
    }
    if (isMeasurementDone()) {
      Serial.println("Measurement done. Interpreting it!");
      // TODO: Process and reset
      measurement_interpretation_t interpretation = interpretMeasurement();
      publishMeasurementInterpretation(interpretation);
    }
  }


}
