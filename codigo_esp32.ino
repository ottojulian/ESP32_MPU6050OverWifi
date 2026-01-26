#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <OSCMessage.h>
#include <WiFiUdp.h>
#include <WiFi.h>


// IMPORTANTE!!//
// Compilar usando el board ES32 Dev Module, sinó o no compila, o compila pero no conecta al wifi.
// Ahora si, el código:

////////////////////////////////
//////// INIT VARIABLES ////////
////////////////////////////////


// Loop Rate:
int mseg_delay = 10;

// EWMA:
int umbral = 20; //--> variable vieja, reivsar
float alpha = 0.2;
float wma_x = 0.0;
float wma_y = 0.0;
float wma_z = 0.0;
float wma_rol = 0.0;
float wma_pic = 0.0;
float wma_yaw = 0.0;

//OSC addresses:
const char addr_sensores[] = "/4/sensores";
const char addr_loopRate[] = "/4/loopRate";
const char addr_ewmaAlpha[] = "/4/ewmaAlpha";

////////////////////////////////
//////// NETWORK SETUP /////////
////////////////////////////////

// IP:
IPAddress staticIP(192, 168, 1, 102);  // IP local ### Me parece que no le da bola a esto
IPAddress gateway(192, 168, 1, 1);    // Gateway
IPAddress subnet(255, 255, 255, 0);   // Subnet mask

const IPAddress outIp(192, 168, 1, 255);  // IP destino ### Probar con 255 para broadcast

// UDP:
const unsigned int outPort = 9000;    // Puerto UPD destino
const unsigned int localPort = 8000;  // Puerto UPD destino

// Autenticación WIFI:

char ssid[] = "lowpoly99";
char pass[] = "lowpoly99";

/*
char ssid[] = "open-score";
char pass[] = "0p3n-5c0r3";


char ssid[] = "LAB1507";
char pass[] = "7051BAL!";
*/
////////////////////////////////
////////// INSTANCIAS //////////
////////////////////////////////

WiFiUDP Udp;
Adafruit_MPU6050 mpu;

////////////////////////////////
////////// FUNCIONES ///////////
////////////////////////////////

float umbralToBool(int value, int umbral) {
  /* Detecta si hubo golpe */
  if (value >= umbral) {

    return 1.0;
  } else {
    return 0.0;
  }
}

void loopRate(OSCMessage &msg) {
  /* Cambia los milisegundos del delay al final del loop */
  if (msg.isInt(0)) {
    if (msg.getInt(0) >= 5) {
      mseg_delay = msg.getInt(0);
      Serial.print("Nuevo Loop Rate: ");
      Serial.println(mseg_delay);
      Serial.print("");
    }
  }
}

void ewmaAlpha(OSCMessage &msg) {
  /* Calibra el alpha para el suavizado de los valores de acc y gyr */
  if (msg.isFloat(0)) {
    umbral = msg.getFloat(0);
    if (umbral >= 0 && umbral <= 1) {
      Serial.print("Nuevo Alpha: ");
      Serial.println(alpha);
      Serial.print("");
    } else {
      Serial.print("Valor alpha fuera de rango! (");
      Serial.print(umbral);
      Serial.println(")");
    }
  }
}

void receiveMessage() {
  /* Recive mensaje OSC */
  OSCMessage inmsg;  // Crea mensaje para recibir valores
  int size = Udp.parsePacket();
  if (size > 0) {
    while (size--) {
      inmsg.fill(Udp.read());
    }
    if (!inmsg.hasError()) {
      inmsg.dispatch(addr_loopRate, loopRate);
      inmsg.dispatch(addr_ewmaAlpha, ewmaAlpha);
    } else {
      auto error = inmsg.getError();
      Serial.print("ERROR en mensaje entrante: ");
      Serial.println(error);
      Serial.print("");
    }
  }
}

////////////////////////////////
//////////// SETUP /////////////
////////////////////////////////

void setup(void) {
  // Inicialización Serial:
  Serial.begin(115200);

  //Inicialización Serial (para sensores):
  /*
  while (!Serial) {
    delay(10);                    // Espera a que conecte el serial
  }
  */

  delay(1000);

  Serial.println("////////////////////////////////");
  Serial.println("////////// RAQUETA 2 ///////////");
  Serial.println("////////////////////////////////");

  //Inicialización WIFI:
  Serial.println("-Conectando a Wi-Fi-");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("status = ");
    Serial.println(WiFi.status());
  }

  Serial.print("Conectado a Wi-Fi");

  // Inicialización UDP:
  Udp.begin(localPort);

  Serial.print("UDP iniciado - escuchando en: ");
  Serial.print(WiFi.localIP());
  Serial.print(":");
  Serial.println(localPort);
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  ////////////////////////////////
  //////////// MPU6050 ///////////
  ////////////////////////////////

  // Inicialización de MPU6050:
  Serial.println("Adafruit MPU6050 test!");
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");

  // Setup del MPU6050: ### Hay que revisar esto
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.print("Accelerometer range set to: ");
  switch (mpu.getAccelerometerRange()) {
    case MPU6050_RANGE_2_G:
      Serial.println("+-2G");
      break;
    case MPU6050_RANGE_4_G:
      Serial.println("+-4G");
      break;
    case MPU6050_RANGE_8_G:
      Serial.println("+-8G");
      break;
    case MPU6050_RANGE_16_G:
      Serial.println("+-16G");
      break;
  }
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  Serial.print("Gyro range set to: ");
  switch (mpu.getGyroRange()) {
    case MPU6050_RANGE_250_DEG:
      Serial.println("+- 250 deg/s");
      break;
    case MPU6050_RANGE_500_DEG:
      Serial.println("+- 500 deg/s");
      break;
    case MPU6050_RANGE_1000_DEG:
      Serial.println("+- 1000 deg/s");
      break;
    case MPU6050_RANGE_2000_DEG:
      Serial.println("+- 2000 deg/s");
      break;
  }
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.print("Filter bandwidth set to: ");
  switch (mpu.getFilterBandwidth()) {
    case MPU6050_BAND_260_HZ:
      Serial.println("260 Hz");
      break;
    case MPU6050_BAND_184_HZ:
      Serial.println("184 Hz");
      break;
    case MPU6050_BAND_94_HZ:
      Serial.println("94 Hz");
      break;
    case MPU6050_BAND_44_HZ:
      Serial.println("44 Hz");
      break;
    case MPU6050_BAND_21_HZ:
      Serial.println("21 Hz");
      break;
    case MPU6050_BAND_10_HZ:
      Serial.println("10 Hz");
      break;
    case MPU6050_BAND_5_HZ:
      Serial.println("5 Hz");
      break;
  }

  Serial.println("");
  delay(100);
}

////////////////////////////////
//////////// LOOP //////////////
////////////////////////////////

void loop() {
  // Recibir mensajes OSC por UPD:
  receiveMessage();

  // Crear el mensaje OSC a enviar:
  OSCMessage oscMsg(addr_sensores);

  //// Acelerómetro y Giroscopio ////
  // Tomar los eventos de los sensores:
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // EWMA - calcular promedios:
  wma_x = alpha * a.acceleration.x + (1 - alpha) * wma_x;
  wma_y = alpha * a.acceleration.y + (1 - alpha) * wma_y;
  wma_z = alpha * a.acceleration.z + (1 - alpha) * wma_z;
  wma_rol = alpha * g.gyro.x + (1 - alpha) * wma_rol;
  wma_pic = alpha * g.gyro.y + (1 - alpha) * wma_pic;
  wma_yaw = alpha * g.gyro.z + (1 - alpha) * wma_yaw;

  //// Envío de mensaje OSC ////
  // Agregar valores al mensaje:
  //oscMsg.add(a.acceleration.x).add(a.acceleration.y).add(a.acceleration.z).add(g.gyro.x).add(g.gyro.y).add(g.gyro.z);   // versión sin smoothing
  oscMsg.add(wma_x).add(wma_y).add(wma_z).add(wma_rol).add(wma_pic).add(wma_yaw);

  // Enviar mensaje OSC por UDP:
  Udp.beginPacket(outIp, outPort);
  oscMsg.send(Udp);
  Udp.endPacket();
  oscMsg.empty();  // Vacía el mensaje para recibir los próximos valores

  ////////////////////////////////
  //////// NETWORK SETUP /////////
  ////////////////////////////////
  /*
  // Print out the values
  Serial.print("(x, y, z): ");
  Serial.print(a.acceleration.x);
  Serial.print(", ");
  Serial.print(a.acceleration.y);
  Serial.print(", ");
  Serial.print(a.acceleration.z);
  Serial.print(". ");

  Serial.print("(y, p, r): ");
  Serial.print(g.gyro.x);
  Serial.print(", ");
  Serial.print(g.gyro.y);
  Serial.print(", ");
  Serial.print(g.gyro.z);
  Serial.print(". ");

  //  Serial.print("Temperature: ");
  //  Serial.print(temp.temperature);
  //  Serial.println(" degC");
  */

  //// Regulación del rate del script ////
  delay(mseg_delay);
}
