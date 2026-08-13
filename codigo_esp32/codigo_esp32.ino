#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <WiFiUdp.h>
#include <WiFi.h>

////////////////////////////////
//////// INIT VARIABLES ////////
////////////////////////////////

unsigned long lastUdpRestart = 0;
const unsigned long udpRestartInterval = 15000; // 15 seconds

// ---------- LOOP RATE ----------
int mseg_delay = 20;

// ---------- OSC ADDRESSES ----------

const char addr_loopRate[]  = "/loopRate";
const char addr_ewmaAlpha[] = "/ewmaAlpha";
const char* oscAddress = "/player";

// ---------- BOTONES ----------
const int boton1Pin = 33;
const int boton2Pin = 32;
bool boton1State = false;
bool boton2State = false;

// ---------- LED RGB (ÁNODO COMÚN) ----------
const int ledR = 27;
const int ledG = 26;
const int ledB = 25;

// ---------- FLAG WIFI ----------
bool wifiReconnecting = false;

// ---------- EWMA ----------
float alpha = 0.2;
float wma_x = 0.0;
float wma_y = 0.0;
float wma_z = 0.0;
float wma_rol = 0.0;
float wma_pic = 0.0;
float wma_yaw = 0.0;

// ---------- MAC ADDRESS ----------
String macAddressStr = "";

////////////////////////////////
//////// NETWORK SETUP /////////
////////////////////////////////

// ---------- NETWORK STRUCT ----------
struct WifiNetwork {
  const char* ssid;
  const char* pass;
  IPAddress staticIP;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress outIp;
};

// ---------- NETWORK LIST ----------
WifiNetwork networks[] = {
  {
    "QC_WLAN",
    "748159263",
    IPAddress(192,168,0,101),
    IPAddress(192,168,0,1),
    IPAddress(255,255,255,0),
    IPAddress(192,168,0,100)
  },
  {
    "SSID",
    "PASSWD",
    IPAddress(192,168,0,101),
    IPAddress(192,168,0,1),
    IPAddress(255,255,255,0),
    IPAddress(192,168,0,100)
  }
};

const int networkCount = sizeof(networks) / sizeof(networks[0]);

IPAddress staticIP;
IPAddress gateway;
IPAddress subnet;
IPAddress outIp;

// UDP
const unsigned int outPort   = 9000;
const unsigned int localPort = 8000;

////////////////////////////////
////////// INSTANCIAS //////////
////////////////////////////////

WiFiUDP Udp;
Adafruit_MPU6050 mpu;

////////////////////////////////
////////// FUNCIONES ///////////
////////////////////////////////
void refreshUDP() {
  if (millis() - lastUdpRestart > udpRestartInterval) {
    Serial.println("Refreshing UDP socket");
    Udp.stop();
    delay(10);
    Udp.begin(localPort);
    lastUdpRestart = millis();
  }
}

bool connectToKnownWiFi() {

  for (int i = 0; i < networkCount; i++) {

    Serial.print("Intentando red: ");
    Serial.println(networks[i].ssid);

    WiFi.disconnect(true);
    delay(200);

    WiFi.config(
      networks[i].staticIP,
      networks[i].gateway,
      networks[i].subnet
    );

    WiFi.begin(networks[i].ssid, networks[i].pass);

    int timeout = 20;

    while (WiFi.status() != WL_CONNECTED && timeout--) {
      setLed(false, true, true);
      delay(200);
      setLed(false, false, false);
      delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {

      staticIP = networks[i].staticIP;
      gateway  = networks[i].gateway;
      subnet   = networks[i].subnet;
      outIp    = networks[i].outIp;

      Serial.println("Conectado a:");
      Serial.println(networks[i].ssid);

      return true;
    }
  }

  return false;
}

void loopRate(OSCMessage &msg) {
  if (msg.isInt(0)) {
    if (msg.getInt(0) >= 5) {
      mseg_delay = msg.getInt(0);
      Serial.print("Nuevo Loop Rate: ");
      Serial.println(mseg_delay);
    }
  }
}

void ewmaAlpha(OSCMessage &msg) {
  if (msg.isFloat(0)) {
    float newAlpha = msg.getFloat(0);
    if (newAlpha >= 0.0 && newAlpha <= 1.0) {
      alpha = newAlpha;
      Serial.print("Nuevo Alpha: ");
      Serial.println(alpha);
    }
  }
}

void receiveMessage() {
  OSCMessage inmsg;
  int size = Udp.parsePacket();
  if (size > 0) {
    while (size--) inmsg.fill(Udp.read());
    if (!inmsg.hasError()) {
      inmsg.dispatch(addr_loopRate, loopRate);
      inmsg.dispatch(addr_ewmaAlpha, ewmaAlpha);
    }
  }
}

////////////////////////////////
////////// LED UTILS ///////////
////////////////////////////////

void setLed(bool r, bool g, bool b) {
  digitalWrite(ledR, r ? LOW : HIGH);
  digitalWrite(ledG, g ? LOW : HIGH);
  digitalWrite(ledB, b ? LOW : HIGH);
}

////////////////////////////////
//////////// SETUP /////////////
////////////////////////////////

void setup(void) {
  delay(1000);
  Serial.begin(9600);
  
  Serial.println("////////////////////////////////");
  Serial.println("////////// JOYSTICK 1 //////////");
  Serial.println("////////////////////////////////");

  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledB, OUTPUT);

  setLed(false, false, false);

  WiFi.setSleep(false);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  while (!connectToKnownWiFi()) {
    Serial.println("No se pudo conectar a ninguna red conocida");
    delay(2000);
  }

  Serial.println("WiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("Subnet: ");
  Serial.println(WiFi.subnetMask());

  Udp.begin(localPort);
  Serial.print("Puerto UDP: ");
  Serial.println(localPort);

  macAddressStr = WiFi.macAddress();
  Serial.print("MAC ESP32: ");
  Serial.println(WiFi.macAddress());

  pinMode(boton1Pin, INPUT_PULLUP);
  pinMode(boton2Pin, INPUT_PULLUP);

  if (!mpu.begin()) {
    Serial.println("MPU6050 no encontrado");
    while (true) {
      setLed(true, false, false);
      delay(100);  
      setLed(false, false, false);
      delay(100);
      setLed(true, false, false);
      delay(100);  
      setLed(false, false, false);
      delay(1000);
      if(mpu.begin()) {
        break;
      }
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU6050 OK");

  setLed(false, true, false);
}

////////////////////////////////
//////////// LOOP //////////////
////////////////////////////////

void loop() {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WiFi desconectado");

    while (!connectToKnownWiFi()) {
      setLed(false, true, true);
      delay(200);
      setLed(false, false, false);
      delay(200);
      Serial.println("Reintentando conexión...");
    }

    Serial.println("WiFi reconectado");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    Serial.print("Gateway: "); Serial.println(WiFi.gatewayIP());
    Serial.print("Subnet: "); Serial.println(WiFi.subnetMask());

    Udp.begin(localPort);
    Serial.print("Puerto UDP: "); Serial.println(localPort);

    Serial.print("MAC ESP32: "); Serial.println(WiFi.macAddress());

    setLed(false, true, false);
  }

  receiveMessage();


  refreshUDP();
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  wma_x   = alpha * a.acceleration.x + (1 - alpha) * wma_x;
  wma_y   = alpha * a.acceleration.y + (1 - alpha) * wma_y;
  wma_z   = alpha * a.acceleration.z + (1 - alpha) * wma_z;
  wma_rol = alpha * g.gyro.x         + (1 - alpha) * wma_rol;
  wma_pic = alpha * g.gyro.y         + (1 - alpha) * wma_pic;
  wma_yaw = alpha * g.gyro.z         + (1 - alpha) * wma_yaw;

  boton1State = !digitalRead(boton1Pin);
  boton2State = !digitalRead(boton2Pin);

  OSCMessage msg(oscAddress);

  msg.add(macAddressStr.c_str());
  msg.add(wma_x);
  msg.add(wma_y);
  msg.add(wma_z);
  msg.add(wma_rol);
  msg.add(wma_pic);
  msg.add(wma_yaw);
  msg.add((int32_t)boton1State);
  msg.add((int32_t)boton2State);

  Udp.beginPacket(outIp, outPort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();

  delay(mseg_delay);
}
