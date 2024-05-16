#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SensirionI2cSht4x.h>
#include <Wire.h>

/* TODOs:
Check if i2c lp is easy to use?
Use TLS to encrypt mqtt connection
Use cert to authentify device to broker
Check how to configure wifi easily?
Make it connect to wifi a lot faster
Handle error cases
Show errors on leds?

*/

const char* test_root_ca = \
  "-----BEGIN CERTIFICATE-----\n" \
"MIIEAzCCAuugAwIBAgIUBY1hlCGvdj4NhBXkZ/uLUZNILAwwDQYJKoZIhvcNAQEL\n" \
"BQAwgZAxCzAJBgNVBAYTAkdCMRcwFQYDVQQIDA5Vbml0ZWQgS2luZ2RvbTEOMAwG\n" \
"A1UEBwwFRGVyYnkxEjAQBgNVBAoMCU1vc3F1aXR0bzELMAkGA1UECwwCQ0ExFjAU\n" \
"BgNVBAMMDW1vc3F1aXR0by5vcmcxHzAdBgkqhkiG9w0BCQEWEHJvZ2VyQGF0Y2hv\n" \
"by5vcmcwHhcNMjAwNjA5MTEwNjM5WhcNMzAwNjA3MTEwNjM5WjCBkDELMAkGA1UE\n" \
"BhMCR0IxFzAVBgNVBAgMDlVuaXRlZCBLaW5nZG9tMQ4wDAYDVQQHDAVEZXJieTES\n" \
"MBAGA1UECgwJTW9zcXVpdHRvMQswCQYDVQQLDAJDQTEWMBQGA1UEAwwNbW9zcXVp\n" \
"dHRvLm9yZzEfMB0GCSqGSIb3DQEJARYQcm9nZXJAYXRjaG9vLm9yZzCCASIwDQYJ\n" \
"KoZIhvcNAQEBBQADggEPADCCAQoCggEBAME0HKmIzfTOwkKLT3THHe+ObdizamPg\n" \
"UZmD64Tf3zJdNeYGYn4CEXbyP6fy3tWc8S2boW6dzrH8SdFf9uo320GJA9B7U1FW\n" \
"Te3xda/Lm3JFfaHjkWw7jBwcauQZjpGINHapHRlpiCZsquAthOgxW9SgDgYlGzEA\n" \
"s06pkEFiMw+qDfLo/sxFKB6vQlFekMeCymjLCbNwPJyqyhFmPWwio/PDMruBTzPH\n" \
"3cioBnrJWKXc3OjXdLGFJOfj7pP0j/dr2LH72eSvv3PQQFl90CZPFhrCUcRHSSxo\n" \
"E6yjGOdnz7f6PveLIB574kQORwt8ePn0yidrTC1ictikED3nHYhMUOUCAwEAAaNT\n" \
"MFEwHQYDVR0OBBYEFPVV6xBUFPiGKDyo5V3+Hbh4N9YSMB8GA1UdIwQYMBaAFPVV\n" \
"6xBUFPiGKDyo5V3+Hbh4N9YSMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEL\n" \
"BQADggEBAGa9kS21N70ThM6/Hj9D7mbVxKLBjVWe2TPsGfbl3rEDfZ+OKRZ2j6AC\n" \
"6r7jb4TZO3dzF2p6dgbrlU71Y/4K0TdzIjRj3cQ3KSm41JvUQ0hZ/c04iGDg/xWf\n" \
"+pp58nfPAYwuerruPNWmlStWAXf0UTqRtg4hQDWBuUFDJTuWuuBvEXudz74eh/wK\n" \
"sMwfu1HFvjy5Z0iMDU8PUDepjVolOCue9ashlS4EB5IECdSR2TItnAIiIwimx839\n" \
"LdUdRudafMu5T5Xma182OC0/u/xRlEm+tvKGGmfFcN0piqVl8OrSPBgIlb+1IKJE\n" \
"m/XriWr/Cq4h/JfB7NTsezVslgkBaoU=\n" \
"-----END CERTIFICATE-----";


// You can use x.509 client certificates if you want
//to verify the client
const char* test_client_key = \ 
"-----BEGIN PRIVATE KEY-----\n" \
"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\n" \
"LclHGp6yPssZWlqooAZ4HkJB9g/ri3E=\n" \
"-----END PRIVATE KEY-----";

//to verify the client
const char* test_client_cert =  \
  "-----BEGIN CERTIFICATE-----\n" \
"MIIEazCCA1OgAwIBAgIBADANBgkqhkiG9w0BAQsFADCBkDELMAkGA1UEBhMCR0Ix\n" \
"FzAVBgNVBAgMDlVuaXRlZCBLaW5nZG9tMQ4wDAYDVQQHDAVEZXJieTESMBAGA1UE\n" \
"CgwJTW9zcXVpdHRvMQswCQYDVQQLDAJDQTEWMBQGA1UEAwwNbW9zcXVpdHRvLm9y\n" \
"ZzEfMB0GCSqGSIb3DQEJARYQcm9nZXJAYXRjaG9vLm9yZzAeFw0yNDA1MTYxMjQ0\n" \
"NTlaFw0yNDA4MTQxMjQ0NTlaMEUxIDAeBgNVBAMMF2Vjb2R1Ym9pcm9uLWFyZHVp\n" \
"bm8tMDAxMQswCQYDVQQGEwJDSDEUMBIGA1UECgwLRWNvZHVib2lyb24wggIiMA0G\n" \
"CSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQCnQYYbifiQYU5V9Ke2oqLxTMzLqSA0\n" \
"cB16S1Id06MvD7uAveuBvlrHebrZFi0HhvWJrZTRKteArSm/ZrKF5iJFZTWH8Ioa\n" \
"falacL4O/QcWrmyUR6SNQVmqrLVSaeTuIiG8C8H1tQPdRNWAZF/4tBk1aNUAWEGf\n" \
"buxmIBn5JmPlGI7MI3CZiG6feIhR7Sro0I6En1rt8vHtV3efMIBShzBkZ1NSvXzI\n" \
"sd+wss/wckCCXyuKlHuyoach291jfWlfSOttikda/ztMHR/vAvFVVghVaQU26ZCu\n" \
"5zvUKfwhWfkqismDLz3xIBDyeqwlM2ogMZfoa2ijxlwgnjKgtfpc6C0bmO+Bzf5k\n" \
"CtCVEWxRT8KIGe8KRLvVXJeU1mXcBBLpJ7ybsgpqkoGWd9tWtih161K62ogM4doD\n" \
"pcdqSmE6a6Ie0HXnei2NoFfmHsXjjgYipGb0zED9RVqJ67y6S9Ydnn+Q3jMxA6Ol\n" \
"5VWyviOnWClnQOk2KL2aTbk+/fhHF5zRWFvCrmPsG0odrhpyczyzjT0RwaccTPft\n" \
"zAJvaiNSDQ9dKRpRMG7laCMKCXzoksUhmCmRkC5hVsCpYjcc4N4Flle+CgsYXhUu\n" \
"VYn9u1wNAulEZngVPCjqXbV8P1Kel3dnUcuvaNiqw1qkkE01nHaSC4v5/Es8iiWt\n" \
"72d4gVy/0yfQwwIDAQABoxowGDAJBgNVHRMEAjAAMAsGA1UdDwQEAwIF4DANBgkq\n" \
"hkiG9w0BAQsFAAOCAQEALR6ZrAU2ode3/EZBwlvhNoz35H31sqPFrCP3+n4Hdu6z\n" \
"hARuMXFEZt9q+fwFOfKR05sJW1evdNXOV04g8nYEMvYGR1c7TvlzE3KEUgSVLoYE\n" \
"NqNnJZo2GrOEQPyHdjZ89MQbYVomuSWeKwZdTKWXw80PWqTRUMvq7qfLvokZhOwv\n" \
"DD938GZ6JBt+/n+XkyM3mLP2luHYcp+/o/F+SSK0M/GdrtHdQzTbEwj4ti/JTWUs\n" \
"318Z/6qzxAuxIi33D8CU8AlfjlU8wEP9xkLGVX/pl76MO3Q0+K7nbxQR3DjXjS8m\n" \
"iY8WbjQj97JLX4tEAfPO46B6bxB7Ufukuz5fSOfRzg==\n" \
"-----END CERTIFICATE-----";



const char *WIFI_SSID = "ROOMZ";    //WIFI Name
const char *WIFI_PASSWORD = "12345678"; //WIFI Password 

const char MQTT_BROKER_ADRRESS[] = "test.mosquitto.org"; 
const int MQTT_PORT = 8883;
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

WiFiClientSecure  espClient;
PubSubClient client(espClient);


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

void callback(char* topic, byte* message, unsigned int length) {
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

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
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
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void connectToMQTT() 
{

  Serial.print("Arduino - Connecting to MQTT broker");

  espClient.setCACert(test_root_ca);
  espClient.setCertificate(test_client_cert); // for client verification
  espClient.setPrivateKey(test_client_key);  // for client verification

  client.setServer(MQTT_BROKER_ADRRESS, MQTT_PORT);
  reconnect();
  // Connect to the MQTT broker
  while (!client.connected())
  {
    delay(1000);
    
  }

  // Create a handler for incoming messages
  client.setCallback(callback);
  if (!client.connected()) 
  {
    reconnect();
  }

  Serial.print("Arduino - Subscribe to the topic: ");
  client.subscribe(SUBSCRIBE_TOPIC);


  Serial.println("Arduino - MQTT broker Connected!");
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
  client.loop();

  Serial.print("Sleeping for seconds: ");
  Serial.println(MEASURE_INTERVAL);

  esp_sleep_enable_timer_wakeup(MEASURE_INTERVAL * 1000 * 1000);
 
  esp_deep_sleep_start();


}