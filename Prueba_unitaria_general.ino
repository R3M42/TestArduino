#include <ArduinoUnit.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Configuración de los sensores
float calibration_value = 21.34;
int phval = 0;
unsigned long int avgval;
int buffer_arr[10], temp;

OneWire ourWire(4); 
DallasTemperature DS18B20(&ourWire);

const int sensorPin = 35; // Sensor de turbidez

// Configuración WiFi y MQTT
const char* ssid = "R3M";
const char* password = "ert34556";
const char* mqtt_server = "192.168.12.122";

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  Serial.begin(115200);
  Serial.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando al MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("Conectado.");
      client.subscribe("test/conexion"); // Suscripción de prueba
    } else {
      Serial.println("Reintentando en 5 seg...");
      delay(5000);
    }
  }
}

// Función para validar si MQTT realmente está activo
bool verificarConexionMQTT() {
  if (!client.connected()) {
    Serial.println("Error: El broker MQTT no está activo.");
    return false;
  }
  
  if (!client.subscribe("test/conexion")) {
    Serial.println("Fallo en suscripción MQTT, el broker podría no estar disponible.");
    return false;
  }
  
  Serial.println("MQTT activo y operando.");
  return true;
}

// TEST SUITE DE LOS SENSORES DE PH, TEMPERATURA Y TURBIDEZ °°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°
test(sensorTemperatura) {
  DS18B20.requestTemperatures();
  float temperatura = DS18B20.getTempCByIndex(0);
  Serial.print("Test Temperatura: ");
  Serial.println(temperatura);
  assertMore(temperatura, -40);
  assertLess(temperatura, 125);
}

test(sensorPH) {
  avgval = 0;
  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(35);
    delay(30);
  }
  
  for (int i = 2; i < 8; i++)
    avgval += buffer_arr[i];
  
  float volt = (float)avgval * 3.3 / 4095 / 6;
  float ph_act = -5.70 * volt + calibration_value;

  Serial.print("Test pH: ");
  Serial.println(ph_act);
  
  assertMoreOrEqual(ph_act, 0);
  assertLessOrEqual(ph_act, 14);
}

test(sensorTurbidez) {
  int valorDigital = analogRead(sensorPin);
  float voltajeTurbidez = (3.3 / 4095.0) * valorDigital;

  Serial.print("Test Turbidez: ");
  Serial.println(voltajeTurbidez);

  assertMoreOrEqual(voltajeTurbidez, 0);
  assertLessOrEqual(voltajeTurbidez, 3.3);

  if (valorDigital < 50) {
    Serial.println("¡Advertencia! El sensor de turbidez podría no estar conectado.");
  }
}
// TEST SUITE SOBRE EL PROTOCOLO MQTT °°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°
test(conexionMQTT) {
  assertTrue(verificarConexionMQTT(), "El broker MQTT no está activo.");
}

test(enviarDatosMQTT) {
  if (!verificarConexionMQTT()) {
    Serial.println("No se enviaron datos porque el broker MQTT no está activo.");
    return;
  }

  float temperatura = DS18B20.getTempCByIndex(0);
  float ph_act = -5.70 * ((analogRead(35) * 3.3 / 4095)) + calibration_value;
  int turbidez = analogRead(sensorPin);

  String payload = "{\"ph\":" + String(ph_act) + ",\"temperatura\":" + String(temperatura) + ",\"turbidez\":" + String(turbidez) + "}";
  
  if (client.publish("sensores/datos", payload.c_str())) {
    Serial.println("Datos enviados exitosamente.");
  } else {
    Serial.println("Error al enviar datos. El broker MQTT podría estar inactivo.");
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  DS18B20.begin();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  Test::run(); // Ejecutar pruebas cada 5 segundos

  // Lectura y cálculo de sensores
  avgval = 0;
  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(35);
    delay(30);
  }
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        temp = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp;
      }
    }
  }
  for (int i = 2; i < 8; i++)
    avgval += buffer_arr[i];
  
  float volt = (float)avgval * 3.3 / 4095 / 6;
  float ph_act = -5.70 * volt + calibration_value;
  DS18B20.requestTemperatures();
  float tem = DS18B20.getTempCByIndex(0);
  int valorDigital = analogRead(sensorPin);
  float voltajeTurbidez = (3.3 / 4095.0) * valorDigital;

  Serial.println("------------- Datos de Sensores y Pruebas Unitarias -------------");
  Serial.print("Voltaje pH: ");
  Serial.println(volt, 2);
  Serial.print("Valor de pH: ");
  Serial.println(ph_act, 2);
  Serial.print("Temperatura: ");
  Serial.print(tem);
  Serial.println(" °C");
  Serial.print("Voltaje Turbidez: ");
  Serial.println(voltajeTurbidez, 2);
  Serial.print("Valor digital Turbidez: ");
  Serial.println(valorDigital);
  Serial.println("---------------------------------------------------------------");

  // Enviar datos solo si MQTT está activo
  if (verificarConexionMQTT()) {
    String payload = String("{\"ph\":") + ph_act + ",\"temperatura\":" + tem + ",\"turbidez\":" + valorDigital + "}";
    client.publish("sensores/datos", payload.c_str());
  }

  delay(5000);
}
