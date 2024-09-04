// Basic demo for accelerometer readings from Adafruit MPU6050

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <WiFiUdp.h>
#include <WiFi.h>


////////////////////////////////
///////////WIFI SETUP///////////
////////////////////////////////
// Replace with your network credentials

IPAddress staticIP(10, 1, 101, 171);     // Set your static IP address
IPAddress gateway(10, 1, 103, 254);        // Gateway IP address (usually your router's IP)
IPAddress subnet(255, 255, 252, 0);       // Subnet mask

// OSC server details
const IPAddress outIp(10,1,101,30);     // Set your static IP address
const unsigned int outPort = 9000;            // Port number of the OSC server
const unsigned int localPort = 8888;
//MACADDRESS = C8:F0:9E:53:07:34
char ssid[] = "LAB1507";
char pass[] = "7051BAL!";


WiFiUDP Udp;        // Create an instance of the UDP class
Adafruit_MPU6050 mpu;

////////////////////////////////
////////PIEZOELECTRICO//////////
////////////////////////////////

const int piezoIn = 34;
int piezoValue = 0;
bool piezoBool = false;
int umbral = 0;

float umbralToBool(int value, int umbral) {
 
  if (piezoValue >= umbral){
    
    return 1.0;
  } else {
    return 0.0;
  }
}

void setup(void) {
  Serial.begin(115200);

umbral = 20; // Umbral para el piezoelectrico

//Inicialización WIFI

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to Wi-Fi");

  // Start UDP
  Udp.begin(12345);  // Local port to listen on (can be any unused port)


////////////////////////////////
//////////// MPU6050 ///////////
////////////////////////////////

  //Inicialización Serial (para sensores)

  while (!Serial)
    delay(10); // will pause Zero, Leonardo, etc until serial console opens

  Serial.println("Adafruit MPU6050 test!");

  // Try to initialize!
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");

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


void loop() {

  //oscMsg.empty();  // Clear previous message
  //oscMsg.setAddress("/test/address");  // Set the OSC address 
  OSCMessage oscMsg("/test/address");  // Create an instance of the OSCMessage class

  //oscMsg.add(3.14f);                // Add a float argument
  

  // Send the OSC message
  
  piezoValue = analogRead(piezoIn);
  //Serial.println(piezoValue);
  piezoBool = umbralToBool (piezoValue, umbral);

  /* Get new sensor events with the readings */
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  oscMsg.add(a.acceleration.x).add(a.acceleration.y).add(a.acceleration.z).add(g.gyro.x).add(g.gyro.y).add(g.gyro.z).add(0.0f + piezoBool);                    // Add an integer argument
  
  Udp.beginPacket(outIp, outPort);
  oscMsg.send(Udp);
  Udp.endPacket();
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
  Serial.println(piezoBool);
  Serial.println("");
  oscMsg.empty();
  delay(5);
}