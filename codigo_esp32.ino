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

// ---------- BOTONES ----------
const int boton1Pin = 33;
const int boton2Pin = 32;
bool boton1State = false;
bool boton2State = false;

// ---------- LED RGB (ÁNODO COMÚN) ----------
const int ledR = 27;
const int ledG = 26;
const int ledB = 25;

// ---------- LOOP RATE ----------
int mseg_delay = 10;

// ---------- EWMA ----------
float alpha = 0.2;
float wma_x = 0.0;
float wma_y = 0.0;
float wma_z = 0.0;
float wma_rol = 0.0;
float wma_pic = 0.0;
float wma_yaw = 0.0;

// ---------- OSC ADDRESSES ----------
const char addr_sensores[]  = "/4/sensores";
const char addr_loopRate[]  = "/4/loopRate";
const char addr_ewmaAlpha[] = "/4/ewmaAlpha";
const char addr_boton1[]    = "/boton1";
const char addr_boton2[]    = "/boton2";

////////////////////////////////
//////// NETWORK SETUP /////////
////////////////////////////////

// IP
IPAddress staticIP(10, 1, 101, 171);
IPAddress gateway(10, 1, 103, 254);
IPAddress subnet(255, 255, 252, 0);

const IPAddress outIp(10, 1, 103, 255);

/*
IPAddress staticIP(192, 168, 1, 102);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

const IPAddress outIp(192, 168, 1, 255);
*/

// UDP
const unsigned int outPort   = 9000;
const unsigned int localPort = 8000;

// WIFI
char ssid[] = "LAB1507";
char pass[] = "7051BAL!";

////////////////////////////////
////////// INSTANCIAS //////////
////////////////////////////////

WiFiUDP Udp;
Adafruit_MPU6050 mpu;

////////////////////////////////
////////// FUNCIONES ///////////
////////////////////////////////

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

// Ánodo común: LOW = encendido, HIGH = apagado
void setLed(bool r, bool g, bool b) {
  digitalWrite(ledR, r ? LOW : HIGH);
  digitalWrite(ledG, g ? LOW : HIGH);
  digitalWrite(ledB, b ? LOW : HIGH);
}

////////////////////////////////
//////////// SETUP /////////////
////////////////////////////////

void setup(void) {
  Serial.begin(115200);
  delay(1000);

  Serial.println("////////////////////////////////");
  Serial.println("////////// JOYSTICK 1 //////////");
  Serial.println("////////////////////////////////");

  // ---------- LED ----------
  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledB, OUTPUT);

  // Inicialmente apagado
  setLed(false, false, false);

  // ---------- WIFI ----------
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  // Aplico IP ESTÁTICA
  if (!WiFi.config(staticIP, gateway, subnet)) {
    Serial.println("Error configurando IP estática");
  }

  // Intento conectarme a la red
  WiFi.begin(ssid, pass);

  // Mientras se conecta, parpadea cyan
  while (WiFi.status() != WL_CONNECTED) {
    setLed(false, true, true);   // Cyan ON
    delay(200);
    setLed(false, false, false); // OFF
    delay(200);
  }

  // Printeo datos de red del ESP32 post conexión
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

  Serial.print("MAC ESP32: ");
  Serial.println(WiFi.macAddress());

  // ---------- BOTONES ----------
  pinMode(boton1Pin, INPUT_PULLUP);
  pinMode(boton2Pin, INPUT_PULLUP);

  // ---------- MPU6050 ----------
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

  // Conectado: verde fijo
  setLed(false, true, false);
}

////////////////////////////////
//////////// LOOP //////////////
////////////////////////////////

void loop() {

  // ---------- OSC IN ----------
  receiveMessage();

  // ---------- MPU ----------
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  wma_x   = alpha * a.acceleration.x + (1 - alpha) * wma_x;
  wma_y   = alpha * a.acceleration.y + (1 - alpha) * wma_y;
  wma_z   = alpha * a.acceleration.z + (1 - alpha) * wma_z;
  wma_rol = alpha * g.gyro.x         + (1 - alpha) * wma_rol;
  wma_pic = alpha * g.gyro.y         + (1 - alpha) * wma_pic;
  wma_yaw = alpha * g.gyro.z         + (1 - alpha) * wma_yaw;

  // ---------- BOTONES ----------
  boton1State = !digitalRead(boton1Pin);
  boton2State = !digitalRead(boton2Pin);

  // ---------- OSC BUNDLE ----------
  OSCBundle bundle;

  // Sensores
  OSCMessage msgSens(addr_sensores);
  msgSens.add(wma_x)
         .add(wma_y)
         .add(wma_z)
         .add(wma_rol)
         .add(wma_pic)
         .add(wma_yaw);
  bundle.add(msgSens);

  // Botón 1
  OSCMessage msgB1(addr_boton1);
  msgB1.add(0.0f + boton1State);
  bundle.add(msgB1);

  // Botón 2
  OSCMessage msgB2(addr_boton2);
  msgB2.add(0.0f + boton2State);
  bundle.add(msgB2);

  // Envío único
  Udp.beginPacket(outIp, outPort);
  bundle.send(Udp);
  Udp.endPacket();
  bundle.empty();

  delay(mseg_delay);
}
