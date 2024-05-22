#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SensirionI2cSht4x.h>
#include <Wire.h>
#include "time.h"

/* TODOs:
Check if i2c lp is easy to use?
Use cert to authentify device to broker
Check how to configure wifi easily?
Handle error cases
Show errors on leds?

*/

// make sure that we use the proper definition of NO_ERROR
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

//Definition of pins number
const uint8_t I2C_SDA = 19;
const uint8_t I2C_SCL = 20;

const int LedPin = 15;

extern const char* test_root_ca;
extern const char* test_client_key;
extern const char* test_client_cert;

const char *WIFI_SSID = "ROOMZ";    //WIFI Name
const char *WIFI_PASSWORD = "12345678"; //WIFI Password 

const char MQTT_BROKER_ADRRESS[] = "test.mosquitto.org"; 
const int MQTT_PORT = 8883;
const char MQTT_CLIENT_ID[] = "ecoduboiron-arduino-001"; 

const int MQTT_TIMEOUT = 30;
const int WIFI_TIMEOUT = 10;


// The MQTT topics that Arduino should publish/subscribe
const char PUBLISH_TOPIC[] = "ecoduboiron-arduino-001/sensors-values";  
const char SUBSCRIBE_TOPIC[] = "ecoduboiron-arduino-001/sensors-command";

// Interval of time between actions
const int MEASURE_INTERVAL = 5;  // in seconds
const int PUBLISH_INTERVAL = 20;  // in seconds

const char* ntpServer = "ch.pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

//Structure used to keep measurements
typedef struct{
  unsigned int timestamp;
  float temperature;
  float humidity;
} measurement_t;

// 16KB of memory is available and kept during hibernate
RTC_DATA_ATTR measurement_t measures[1000];
RTC_DATA_ATTR unsigned int measure_count=0;


WiFiClientSecure  espClient;
PubSubClient client(espClient);


static char errorMessage[64];
static int16_t error;

RTC_DATA_ATTR unsigned long lastPublishTime = 0;
RTC_DATA_ATTR unsigned long lastMeasureTime = 0;

//Instand handling the temperature sensor
SensirionI2cSht4x sensor;

extern void wifi_scan();
extern void printLocalTime();


void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void blinkLed(int number, int delayMs)
{

  Serial.print("Blinking led: ");
  Serial.println(number);

  for (int i=0; i<number; i++)
  {
    digitalWrite(LedPin, HIGH);
    delay(delayMs);
    digitalWrite(LedPin, LOW);
    if (i < number)
      delay(delayMs);
  } 
}


void setup()
{
  esp_sleep_wakeup_cause_t wakeup_reason;

  pinMode(LedPin, OUTPUT);

  Serial.begin(115200);
  Serial.println();

  i2c_init();

  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  int64_t time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
  Serial.print("Time:");
  Serial.println(tv_now.tv_sec);

  print_wakeup_reason();

  wakeup_reason = esp_sleep_get_wakeup_cause();

  blinkLed(1, 600);

  if (tv_now.tv_sec - lastMeasureTime > MEASURE_INTERVAL) 
  {
    measurement_t *measure = &measures[measure_count++];

    Serial.println("Measure temperature");
    error = sensor.measureLowestPrecision(measure->temperature, measure->humidity);
    if (error != NO_ERROR) 
    {
        Serial.print("Error trying to execute measureLowestPrecision(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
    }
    else
    {
      measure->timestamp = tv_now.tv_sec;
      Serial.print("Measure: ");
      Serial.println(measure_count);
      Serial.print("Temperature: ");
      Serial.print(measure->temperature);
      Serial.print("\t");
      Serial.print("Humidity: ");
      Serial.println(measure->humidity);

    }
    lastMeasureTime = tv_now.tv_sec;
  }


  if (tv_now.tv_sec - lastPublishTime > PUBLISH_INTERVAL || wakeup_reason == 0) 
  {
    int timeout_count=0;  
    //wifi_scan();

    Serial.print("Arduino - Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);


    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println(WiFi.status());
        timeout_count++;
        
        if (timeout_count >= WIFI_TIMEOUT)
        {
          Serial.println("Timeout wifi");
          blinkLed(2, 300);
          goto hibernate;
        }
    }
    //TODO: handle timeout

    //digitalWrite(LedPin, HIGH);
    //Serial.println("Toggle!");


    Serial.println("WiFi connected!");

    // print your board's IP address:
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    //This is the first power on I suppose
    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED)
    {
      Serial.println("Syncing time");
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      printLocalTime();
    }
  

    Serial.println("Arduino - Connecting to MQTT broker");

    espClient.setCACert(test_root_ca);
    espClient.setCertificate(test_client_cert); // for client verification
    espClient.setPrivateKey(test_client_key);  // for client verification

    client.setServer(MQTT_BROKER_ADRRESS, MQTT_PORT);
    
    client.setCallback(callback);

    timeout_count = 0;
    
    // Loop until we're reconnected
    while (!client.connected()) 
    {
      Serial.print("Attempting MQTT connection...");
      // Attempt to connect
      if (client.connect(MQTT_CLIENT_ID)) 
      {
        Serial.println("connected");
      } 
      else 
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());

        timeout_count++;        

        if (timeout_count >= MQTT_TIMEOUT)
        {
          Serial.println("Timeout mqtt");
          blinkLed(4, 300);
          goto hibernate;
        }

        Serial.println(" try again in 1 seconds");
        // Wait 5 seconds before retrying
        delay(1000);
      }
    }


    Serial.print("Arduino - Subscribe to the topic: ");
    client.subscribe(SUBSCRIBE_TOPIC);


    Serial.println("Arduino - MQTT broker Connected!");

    client.loop(); //TODO: Is it needed
    
    Serial.print("Send measures: ");
    Serial.println(measure_count);

    for(int i=0; i< measure_count; i++)
    {
      sendToMQTT(measures[i]);
    }

    //TODO: Handle failure to send
    measure_count = 0;
    lastPublishTime = tv_now.tv_sec;

  }

hibernate:

  blinkLed(2, 600);

  Serial.print("Sleeping for seconds: ");
  Serial.println(MEASURE_INTERVAL);

  esp_sleep_enable_timer_wakeup(MEASURE_INTERVAL * 1000 * 1000);
 
  esp_deep_sleep_start();

}


// Initialize i2c driver and pins
void i2c_init()
{

  //Activate internal pull up resistors (not sure this is kept when initializing the i2c though)
  pinMode (I2C_SDA, INPUT_PULLUP); 
  pinMode (I2C_SCL, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  sensor.begin(Wire, SHT40_I2C_ADDR_44);

  sensor.softReset();
  delay(10);
  uint32_t serialNumber = 0;
  error = sensor.serialNumber(serialNumber);
  if (error != NO_ERROR) {
      Serial.print("Error trying to execute serialNumber(): ");
      errorToString(error, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
      return;
  }
  Serial.print("serialNumber: ");
  Serial.print(serialNumber);
  Serial.println();
}

void callback(char* topic, byte* message, unsigned int length) 
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
}

void sendToMQTT(measurement_t measure) 
{
  StaticJsonDocument<200> message;
  Serial.println("Sending measure");
  Serial.println(measure.timestamp);
  message["timestamp"] = measure.timestamp;

  JsonArray data = message.createNestedArray("data");
  data.add(measure.temperature);
  data.add(measure.humidity);

  char messageBuffer[512];
  serializeJson(message, messageBuffer);

  client.publish(PUBLISH_TOPIC, messageBuffer);

  Serial.println("Arduino - sent to MQTT:");
  Serial.print("- topic: ");
  Serial.println(PUBLISH_TOPIC);
  Serial.print("- payload:");
  Serial.println(messageBuffer);
}


void loop() 
{
  //Nothing is done here as it restarts from startup upon deep sleep exit

}