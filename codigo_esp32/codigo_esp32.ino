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

unsigned long lastUdpRestart = 0;
const unsigned long udpRestartInterval = 15000;

// ---------- LOOP RATE ----------
int mseg_delay = 20;

// ---------- OSC ADDRESSES ----------
const char addr_loopRate[] = "/loopRate";
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

// ---------- JOYSTICK GROUP ----------
String joystickGroup = "A";

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

const unsigned int outPort = 9000;
const unsigned int localPort = 8000;

////////////////////////////////
////////// INSTANCIAS //////////
////////////////////////////////

WiFiUDP Udp;
Adafruit_MPU6050 mpu;

////////////////////////////////
////////// FUNCIONES ///////////
////////////////////////////////

// ============================================================
// UDP REFRESH
// ============================================================

void refreshUDP() {
  if (millis() - lastUdpRestart > udpRestartInterval) {
    Serial.println("Refreshing UDP socket");
    Udp.stop();
    delay(10);
    Udp.begin(localPort);
    lastUdpRestart = millis();
  }
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
    while (size--) inmsg.fill(Udp.read());

    if (!inmsg.hasError()) {
      inmsg.dispatch(addr_loopRate, loopRate);
      inmsg.dispatch(addr_ewmaAlpha, ewmaAlpha);
    }
  }
}

// ============================================================
// LED UTILS
// ============================================================

void setLed(bool r, bool g, bool b) {
  digitalWrite(ledR, r ? LOW : HIGH);
  digitalWrite(ledG, g ? LOW : HIGH);
  digitalWrite(ledB, b ? LOW : HIGH);
}

// ============================================================
// FIND ROUTINE
// ============================================================

void findJoystick() {
  Serial.println("FIND activado");

  // ON
  setLed(false, true, false);
  delay(200);

  // OFF
  setLed(false, false, false);
  delay(200);

  // ON
  setLed(false, true, false);
  delay(200);

  // OFF
  setLed(false, false, false);
  delay(200);

  // ON final
  setLed(false, true, false);

  Serial.println("FIND terminado");

  // Volver a la interfaz principal
  server.sendHeader("Location", "/");
  server.send(303);
}

// ============================================================
// LOAD GROUP FROM MEMORY
// ============================================================

void loadJoystickGroup() {
  preferences.begin("joystick", false);
  joystickGroup = preferences.getString("group", "A");

  Serial.print("Grupo almacenado: ");
  Serial.println(joystickGroup);
}

// ============================================================
// SAVE GROUP TO MEMORY
// ============================================================

bool saveJoystickGroup(String newGroup) {
  if (newGroup.length() != 1) return false;

  char group = newGroup.charAt(0);

  if (group < 'A' || group > 'Z') return false;

  joystickGroup = newGroup;
  preferences.putString("group", joystickGroup);

  Serial.print("Nuevo grupo guardado: ");
  Serial.println(joystickGroup);

  return true;
}

// ============================================================
// SAVE EWMA TO MEMORY
// ============================================================

bool saveAlpha(float newAlpha) {
  if (newAlpha < 0.0 || newAlpha > 1.0) return false;

  alpha = newAlpha;
  preferences.putFloat("alpha", alpha);

  Serial.print("Nuevo EWMA Alpha guardado: ");
  Serial.println(alpha);

  return true;
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
    }

    button {
      width: 100%;
      padding: 14px;
      font-size: 18px;
      border: none;
      border-radius: 6px;
      cursor: pointer;
      margin-bottom: 10px;
    }
  </style>
</head>

<body>
<div class="container">

  <h1>Joystick Configuration</h1>

  <div class="info">
    <p>
      <strong>MAC:</strong><br>
)rawliteral";

  html += WiFi.macAddress();

  html += R"rawliteral(
    </p>

    <p>
      <strong>IP:</strong><br>
)rawliteral";

  html += WiFi.localIP().toString();

  html += R"rawliteral(
    </p>

    <p>
      <strong>Current Group:</strong><br>
)rawliteral";

  html += joystickGroup;

  html += R"rawliteral(
    </p>

    <p>
      <strong>EWMA Alpha:</strong><br>
)rawliteral";

  html += String(alpha, 3);

  html += R"rawliteral(
    </p>
  </div>

  <form action="/find" method="POST">
    <button type="submit">FIND</button>
  </form>

  <form action="/save" method="POST">

    <label for="group">
      Group
    </label>

    <input
      type="text"
      name="group"
      id="group"
      value=")rawliteral";

  html += joystickGroup;

  html += R"rawliteral("
      maxlength="1"
      pattern="[A-Za-z]"
      required
    >

    <label for="alpha">
      EWMA Alpha
    </label>

    <input
      type="number"
      name="alpha"
      id="alpha"
      value=")rawliteral";

  html += String(alpha, 3);

  html += R"rawliteral("
      min="0"
      max="1"
      step="0.01"
      required
    >

    <button type="submit">
      SAVE
    </button>

  </form>

</div>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// ============================================================
// SAVE WEB REQUEST
// ============================================================

void handleSave() {
  if (!server.hasArg("group") || !server.hasArg("alpha")) {
    server.send(400, "text/plain", "Missing configuration");
    return;
  }

  String newGroup = server.arg("group");
  newGroup.toUpperCase();

  float newAlpha = server.arg("alpha").toFloat();

  if (!saveJoystickGroup(newGroup)) {
    server.send(400, "text/plain", "Invalid group");
    return;
  }

  if (!saveAlpha(newAlpha)) {
    server.send(400, "text/plain", "Invalid EWMA alpha");
    return;
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

    a {
      display: block;
      width: 100%;
      box-sizing: border-box;
      padding: 14px;
      font-size: 18px;
      text-align: center;
      text-decoration: none;
      color: black;
      background: #eeeeee;
      border-radius: 6px;
    }
  </style>
</head>

<body>
<div class="container">

  <h1>Configuration Saved</h1>

  <div class="info">

    <p>
      <strong>Group:</strong><br>
)rawliteral";

  html += joystickGroup;

  html += R"rawliteral(
    </p>

    <p>
      <strong>EWMA Alpha:</strong><br>
)rawliteral";

  html += String(alpha, 3);

  html += R"rawliteral(
    </p>

    <p>
      The configuration has been stored in the ESP32 memory.
    </p>

    <p>
      The settings will survive a reboot.
    </p>

  </div>

  <a href="/">
    Back to configuration
  </a>

</div>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// ============================================================
// JSON CONFIGURATION ENDPOINT
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

  json += "\"alpha\":";
  json += String(alpha, 3);

  json += "}";

  server.send(200, "application/json", json);
}

// ============================================================
// START WEB SERVER
// ============================================================

void startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/find", HTTP_POST, findJoystick);
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

  // ----------------------------------------------------------
  // Load saved configuration
  // ----------------------------------------------------------

  loadJoystickGroup();
  alpha = preferences.getFloat("alpha", 0.2);

  Serial.println("Configuración almacenada:");
  Serial.print("Grupo: ");
  Serial.println(joystickGroup);
  Serial.print("EWMA Alpha: ");
  Serial.println(alpha);

  // ----------------------------------------------------------
  // WiFi
  // ----------------------------------------------------------

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

  // ----------------------------------------------------------
  // UDP
  // ----------------------------------------------------------

  Udp.begin(localPort);

  Serial.print("Puerto UDP: ");
  Serial.println(localPort);

  // ----------------------------------------------------------
  // MAC
  // ----------------------------------------------------------

  macAddressStr = WiFi.macAddress();

  Serial.print("MAC ESP32: ");
  Serial.println(macAddressStr);

  // ----------------------------------------------------------
  // WEB SERVER
  // ----------------------------------------------------------

  startWebServer();

  // ----------------------------------------------------------
  // BUTTONS
  // ----------------------------------------------------------

  pinMode(boton1Pin, INPUT_PULLUP);
  pinMode(boton2Pin, INPUT_PULLUP);

  // ----------------------------------------------------------
  // MPU6050
  // ----------------------------------------------------------

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

      if (mpu.begin()) break;
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU6050 OK");

  Serial.print("Grupo actual: ");
  Serial.println(joystickGroup);

  Serial.print("EWMA Alpha actual: ");
  Serial.println(alpha);

  setLed(false, true, false);
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  // ----------------------------------------------------------
  // WEB SERVER
  // ----------------------------------------------------------

  server.handleClient();

  // ----------------------------------------------------------
  // WIFI
  // ----------------------------------------------------------

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

    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());

    Serial.print("Subnet: ");
    Serial.println(WiFi.subnetMask());

    Udp.begin(localPort);

    Serial.print("Puerto UDP: ");
    Serial.println(localPort);

    Serial.print("MAC ESP32: ");
    Serial.println(WiFi.macAddress());

    startWebServer();

    setLed(false, true, false);
  }

  // ----------------------------------------------------------
  // OSC INPUT
  // ----------------------------------------------------------

  receiveMessage();

  // ----------------------------------------------------------
  // MPU6050
  // ----------------------------------------------------------

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // ----------------------------------------------------------
  // EWMA
  // ----------------------------------------------------------

  wma_x = alpha * a.acceleration.x + (1 - alpha) * wma_x;
  wma_y = alpha * a.acceleration.y + (1 - alpha) * wma_y;
  wma_z = alpha * a.acceleration.z + (1 - alpha) * wma_z;

  wma_rol = alpha * g.gyro.x + (1 - alpha) * wma_rol;
  wma_pic = alpha * g.gyro.y + (1 - alpha) * wma_pic;
  wma_yaw = alpha * g.gyro.z + (1 - alpha) * wma_yaw;

  // ----------------------------------------------------------
  // BUTTONS
  // ----------------------------------------------------------

  boton1State = !digitalRead(boton1Pin);
  boton2State = !digitalRead(boton2Pin);

  // ----------------------------------------------------------
  // OSC MESSAGE
  // ----------------------------------------------------------

  OSCMessage msg(oscAddress);

  // [0] MAC
  // [1] acceleration X
  // [2] acceleration Y
  // [3] acceleration Z
  // [4] gyro X
  // [5] gyro Y
  // [6] gyro Z
  // [7] button 1
  // [8] button 2

  msg.add(macAddressStr.c_str());

  msg.add(wma_x);
  msg.add(wma_y);
  msg.add(wma_z);

  msg.add(wma_rol);
  msg.add(wma_pic);
  msg.add(wma_yaw);

  msg.add((int32_t)boton1State);
  msg.add((int32_t)boton2State);

  // ----------------------------------------------------------
  // SEND OSC
  // ----------------------------------------------------------

  Udp.beginPacket(outIp, outPort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();

  // ----------------------------------------------------------
  // LOOP DELAY
  // ----------------------------------------------------------

  delay(mseg_delay);
}