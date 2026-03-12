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

// ---------- FLAG WIFI ----------
bool wifiReconnecting = false;

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
int mseg_delay = 16;

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

// ---------- OSC ADDRESSES ----------
const char addr_sensores[]  = "/sensores";
const char addr_loopRate[]  = "/loopRate";
const char addr_ewmaAlpha[] = "/ewmaAlpha";
const char addr_boton1[]    = "/boton1";
const char addr_boton2[]    = "/boton2";
const char addr_mac[]    = "/macaddress";

////////////////////////////////
//////// NETWORK SETUP /////////
////////////////////////////////
/*
DESKTOP-2Q055UG 4525
3p;2A849
*/

/*
// IP config superDDL 2.4
IPAddress staticIP(192, 168, 0, 102);  // IP local ### Me parece que no le da bola a esto
IPAddress gateway(192, 168, 0, 1);    // Gateway
IPAddress subnet(255, 255, 255, 0);   // Subnet mask

const IPAddress outIp(192, 168, 0, 107);  // IP destino ### Probar con 255 para broadcast

*/

/*
// IP config LAB1507
IPAddress staticIP(10, 1, 101, 171);
IPAddress gateway(10, 1, 103, 254);
IPAddress subnet(255, 255, 252, 0);

const IPAddress outIp(10, 1, 103, 255);
*/

/*
//IP config Lowpoly99
IPAddress staticIP(192, 168, 1, 102);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

const IPAddress outIp(192, 168, 1, 255); //IP destino. Último octeto en 255 para broadcas
*/

//IP config dd-wrtt

IPAddress staticIP(192, 168, 1, 102);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

const IPAddress outIp(192, 168, 1, 255); //IP destino. Último octeto en 255 para broadcas


// UDP
const unsigned int outPort   = 9000;
const unsigned int localPort = 8000;

// WIFI

/*
char ssid[] = "superDDL 2.4";
char pass[] = "FTZWCZM2KTZJ";
*/

/*
char ssid[] = "LAB1507";
char pass[] = "7051BAL!";
*/

/*
char ssid[] = "lowpoly99";
char pass[] = "lowpoly99";
*/
 
char ssid[] = "dd-wrtt";
char pass[] = "FTZWCZM2KTZJ";


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
  delay(1000);
  Serial.begin(9600);
  
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
    Serial.print("WiFi status: ");
    Serial.println(WiFi.status());

    WiFi.begin(ssid, pass);
    delay(500);
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

  macAddressStr = WiFi.macAddress();
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
  // Chequeo que estoy conectado al wifi
   if (WiFi.status() != WL_CONNECTED) {

    // ---------- BUCLE DE RECONEXIÓN ----------
    Serial.println("WiFi desconectado");
    WiFi.disconnect();
    WiFi.begin(ssid, pass);

    // Mientras no se conecte, titilea el LED cyan
    while (WiFi.status() != WL_CONNECTED) {
      setLed(false, true, true);   // Cyan ON
      delay(100);
      setLed(false, false, false); // OFF
      delay(100);
      setLed(false, true, true);   // Cyan ON
      delay(100);
      setLed(false, false, false); // OFF
      delay(100);
      // Intento reconectar periódicamente
      WiFi.disconnect();
      WiFi.begin(ssid, pass);

      Serial.print("WiFi status: ");
      Serial.println(WiFi.status());
      delay(1000);
    }

    // ---------- WIFI RECONECTADO ----------
    Serial.println("WiFi reconectado");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    Serial.print("Gateway: "); Serial.println(WiFi.gatewayIP());
    Serial.print("Subnet: "); Serial.println(WiFi.subnetMask());
    Udp.begin(localPort);
    Serial.print("Puerto UDP: "); Serial.println(localPort);
    Serial.print("MAC ESP32: "); Serial.println(WiFi.macAddress());

    // Conectado: verde fijo
    setLed(false, true, false);
  }

// ---------- WIFI RECONECTADO ----------
if (wifiReconnecting && WiFi.status() == WL_CONNECTED) {
  wifiReconnecting = false;

  Serial.println("WiFi reconectado");
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

  setLed(false, true, false); // verde
}

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

  // Mac address
  OSCMessage msgMac(addr_mac);
  msgMac.add(macAddressStr.c_str());  // enviar como string
  bundle.add(msgMac);

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

/*
  // Print out the values
  Serial.print("(x, y, z): ");
  Serial.print(a.acceleration.x);
  Serial.print(", ");
  Serial.print(a.acceleration.y);
  Serial.print(", ");
  Serial.print(a.acceleration.z);
  Serial.println(". ");

  Serial.print("(y, p, r): ");
  Serial.print(g.gyro.x);
  Serial.print(", ");
  Serial.print(g.gyro.y);
  Serial.print(", ");
  Serial.print(g.gyro.z);
  Serial.println(". ");
*/

  delay(mseg_delay);
}
