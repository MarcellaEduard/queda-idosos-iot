// ============================================================
// Sistema IoT de Detecao de Quedas para Idosos (ODS 3)
// Plataforma: ESP32 DevKit V1 + MPU6050
// Comunicacao: Wi-Fi + MQTT (broker.hivemq.com)
// Simulacao: Wokwi (https://wokwi.com)
// ============================================================

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// ---------- Wi-Fi ----------
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// ---------- MQTT ----------
const char* MQTT_BROKER   = "broker.hivemq.com";
const int   MQTT_PORT     = 1883;
const char* TOPIC_TELEM   = "ods3/queda/telemetria";
const char* TOPIC_EVENTO  = "ods3/queda/evento";
const char* TOPIC_STATUS  = "ods3/queda/status";
const char* TOPIC_COMANDO = "ods3/queda/comando";

// ---------- Pinos ----------
#define LED_PIN     2    // LED vermelho de alerta
#define BUZZER_PIN  4    // Buzzer ativo
#define BUTTON_PIN  5    // Botao de silenciar alarme

// ---------- Objetos ----------
WiFiClient espClient;
PubSubClient mqtt(espClient);
Adafruit_MPU6050 mpu;

// ---------- Estado ----------
bool alarmeAtivo      = false;
bool faseQuedaLivre   = false;
unsigned long quedaInicio      = 0;
unsigned long ultimoTelemetria = 0;

// ---------- Parametros de deteccao ----------
const float   LIMIAR_QUEDA_LIVRE    = 0.4f;   // g
const float   LIMIAR_IMPACTO        = 2.0f;   // g
const unsigned long DURACAO_QUEDA_MIN = 100;  // ms
const unsigned long JANELA_IMPACTO   = 500;   // ms
const unsigned long INTERVALO_TELEM  = 5000;  // ms

// ---------- Funcoes auxiliares ----------
void conectarWiFi() {
  Serial.print("Conectando ao Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Wi-Fi conectado. IP: ");
  Serial.println(WiFi.localIP());
}

void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.print("MQTT [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  if (String(topic) == TOPIC_COMANDO && msg == "silenciar") {
    alarmeAtivo = false;
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("Alarme silenciado via MQTT.");
  }
}

void conectarMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Conectando ao MQTT...");
    String clientId = "ESP32-Queda-" + String(random(0xffff), HEX);
    // Last Will and Testament (status offline)
    if (mqtt.connect(clientId.c_str(), nullptr, nullptr,
                     TOPIC_STATUS, 1, true, "offline")) {
      Serial.println(" conectado.");
      mqtt.publish(TOPIC_STATUS, "online", true);
      mqtt.subscribe(TOPIC_COMANDO);
    } else {
      Serial.print(" falha (rc=");
      Serial.print(mqtt.state());
      Serial.println("). Tentando em 5s...");
      delay(5000);
    }
  }
}

void publicarTelemetria(float svm, float ax, float ay, float az) {
  StaticJsonDocument<256> doc;
  doc["device_id"] = "esp32-queda-01";
  doc["svm"]       = round(svm * 100) / 100.0;
  doc["ax"]        = round(ax * 100) / 100.0;
  doc["ay"]        = round(ay * 100) / 100.0;
  doc["az"]        = round(az * 100) / 100.0;
  doc["estado"]    = alarmeAtivo ? "queda" : "normal";
  doc["rssi"]      = WiFi.RSSI();
  doc["ts"]        = millis();
  char buffer[256];
  serializeJson(doc, buffer);
  mqtt.publish(TOPIC_TELEM, buffer);
}

void publicarEvento(float svmMax) {
  StaticJsonDocument<256> doc;
  doc["device_id"] = "esp32-queda-01";
  doc["tipo"]      = "queda";
  doc["svm_max"]   = round(svmMax * 100) / 100.0;
  doc["ts"]        = millis();
  doc["local"]     = "sala";
  char buffer[256];
  serializeJson(doc, buffer);
  mqtt.publish(TOPIC_EVENTO, buffer, true);
  Serial.println("Evento de queda publicado via MQTT.");
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);

  if (!mpu.begin()) {
    Serial.println("Falha ao iniciar MPU6050. Verifique as conexoes I2C.");
    while (true) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("MPU6050 inicializado com sucesso.");

  conectarWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(callbackMQTT);
}

// ---------- Loop ----------
void loop() {
  if (!mqtt.connected()) conectarMQTT();
  mqtt.loop();

  // Botao fisico: silenciar alarme
  if (digitalRead(BUTTON_PIN) == LOW) {
    alarmeAtivo = false;
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("Alarme silenciado pelo botao.");
    delay(300);
  }

  // Leitura do acelerometro
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float ax  = a.acceleration.x / 9.81f;
  float ay  = a.acceleration.y / 9.81f;
  float az  = a.acceleration.z / 9.81f;
  float svm = sqrtf(ax * ax + ay * ay + az * az);

  unsigned long agora = millis();

  // ---- Algoritmo de deteccao de queda ----
  if (!alarmeAtivo) {
    if (svm < LIMIAR_QUEDA_LIVRE) {
      if (!faseQuedaLivre) {
        faseQuedaLivre = true;
        quedaInicio    = agora;
      }
    } else {
      if (faseQuedaLivre) {
        unsigned long duracao = agora - quedaInicio;
        if (duracao >= DURACAO_QUEDA_MIN && svm >= LIMIAR_IMPACTO) {
          alarmeAtivo = true;
          digitalWrite(LED_PIN, HIGH);
          digitalWrite(BUZZER_PIN, HIGH);
          publicarEvento(svm);
          Serial.println("!!! QUEDA DETECTADA !!!");
        }
        faseQuedaLivre = false;
      }
    }
    // Timeout da fase de queda livre
    if (faseQuedaLivre && (agora - quedaInicio > JANELA_IMPACTO)) {
      faseQuedaLivre = false;
    }
  } else {
    // Mantem buzzer ativo enquanto alarme nao for silenciado
    digitalWrite(BUZZER_PIN, HIGH);
  }

  // ---- Telemetria periodica ----
  if (agora - ultimoTelemetria >= INTERVALO_TELEM) {
    ultimoTelemetria = agora;
    publicarTelemetria(svm, ax, ay, az);
    Serial.printf("SVM=%.2f g  ax=%.2f ay=%.2f az=%.2f  estado=%s\n",
                  svm, ax, ay, az, alarmeAtivo ? "queda" : "normal");
  }

  delay(20); // ~50 Hz de amostragem
}
