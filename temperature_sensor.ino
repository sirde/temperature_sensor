#include <WiFi.h>
#include <WiFiClient.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <SensirionI2cSht4x.h>
#include <Wire.h>

/* TODOs:
read i2c sensor, preferrably with i2c lp
Use TLS to encrypt mqtt connection
Use cert to authentify device to broker
Check how to configure wifi easily?
Make it connect to wifi a lot faster
Handle error cases
Show errors on leds?

*/


const char *WIFI_SSID = "ROOMZ";    //WIFI Name
const char *WIFI_PASSWORD = "12345678"; //WIFI Password 

const char MQTT_BROKER_ADRRESS[] = "test.mosquitto.org"; 
const int MQTT_PORT = 1883;
const char MQTT_CLIENT_ID[] = "ecoduboiron-arduino-001"; 
const char MQTT_USERNAME[] = "";                        // CHANGE IT IF REQUIRED, empty if not required
const char MQTT_PASSWORD[] = "";                        // CHANGE IT IF REQUIRED, empty if not required

// The MQTT topics that Arduino should publish/subscribe
const char PUBLISH_TOPIC[] = "ecoduboiron-arduino-001/sensors-values";  
const char SUBSCRIBE_TOPIC[] = "ecoduboiron-arduino-001/sensors-command";

const int MEASURE_INTERVAL = 5;  // 5 seconds
const int PUBLISH_INTERVAL = 20;  // 20 seconds



typedef struct{
  unsigned int timestamp;
  float temperature;
  float humidity;
} measurement_t;

// 16K (byte I suppose) of memory is available and kept during hibernate
RTC_DATA_ATTR measurement_t measures[1000];
RTC_DATA_ATTR unsigned int measure_count=0;

#define I2C_SDA 19
#define I2C_SCL 20

// macro definitions
// make sure that we use the proper definition of NO_ERROR
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0


WiFiClient network;
MQTTClient mqtt = MQTTClient(256);

static char errorMessage[64];
static int16_t error;

RTC_DATA_ATTR unsigned long lastPublishTime = 0;
RTC_DATA_ATTR unsigned long lastMeasureTime = 0;
SensirionI2cSht4x sensor;

int LedPin = 15;

void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason){
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}


void setup(){
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


  if (tv_now.tv_sec - lastMeasureTime > MEASURE_INTERVAL) 
  {
    measurement_t *measure = &measures[measure_count++];

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


 if (tv_now.tv_sec - lastPublishTime > PUBLISH_INTERVAL) 
  {  
    //wifi_scan();

    Serial.print("Arduino - Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.println(WiFi.status());
    }

    //digitalWrite(LedPin, HIGH);
    //Serial.println("Toggle!");


    Serial.println("WiFi connected!");

    // print your board's IP address:
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    connectToMQTT();

    mqtt.loop(); //TODO: Is it needed
    
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



  // Serial.print("Sleeping for seconds: ");
  // Serial.println(MEASURE_INTERVAL/1000);

  // esp_sleep_enable_timer_wakeup(MEASURE_INTERVAL * 1000);
 
  // esp_deep_sleep_start();


}


void wifi_scan()
{
  Serial.println("Scan start");
 
  // WiFi.scanNetworks will return the number of networks found.
  int n = WiFi.scanNetworks();
  Serial.println("Scan done");
  if (n == 0) {
      Serial.println("no networks found");
  } else {
      Serial.print(n);
      Serial.println(" networks found");
      Serial.println("Nr | SSID                             | RSSI | CH | Encryption");
      for (int i = 0; i < n; ++i) {
          // Print SSID and RSSI for each network found
          Serial.printf("%2d",i + 1);
          Serial.print(" | ");
          Serial.printf("%-32.32s", WiFi.SSID(i).c_str());
          Serial.print(" | ");
          Serial.printf("%4d", WiFi.RSSI(i));
          Serial.print(" | ");
          Serial.printf("%2d", WiFi.channel(i));
          Serial.print(" | ");
          switch (WiFi.encryptionType(i))
          {
          case WIFI_AUTH_OPEN:
              Serial.print("open");
              break;
          case WIFI_AUTH_WEP:
              Serial.print("WEP");
              break;
          case WIFI_AUTH_WPA_PSK:
              Serial.print("WPA");
              break;
          case WIFI_AUTH_WPA2_PSK:
              Serial.print("WPA2");
              break;
          case WIFI_AUTH_WPA_WPA2_PSK:
              Serial.print("WPA+WPA2");
              break;
          case WIFI_AUTH_WPA2_ENTERPRISE:
              Serial.print("WPA2-EAP");
              break;
          case WIFI_AUTH_WPA3_PSK:
              Serial.print("WPA3");
              break;
          case WIFI_AUTH_WPA2_WPA3_PSK:
              Serial.print("WPA2+WPA3");
              break;
          case WIFI_AUTH_WAPI_PSK:
              Serial.print("WAPI");
              break;
          default:
              Serial.print("unknown");
          }
          Serial.println();
          delay(10);
      }
  }
  Serial.println("");

  // Delete the scan result to free memory for code below.
  WiFi.scanDelete();
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

void connectToMQTT() 
{
  // Connect to the MQTT broker
  mqtt.begin(MQTT_BROKER_ADRRESS, MQTT_PORT, network);

  // Create a handler for incoming messages
  mqtt.onMessage(messageHandler);

  Serial.print("Arduino - Connecting to MQTT broker");

  while (!mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) 
  {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (!mqtt.connected()) 
  {
    Serial.println("Arduino - MQTT broker Timeout!");
    return;
  }

  // Subscribe to a topic, the incoming messages are processed by messageHandler() function
  if (mqtt.subscribe(SUBSCRIBE_TOPIC))
    Serial.print("Arduino - Subscribed to the topic: ");
  else
    Serial.print("Arduino - Failed to subscribe to the topic: ");

  Serial.println(SUBSCRIBE_TOPIC);
  Serial.println("Arduino - MQTT broker Connected!");
}

void messageHandler(String &topic, String &payload) 
{
  Serial.println("Arduino - received from MQTT:");
  Serial.println("- topic: " + topic);
  Serial.println("- payload:");
  Serial.println(payload);
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

  mqtt.publish(PUBLISH_TOPIC, messageBuffer);

  Serial.println("Arduino - sent to MQTT:");
  Serial.print("- topic: ");
  Serial.println(PUBLISH_TOPIC);
  Serial.print("- payload:");
  Serial.println(messageBuffer);
}


void loop() 
{
  mqtt.loop();

  Serial.print("Sleeping for seconds: ");
  Serial.println(MEASURE_INTERVAL);

  esp_sleep_enable_timer_wakeup(MEASURE_INTERVAL * 1000 * 1000);
 
  esp_deep_sleep_start();


}