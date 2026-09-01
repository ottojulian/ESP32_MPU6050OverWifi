#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

////////////////////////////////
//////// INIT VARIABLES ////////
////////////////////////////////

// ---------- LOOP RATE ----------
int mseg_delay = 20;

// ---------- OSC ADDRESSES ----------
const char addr_loopRate[] = "/loopRate";
const char addr_ewmaAlpha[] = "/ewmaAlpha";
String oscAddress = "/player1";

// ---------- BOTONES ----------
const int boton1Pin = 33;
const int boton2Pin = 32;
bool boton1State = false;
bool boton2State = false;

// ---------- LED RGB ----------
const int ledR = 27;
const int ledG = 26;
const int ledB = 25;

// ---------- EWMA ----------
float alpha = 0.2;
float wma_x = 0.0;
float wma_y = 0.0;
float wma_z = 0.0;
float wma_rol = 0.0;
float wma_pic = 0.0;
float wma_yaw = 0.0;

// ---------- MAC ----------
String macAddressStr = "";

// ---------- JOYSTICK CONFIG ----------
String joystickGroup = "A";

// ---------- UDP PORT CONFIG ----------
unsigned int controlPort = 9000;

// ---------- DISCOVERY ----------
const unsigned int discoveryPort = 8001;
const unsigned long discoveryInterval = 1000;
unsigned long lastDiscovery = 0;

// ---------- WEB SERVER ----------
WebServer server(80);

// ---------- PERSISTENT STORAGE ----------
Preferences preferences;

////////////////////////////////
//////// NETWORK SETUP /////////
////////////////////////////////

struct WifiNetwork {
  const char* ssid;
  const char* pass;
  IPAddress outIp;
};

WifiNetwork networks[] = {
  {
    "QC_WLAN",
    "748159263",
    IPAddress(192, 168, 0, 100)
  },
  {
    "superDDL 2.4",
    "FTZWCZM2KTZJ",
    IPAddress(192, 168, 0, 255)
  },
  {
    "SSID",
    "PASSWD",
    IPAddress(192, 168, 0, 100)
  }
};

const int networkCount = sizeof(networks) / sizeof(networks[0]);

IPAddress outIp;

////////////////////////////////
////////// INSTANCIAS //////////
////////////////////////////////

WiFiUDP Udp;
WiFiUDP discoveryUdp;
Adafruit_MPU6050 mpu;

////////////////////////////////
////////// FUNCIONES ///////////
////////////////////////////////

// ============================================================
// LED
// ============================================================

void setLed(bool r, bool g, bool b) {
  digitalWrite(ledR, r ? LOW : HIGH);
  digitalWrite(ledG, g ? LOW : HIGH);
  digitalWrite(ledB, b ? LOW : HIGH);
}

// ============================================================
// WIFI
// ============================================================

bool connectToKnownWiFi() {
  for (int i = 0; i < networkCount; i++) {
    Serial.print("Intentando red: ");
    Serial.println(networks[i].ssid);

    WiFi.disconnect(true);
    delay(200);

    WiFi.begin(networks[i].ssid, networks[i].pass);

    int timeout = 20;

    while (WiFi.status() != WL_CONNECTED && timeout--) {
      setLed(false, true, true);
      delay(200);
      setLed(false, false, false);
      delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
      outIp = networks[i].outIp;

      Serial.println("Conectado a:");
      Serial.println(networks[i].ssid);

      Serial.print("IP asignada por DHCP: ");
      Serial.println(WiFi.localIP());

      return true;
    }
  }

  return false;
}

// ============================================================
// LOAD CONFIGURATION
// ============================================================

void loadConfiguration() {
  preferences.begin("joystick", false);

  joystickGroup = preferences.getString("group", "A");
  oscAddress = preferences.getString("address", "/player1");
  controlPort = preferences.getUInt("port", 9000);
  alpha = preferences.getFloat("alpha", 0.2);

  Serial.println("Configuración almacenada:");
  Serial.print("Grupo: ");
  Serial.println(joystickGroup);
  Serial.print("Address: ");
  Serial.println(oscAddress);
  Serial.print("UDP Port: ");
  Serial.println(controlPort);
  Serial.print("EWMA Alpha: ");
  Serial.println(alpha);
}

// ============================================================
// SAVE GROUP
// ============================================================

bool saveJoystickGroup(String newGroup) {
  newGroup.trim();

  if (newGroup.length() != 1)
    return false;

  char group = newGroup.charAt(0);

  if (group >= 'a' && group <= 'z')
    group -= 32;

  if (group < 'A' || group > 'Z')
    return false;

  joystickGroup = String(group);
  preferences.putString("group", joystickGroup);

  Serial.print("Nuevo grupo guardado: ");
  Serial.println(joystickGroup);

  return true;
}

// ============================================================
// SAVE ADDRESS
// ============================================================

bool saveOscAddress(String newAddress) {
  newAddress.trim();

  if (newAddress.length() == 0)
    return false;

  if (newAddress.charAt(0) != '/')
    return false;

  oscAddress = newAddress;
  preferences.putString("address", oscAddress);

  Serial.print("Nuevo OSC Address guardado: ");
  Serial.println(oscAddress);

  return true;
}

// ============================================================
// SAVE UDP PORT
// ============================================================

bool saveControlPort(String newPort) {
  newPort.trim();

  int port = newPort.toInt();

  if (port < 1 || port > 65535)
    return false;

  if (port == discoveryPort)
    return false;

  controlPort = (unsigned int)port;

  preferences.putUInt("port", controlPort);

  Udp.stop();
  delay(10);
  Udp.begin(controlPort);

  Serial.print("Nuevo UDP Port guardado: ");
  Serial.println(controlPort);

  return true;
}

// ============================================================
// SAVE EWMA
// ============================================================

bool saveAlpha(String newAlpha) {
  newAlpha.trim();

  float newValue = newAlpha.toFloat();

  if (newValue < 0.0 || newValue > 1.0)
    return false;

  alpha = newValue;
  preferences.putFloat("alpha", alpha);

  Serial.print("Nuevo EWMA Alpha guardado: ");
  Serial.println(alpha);

  return true;
}

// ============================================================
// OSC SETTINGS
// ============================================================

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
    while (size--)
      inmsg.fill(Udp.read());

    if (!inmsg.hasError()) {
      inmsg.dispatch(addr_loopRate, loopRate);
      inmsg.dispatch(addr_ewmaAlpha, ewmaAlpha);
    }
  }
}

// ============================================================
// DISCOVERY BEACON
// ============================================================

void sendDiscovery() {
  if (millis() - lastDiscovery < discoveryInterval)
    return;

  lastDiscovery = millis();

  OSCMessage msg("/joystick");

  msg.add(macAddressStr.c_str());
  msg.add(WiFi.localIP().toString().c_str());
  msg.add(joystickGroup.c_str());
  msg.add(oscAddress.c_str());
  msg.add((int32_t)controlPort);

  discoveryUdp.beginPacket(
    IPAddress(255, 255, 255, 255),
    discoveryPort
  );

  msg.send(discoveryUdp);
  discoveryUdp.endPacket();

  msg.empty();
}

// ============================================================
// FIND
// ============================================================

void findJoystick() {
  Serial.println("FIND!");

  setLed(true, true, true);
  delay(150);

  setLed(false, false, false);
  delay(150);

  setLed(true, true, true);
  delay(150);

  setLed(false, false, false);
  delay(150);

  setLed(false, true, false);
}

// ============================================================
// WEB PAGE
// ============================================================

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Joystick Configuration</title>
<style>
body {
  font-family: Arial, sans-serif;
  max-width: 500px;
  margin: 40px auto;
  padding: 20px;
  background: #f5f5f5;
}
.container {
  background: white;
  padding: 25px;
  border-radius: 10px;
  box-shadow: 0 2px 10px rgba(0,0,0,0.1);
}
h1 {
  margin-top: 0;
}
.info {
  background: #eeeeee;
  padding: 15px;
  border-radius: 8px;
  margin-bottom: 25px;
}
.info p {
  margin: 8px 0;
}
label {
  display: block;
  margin-bottom: 8px;
  font-weight: bold;
}
input {
  width: 100%;
  box-sizing: border-box;
  padding: 12px;
  font-size: 18px;
  margin-bottom: 20px;
  border: 1px solid #ccc;
  border-radius: 6px;
}
button {
  width: 100%;
  padding: 14px;
  font-size: 18px;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  margin-bottom: 12px;
}
.find {
  background: #ddd;
}
.save {
  background: #ccc;
}
</style>
</head>
<body>
<div class="container">
<h1>Joystick Configuration</h1>

<div class="info">
<p><strong>MAC:</strong><br>
)rawliteral";

  html += WiFi.macAddress();

  html += R"rawliteral(
</p>
<p><strong>IP:</strong><br>
)rawliteral";

  html += WiFi.localIP().toString();

  html += R"rawliteral(
</p>
<p><strong>Current Group:</strong><br>
)rawliteral";

  html += joystickGroup;

  html += R"rawliteral(
</p>
<p><strong>OSC Address:</strong><br>
)rawliteral";

  html += oscAddress;

  html += R"rawliteral(
</p>
<p><strong>UDP Port:</strong><br>
)rawliteral";

  html += String(controlPort);

  html += R"rawliteral(
</p>
<p><strong>EWMA Alpha:</strong><br>
)rawliteral";

  html += String(alpha, 3);

  html += R"rawliteral(
</p>
</div>

<form action="/find" method="POST">
<button class="find" type="submit">FIND</button>
</form>

<form action="/save" method="POST">

<label for="group">Group</label>
<input type="text" name="group" id="group"
       value=")rawliteral";

  html += joystickGroup;

  html += R"rawliteral(" maxlength="1">

<label for="address">OSC Address</label>
<input type="text" name="address" id="address"
       value=")rawliteral";

  html += oscAddress;

  html += R"rawliteral(">

<label for="port">UDP Port</label>
<input type="text" name="port" id="port"
       value=")rawliteral";

  html += String(controlPort);

  html += R"rawliteral(" inputmode="numeric">

<label for="alpha">EWMA Alpha</label>
<input type="text" name="alpha" id="alpha"
       value=")rawliteral";

  html += String(alpha, 3);

  html += R"rawliteral(">

<button class="save" type="submit">SAVE</button>

</form>
</div>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// ============================================================
// FIND REQUEST
// ============================================================

void handleFind() {
  findJoystick();
  handleRoot();
}

// ============================================================
// SAVE REQUEST
// ============================================================

void handleSave() {
  bool success = true;

  if (server.hasArg("group")) {
    if (!saveJoystickGroup(server.arg("group")))
      success = false;
  }

  if (server.hasArg("address")) {
    if (!saveOscAddress(server.arg("address")))
      success = false;
  }

  if (server.hasArg("port")) {
    if (!saveControlPort(server.arg("port")))
      success = false;
  }

  if (server.hasArg("alpha")) {
    if (!saveAlpha(server.arg("alpha")))
      success = false;
  }

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Configuration Saved</title>
<style>
body {
  font-family: Arial, sans-serif;
  max-width: 500px;
  margin: 40px auto;
  padding: 20px;
  background: #f5f5f5;
}
.container {
  background: white;
  padding: 25px;
  border-radius: 10px;
  box-shadow: 0 2px 10px rgba(0,0,0,0.1);
}
h1 {
  margin-top: 0;
}
.info {
  background: #eeeeee;
  padding: 15px;
  border-radius: 8px;
  margin-bottom: 25px;
}
.info p {
  margin: 8px 0;
}
button {
  width: 100%;
  padding: 14px;
  font-size: 18px;
  border: none;
  border-radius: 6px;
  cursor: pointer;
}
</style>
</head>
<body>
<div class="container">
<h1>)rawliteral";

  html += success ? "Configuration saved" : "Configuration error";

  html += R"rawliteral(</h1>

<div class="info">
<p><strong>Group:</strong><br>
)rawliteral";

  html += joystickGroup;

  html += R"rawliteral(
</p>

<p><strong>OSC Address:</strong><br>
)rawliteral";

  html += oscAddress;

  html += R"rawliteral(
</p>

<p><strong>UDP Port:</strong><br>
)rawliteral";

  html += String(controlPort);

  html += R"rawliteral(
</p>

<p><strong>EWMA Alpha:</strong><br>
)rawliteral";

  html += String(alpha, 3);

  html += R"rawliteral(
</p>
</div>

<p>
The configuration has been stored in the ESP32 memory.
</p>

<p>
The settings will survive a reboot.
</p>

<form action="/" method="GET">
<button type="submit">Back to configuration</button>
</form>

</div>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// ============================================================
// JSON CONFIGURATION
// ============================================================

void handleConfig() {
  String json = "{";

  json += "\"mac\":\"";
  json += WiFi.macAddress();
  json += "\",";

  json += "\"ip\":\"";
  json += WiFi.localIP().toString();
  json += "\",";

  json += "\"group\":\"";
  json += joystickGroup;
  json += "\",";

  json += "\"address\":\"";
  json += oscAddress;
  json += "\",";

  json += "\"port\":";
  json += String(controlPort);
  json += ",";

  json += "\"alpha\":";
  json += String(alpha, 3);

  json += "}";

  server.send(200, "application/json", json);
}

// ============================================================
// WEB SERVER
// ============================================================

void startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/find", HTTP_POST, handleFind);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/config", HTTP_GET, handleConfig);

  server.begin();

  Serial.println("Web server iniciado");
  Serial.print("Configuration page: http://");
  Serial.println(WiFi.localIP());
  Serial.print("Configuration API: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/config");
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  delay(1000);
  Serial.begin(9600);

  Serial.println("////////////////////////////////");
  Serial.println("////////// JOYSTICK ////////////");
  Serial.println("////////////////////////////////");

  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledB, OUTPUT);
  setLed(false, false, false);

  loadConfiguration();

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

  Udp.begin(controlPort);
  discoveryUdp.begin(discoveryPort);

  Serial.print("Control UDP port: ");
  Serial.println(controlPort);

  Serial.print("Discovery UDP port: ");
  Serial.println(discoveryPort);

  macAddressStr = WiFi.macAddress();

  Serial.print("MAC ESP32: ");
  Serial.println(macAddressStr);

  startWebServer();

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

      if (mpu.begin())
        break;
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU6050 OK");

  Serial.print("Grupo actual: ");
  Serial.println(joystickGroup);

  Serial.print("OSC Address actual: ");
  Serial.println(oscAddress);

  Serial.print("UDP Port actual: ");
  Serial.println(controlPort);

  Serial.print("EWMA Alpha actual: ");
  Serial.println(alpha);

  setLed(false, true, false);
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  server.handleClient();

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
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    Udp.begin(controlPort);
    discoveryUdp.begin(discoveryPort);

    startWebServer();

    setLed(false, true, false);
  }

  receiveMessage();
  sendDiscovery();

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  wma_x = alpha * a.acceleration.x + (1 - alpha) * wma_x;
  wma_y = alpha * a.acceleration.y + (1 - alpha) * wma_y;
  wma_z = alpha * a.acceleration.z + (1 - alpha) * wma_z;

  wma_rol = alpha * g.gyro.x + (1 - alpha) * wma_rol;
  wma_pic = alpha * g.gyro.y + (1 - alpha) * wma_pic;
  wma_yaw = alpha * g.gyro.z + (1 - alpha) * wma_yaw;

  boton1State = !digitalRead(boton1Pin);
  boton2State = !digitalRead(boton2Pin);

  OSCMessage msg(oscAddress.c_str());

  msg.add(macAddressStr.c_str());
  msg.add(wma_x);
  msg.add(wma_y);
  msg.add(wma_z);
  msg.add(wma_rol);
  msg.add(wma_pic);
  msg.add(wma_yaw);
  msg.add((int32_t)boton1State);
  msg.add((int32_t)boton2State);

  Udp.beginPacket(outIp, controlPort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();

  delay(mseg_delay);
}