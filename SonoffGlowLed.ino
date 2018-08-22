/*
    Generic Esp8285 Module: CPU Frequency: 80MHz
    Flash Size: 1M (64K SPIFFS)
*/

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <MQTTClient.h>   //https://github.com/256dpi/arduino-mqtt


const char* vrs = "1.0"; // EvertDekker.com 2017
const char* ssid = "MySsid";
const char* password = "MyWifiPassw";
const char* hostn = "Sonoff Touch";

IPAddress ip(192, 168, 1, 123);
IPAddress gateway(192, 168, 1, 254);
IPAddress subnet(255, 255, 255, 0);

//Set the debug output, only one at the time
//#define NONE true
#define SERIAL true

#ifdef  NONE
#define DEBUG_PRINTF(x,...)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT(x)
#elif SERIAL
#define DEBUG_PRINTF(x,...) Serial.printf(x, ##__VA_ARGS__)
#define DEBUG_PRINTLN(x)    Serial.println(x)
#define DEBUG_PRINT(x)      Serial.print(x)
#endif


//Config
#define WIFILED   13
#define RELAY1  12 //Operates Relay coil
#define TOUCHLED 4
#define TOUCHINPUT 0

// declaration
void connect(); // <- predefine connect() for setup Mqtt()

//WiFiClientSecure net ;
WiFiClient net;
MQTTClient client;

// globals
unsigned long startMills;
int nodestatus;
unsigned long lastMillis = 0;
int Output1 = 0;
int brightness = 0;    // how bright the LED is
int fadeAmount = 5;    // how many points to fade the LED by

void setup()
{
  Serial.begin(115200);
  Serial.println("Booting");
  pinMode(WIFILED, OUTPUT);
  digitalWrite(WIFILED, LOW);
  pinMode(RELAY1, OUTPUT);
  pinMode(TOUCHLED, OUTPUT);
  pinMode(TOUCHINPUT, INPUT);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(hostn);
  WiFi.begin(ssid, password);
  WiFi.config(ip, gateway, subnet);


  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    DEBUG_PRINTLN("Connection Failed! Rebooting...");
    blinkledlong(1); //1 long blink: failed wifi connected
    ESP.deepSleep(120 * 1000000);   // deepSleep time is defined in microseconds. Multiply seconds by 1e6
    delay(1000);
  }
  blinkledshort(1); //1 fast blink: wifi connected
  ArduinoOTA.setHostname(hostn);

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)OTA_PASSW);

  ArduinoOTA.onStart([]()
  {
    DEBUG_PRINTLN("OTA Start");
  });

  ArduinoOTA.onEnd([]()
  {
    DEBUG_PRINTLN("\nOTA End");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
  {
    DEBUG_PRINTF("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error)
  {
    DEBUG_PRINTF("OTA Error[%u]: ", error);
    blinkledlong(3); //3 long blink: Ota Failed
    if (error == OTA_AUTH_ERROR) DEBUG_PRINTLN("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) DEBUG_PRINTLN("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) DEBUG_PRINTLN("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) DEBUG_PRINTLN("Receive Failed");
    else if (error == OTA_END_ERROR) DEBUG_PRINTLN("End Failed");
  });

  ArduinoOTA.begin();
  DEBUG_PRINTLN("Ready");
  DEBUG_PRINT("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());
  DEBUG_PRINT("Hostname: ");
  DEBUG_PRINTLN(WiFi.hostname());
  client.begin("192.168.1.127", net); // MQTT brokers usually use port 8883 for secure connections
  client.onMessage(messageReceived);
  connect();
}

void loop()
{
  ArduinoOTA.handle();
  delay(10); // <- fixes some issues with WiFi stability

  if (!client.connected())
  {
    DEBUG_PRINTLN("\Retry connecting Mqtt");
    connect();
  }

  int reading = digitalRead(TOUCHINPUT);
  if (reading == false)
  {
    handlebutton1();
    Serial.print("Button ");
    Serial.println(Output1);
    delay(500);
  }


  if (Output1 == false)
  {
    analogWrite(TOUCHLED, brightness);
    brightness = brightness + fadeAmount;     // change the brightness for next time through the loop:
    if (brightness <= 0 || brightness >= 1020)   // reverse the direction of the fading at the ends of the fade:
    {
      fadeAmount = -fadeAmount;
    }
    delay(8); // wait for 8 milliseconds to see the dimming effect
  }
  else
  {
    analogWrite(TOUCHLED, 1023);
  }



  client.loop();
}

void handlebutton1()
{
  if (Output1 == true)
  {
    resetoutput1();
  }
  else
  {
    setoutput1();
  }
}


void setoutput1()
{
  Output1 = true;
  digitalWrite(RELAY1, HIGH);
  send2broker(Output1);
}

void resetoutput1()
{
  Output1 = false;
  digitalWrite(RELAY1, LOW);
  send2broker(Output1);
}
void send2broker(int outstat)
{
  client.publish("/test/lichtschakelaar/nodeid", String(123), true, 0);
  client.publish("/test/lichtschakelaar/versie", String(vrs), true, 0);
  client.publish("/test/lichtschakelaar/output1", String(outstat) , true, 0);
  client.publish("/test/lichtschakelaar/status", String(nodestatus), true, 0);
}

void connect()
{
  DEBUG_PRINT("Mqtt checking wifi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    DEBUG_PRINT(".");
    delay(1000);
  }

  DEBUG_PRINT("\nMqtt connecting...");
  while (!client.connect(hostn, "Mqtt_login" , "Mqtt_passw"))
  {
    DEBUG_PRINT(".");
    blinkledlong(2); //2 Long blink: Mqtt failed to connect
    delay(1000);
    // ArduinoOTA.handle();
  }
  DEBUG_PRINTLN("\nMqtt connected!");
  blinkledshort(2); //2 fast blink: Mqtt connected
  client.subscribe("/test/lichtschakelaar/status");
  client.subscribe("/test/lichtschakelaar/out1");
}


void messageReceived(String &topic, String &payload)
{
  //nodestatus = payload.toInt();
  DEBUG_PRINT("incoming: ");
  DEBUG_PRINT(topic);
  DEBUG_PRINT(" ");
  DEBUG_PRINT(payload);
  DEBUG_PRINTLN();

  if (topic == "/test/lichtschakelaar/status")
  {
    nodestatus = payload.toInt();
  }


  if (topic == "/test/lichtschakelaar/out1")
  {
    if (payload.toInt() == true)
    {
      setoutput1();
    }
    else
    {
      resetoutput1();
    }
  }
}



void blinkledshort(byte blinks)
{
  for (int i = 1; i <= blinks; i++)
  {
    digitalWrite(WIFILED, HIGH);
    delay(50);
    digitalWrite(WIFILED, LOW);
    delay(100);
  }
}

void blinkledlong(byte blinks)
{
  for (int i = 1; i <= blinks; i++)
  {
    digitalWrite(WIFILED, HIGH);
    delay(500);
    digitalWrite(WIFILED, LOW);
    delay(500);
  }
}

