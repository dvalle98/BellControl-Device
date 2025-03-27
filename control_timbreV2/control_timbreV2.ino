#include <WiFiManager.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <ArduinoJson.h>

// Configuración de logging
#define TAG "BellController"
#ifdef ARDUINO
#define APP_LOGE(format, ...) ESP_LOGE(TAG, format, ##__VA_ARGS__)
#define APP_LOGW(format, ...) ESP_LOGW(TAG, format, ##__VA_ARGS__)
#define APP_LOGI(format, ...) ESP_LOGI(TAG, format, ##__VA_ARGS__)
#endif

// Configuración de la aplicación
struct AppConfig {
  const char* mqttBroker;
  const int mqttPort;
  const char* mqttUser;
  const char* mqttPassword;
  const char* topicConfig;
  const char* topicControl;
  const char* topicStatus;
  const int wifiCheckInterval;
  const int maxReconnectAttempts;
  const int buzzerPin;
  const int relayPIN;
  const int WIFI_LED;
  const int MQTT_LED;
  const int ERROR_LED;
  const int Button_AP;
};

const AppConfig config PROGMEM = {
  "18.212.130.131",          // mqttBroker
  1883,                      // mqttPort
  "test",                    // mqttUser
  "CloudTech*",              // mqttPassword
  "bellcontrol/v1/config",   // topicConfig
  "bellcontrol/v1/control",  // topicControl
  "bellcontrol/v1/status",   // topicStatus
  3000,                      // wifiCheckInterval (ms)
  5,                         // maxReconnectAttempts
  15,                        // buzzerPin
  4,                         // relay pin
  27,
  32,
  14,
  25
};

enum class SystemState {
  BELL_INITIALIZING,
  BELL_WIFI_CONNECTED,
  BELL_WIFI_DISCONNECTED,
  BELL_MQTT_CONNECTED,
  BELL_MQTT_DISCONNECTED,
  BELL_CONFIG_PORTAL_ACTIVE
};

// Variables globales protegidas
volatile SystemState currentState = SystemState::BELL_INITIALIZING;
WiFiManager wm;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
QueueHandle_t mqttQueue = NULL;
SemaphoreHandle_t Bell_systemState = NULL;
SemaphoreHandle_t mqttMutex = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t mqttTaskHandle = NULL;
bool wm_nonblocking = false;  // Change to true to use non-blocking mode
struct MQTTMessage {
  char topic[64];
  char payload[2256];
};

// Prototipos de funciones
void updateSystemState(SystemState newState);
void startConfigPortal();
void safeDelay(uint32_t ms);
bool initHardware();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void sendMqttResponse(const char* topic, const char* message);

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando sistema...");

  if (!initHardware()) {
    APP_LOGE(TAG, "Fallo inicialización hardware");
    ESP.restart();
  }

  // Inicialización WiFiManager
  WiFi.mode(WIFI_STA);
  // Set WiFiManager to non-blocking mode if required
  if (wm_nonblocking) wm.setConfigPortalBlocking(false);
  wm.setConfigPortalBlocking(false);
  wm.setClass("invert");
  wm.setConnectTimeout(30);
  wm.setConfigPortalTimeout(120);  // Set configuration portal timeout to 40 seconds
  // Start WiFiManager and try to connect to WiFi
  if (!wm.autoConnect("BellControl-AP")) {  // Start an anonymous access point named "Besafe_mini"
    Serial.println(F("Failed to connect or hit timeout"));
  }

  // Configuración MQTT
  mqttClient.setServer(config.mqttBroker, config.mqttPort);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024);

  // Creación de objetos FreeRTOS
  Bell_systemState = xSemaphoreCreateMutex();
  if (Bell_systemState == NULL) {
    APP_LOGE(TAG, "Error creando mutex");
    ESP.restart();
  }

  mqttMutex = xSemaphoreCreateMutex();
  if (mqttMutex == NULL) {
    APP_LOGE(TAG, "Error creando mutex");
    ESP.restart();
  }

  mqttQueue = xQueueCreate(10, sizeof(struct MQTTMessage));
  if (mqttQueue == NULL) {
    APP_LOGE(TAG, "Error creando cola MQTT");
    ESP.restart();
  }

  // Creación de tareas
  BaseType_t taskStatus;

  taskStatus = xTaskCreatePinnedToCore(
    wifiTask,
    "WiFiManager",
    4096,
    NULL,
    tskIDLE_PRIORITY + 2,
    &wifiTaskHandle,
    0);

  if (taskStatus != pdPASS) {
    APP_LOGE(TAG, "Error creando tarea WiFi (Código: %d)", taskStatus);
    ESP.restart();
  }

  taskStatus = xTaskCreatePinnedToCore(
    mqttTask,
    "MQTTProcessor",
    6144,  // Stack mayor para procesamiento JSON
    NULL,
    tskIDLE_PRIORITY + 3,
    &mqttTaskHandle,
    1);

  if (taskStatus != pdPASS) {
    APP_LOGE(TAG, "Error creando tarea MQTT (Código: %d)", taskStatus);
    ESP.restart();
  }

  updateSystemState(SystemState::BELL_INITIALIZING);
  Serial.println("Inicialización completa");
}

void loop() {
  mqttClient.loop();

  if (wm.getConfigPortalActive()) {
    wm.process();
  }

  safeDelay(100);
}

// Implementación de funciones principales --------------------------------------------------

bool initHardware() {
  pinMode(config.buzzerPin, OUTPUT);
  digitalWrite(config.buzzerPin, LOW);
  return true;
}

void wifiTask(void* pvParameters) {
  uint8_t reconnectAttempts = 0;
  TickType_t xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      updateSystemState(SystemState::BELL_WIFI_DISCONNECTED);

      if (reconnectAttempts >= config.maxReconnectAttempts) {
        APP_LOGW(TAG, "Máximos intentos de conexión alcanzados");
        startConfigPortal();
        reconnectAttempts = 0;
      } else {
        APP_LOGI(TAG, "Intentando conexión WiFi (%d/%d)",
                 reconnectAttempts + 1, config.maxReconnectAttempts);

        WiFi.disconnect(true);
        WiFi.reconnect();
        reconnectAttempts++;
      }
    } else {
      if (currentState != SystemState::BELL_WIFI_CONNECTED) {
        APP_LOGI(TAG, "WiFi conectado: %s", WiFi.localIP().toString().c_str());
        updateSystemState(SystemState::BELL_WIFI_CONNECTED);
        reconnectAttempts = 0;
      }
    }

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(config.wifiCheckInterval));
  }
}

void mqttTask(void* pvParameters) {
  for (;;) {

    vTaskDelay(pdMS_TO_TICKS(4000));  // Wait for 10 seconds before checking again

    if (!mqttClient.connected()) {

      Serial.print(F("Attempting MQTT connection..."));
      char nameCliente[50];
      snprintf(nameCliente, sizeof(nameCliente), "BellControl(%s)", WiFi.macAddress().c_str());

      if (mqttClient.connect(nameCliente, config.mqttUser, config.mqttPassword)) {
        Serial.println(F("connected"));
        mqttClient.subscribe(config.topicConfig);   // Subscribe to the response topic after connecting
        mqttClient.subscribe(config.topicControl);  // Subscribe to the response topic after connecting
      } else {
        Serial.print(F("failed, rc="));
        Serial.print(mqttClient.state());
        Serial.println(F(" try again in 10 seconds"));
      }
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Mensaje recibido, tamaño: %d \n", length);

  char message[length + 1];  // Buffer local para el mensaje
  memcpy(message, payload, length);
  message[length] = '\0';  // Terminar con null-terminator

  Serial.printf("Tópico: %s\nContenido: %s\n", topic, message);

  // Crear el objeto JSON
  StaticJsonDocument<2048> doc; // Ajusta el tamaño según el JSON recibido
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.printf("Error al parsear JSON: %s\n", error.c_str());
    return;
  }

  // Extraer los horarios de cada día
  const char* dias[] = {"L", "M", "X", "J", "V"};
  for (int i = 0; i < 5; i++) {
    const char* dia = dias[i];
    Serial.printf("Horarios para %s: ", dia);

    if (doc["s"][dia].isNull()) {
      Serial.println("No hay horarios");
      continue;
    }

    JsonArray horarios = doc["s"][dia].as<JsonArray>();
    for (const char* hora : horarios) {
      Serial.printf("%s ", hora);
    }
    Serial.println();
  }
}


void activateBell(int durationMs) {
  digitalWrite(config.relayPIN, HIGH);
  safeDelay(durationMs);
  digitalWrite(config.relayPIN, LOW);

  char response[64];
  snprintf(response, sizeof(response),
           "{\"status\":\"OK\",\"action\":\"bell\",\"duration\":%d}",
           durationMs);
  sendMqttResponse(config.topicStatus, response);
}

void testBuzzer(int durationMs) {
  digitalWrite(config.buzzerPin, HIGH);
  safeDelay(durationMs);
  digitalWrite(config.buzzerPin, LOW);

  char response[64];
  snprintf(response, sizeof(response),
           "{\"status\":\"OK\",\"action\":\"bell\",\"duration\":%d}",
           durationMs);
  sendMqttResponse(config.topicStatus, response);
}

void sendMqttResponse(const char* topic, const char* message) {
  if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(250))) {
    if (mqttClient.publish(topic, message)) {
      APP_LOGI(TAG, "Mensaje publicado en %s", topic);
    } else {
      APP_LOGW(TAG, "Error publicando en %s", topic);
    }
    xSemaphoreGive(mqttMutex);
  }
}

void updateSystemState(SystemState newState) {
  static SystemState previousState = SystemState::BELL_INITIALIZING;

  if (xSemaphoreTake(Bell_systemState, pdMS_TO_TICKS(250))) {
    if (newState != previousState) {
      previousState = currentState;
      currentState = newState;
      APP_LOGI(TAG, "Cambio de estado: %d -> %d",
               (int)previousState, (int)newState);
    }
    xSemaphoreGive(Bell_systemState);
  }
}

void startConfigPortal() {
  updateSystemState(SystemState::BELL_CONFIG_PORTAL_ACTIVE);
  APP_LOGI(TAG, "Iniciando portal de configuración");

  if (!wm.startConfigPortal("BellControl-AP")) {
    APP_LOGE(TAG, "Error en portal de configuración");
    ESP.restart();
  }

  updateSystemState(SystemState::BELL_WIFI_CONNECTED);
}

void safeDelay(uint32_t ms) {
  const TickType_t xDelay = pdMS_TO_TICKS(ms);
  TickType_t xLastWakeTime = xTaskGetTickCount();
  vTaskDelayUntil(&xLastWakeTime, xDelay);
}