#include <WiFiManager.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "config.h"

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando sistema...");

  if (!initHardware()) {
    Serial.println("Fallo inicialización hardware");
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
  BellSystemState = xSemaphoreCreateMutex();
  if (BellSystemState == NULL) {
    Serial.println("Error creando mutex");
    ESP.restart();
  }

  mqttSendMessage = xSemaphoreCreateMutex();
  if (mqttSendMessage == NULL) {
    Serial.printf("Error creando mutex");
    ESP.restart();
  }

  mqttQueue = xQueueCreate(5, sizeof(MQTTMessage*));  // Cola de punteros
  if (mqttQueue == NULL) {
    Serial.println("Error creando cola MQTT");
    ESP.restart();
  }

  updateSystemState(SystemState::BELL_INITIALIZING);

  // Creación de tareas
  BaseType_t taskStatus;

  taskStatus = xTaskCreatePinnedToCore(
    wifiTask,
    "WiFiManager",
    8192,
    NULL,
    5,
    &wifiTaskHandle,
    0);

  if (taskStatus != pdPASS) {
    Serial.printf("Error creando tarea WiFi (Código: %d)", taskStatus);
    ESP.restart();
  }

  // Crear tarea procesadora de mensajes MQTT
  taskStatus = xTaskCreatePinnedToCore(
    mqttProcessorTask,
    "MQTT Processor",
    8192,  // Stack grande para JSON complejos
    NULL,
    4,  // Prioridad alta
    &mqttProcessorTaskHandle,
    0  // Core 0
  );

  if (taskStatus != pdPASS) {
    Serial.printf("Error creando tarea MQTT (Código: %d)", taskStatus);
    ESP.restart();
  }

  // Crear tarea monitorear la conexion mqtt
  taskStatus = xTaskCreatePinnedToCore(
    MQTTTask,
    "MQTTTask",
    8192,  // Stack mayor para procesamiento JSON
    NULL,
    3,
    &mqttTaskHandle,
    1);

  if (taskStatus != pdPASS) {
    Serial.printf("Error creando tarea MQTT (Código: %d)", taskStatus);
    ESP.restart();
  }

  // Crear tarea para manejo de tiempo
  taskStatus = xTaskCreatePinnedToCore(
    timeTask,
    "TimeTask",
    4096,
    NULL,
    2,
    &timeTaskHandle,
    1);

  if (taskStatus != pdPASS) {
    Serial.printf("Error creando tarea MQTT (Código: %d)", taskStatus);
    ESP.restart();
  }

  Serial.println("Inicialización completa");
}

void loop() {
  mqttClient.loop();

  if (wm_nonblocking) wm.process();

  safeDelay(100);
}

//------------------------------------------------------------------
// ********* Implementación de funciones principales *********
//------------------------------------------------------------------


bool initHardware() {
  pinMode(config.Button_AP, INPUT_PULLUP);

  pinMode(config.buzzerPin, OUTPUT);
  digitalWrite(config.buzzerPin, LOW);

  pinMode(config.relayPIN, OUTPUT);
  digitalWrite(config.relayPIN, LOW);

  pinMode(config.WIFI_LED, OUTPUT);
  digitalWrite(config.WIFI_LED, LOW);  // Turn off LED (WIFI IS NOT CONNECTED)

  pinMode(config.MQTT_LED, OUTPUT);
  digitalWrite(config.MQTT_LED, LOW);  // Turn off MQTT LED (MQTT IS NOT CONNECTED)

  pinMode(config.ERROR_LED, OUTPUT);
  digitalWrite(config.ERROR_LED, LOW);  // Turn off MQTT LED (MQTT IS NOT CONNECTED)

  return true;
}

/* ******************* TAREAS ******************* */
void wifiTask(void* pvParameters) {
  uint8_t reconnectAttempts = 0;
  TickType_t xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(config.wifiCheckInterval));

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi Desconectado");
      updateSystemState(SystemState::BELL_WIFI_DISCONNECTED);

      if (reconnectAttempts >= config.maxReconnectAttempts) {
        Serial.println("Máximos intentos de conexión alcanzados");
        startConfigPortal();
        reconnectAttempts = 0;
      } else {
        Serial.printf("Intentando conexión WiFi (%d/%d)",
                      reconnectAttempts + 1, config.maxReconnectAttempts);

        WiFi.disconnect(true);
        WiFi.reconnect();
        reconnectAttempts++;
      }
    } else {
      if (currentState != SystemState::BELL_WIFI_CONNECTED) {
        Serial.println("WiFi conectado");
        updateSystemState(SystemState::BELL_WIFI_CONNECTED);
        reconnectAttempts = 0;
      }
    }
  }
}
void MQTTTask(void* parameter) {
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
void timeTask(void* parameter) {
  const TickType_t xFrequency = pdMS_TO_TICKS(5000);
  TickType_t xLastWakeTime = xTaskGetTickCount();
  bool timeSynced = false;

  for (;;) {
    // 1. Verificar estado WiFi de forma segura
    SystemState currentStateCopy;
    if (xSemaphoreTake(BellSystemState, pdMS_TO_TICKS(100))) {
      currentStateCopy = currentState;
      xSemaphoreGive(BellSystemState);
    } else {
      currentStateCopy = SystemState::BELL_WIFI_DISCONNECTED;
    }

    // 2. Solo actuar si WiFi está conectado
    if (currentStateCopy == SystemState::BELL_WIFI_CONNECTED) {
      if (!timeSynced) {
        if (timeClient.forceUpdate()) {
          Serial.println("Hora sincronizada via NTP");
          timeSynced = true;
        } else {
          Serial.println("Error sincronizando hora");
        }
      }

      // Actualizar y mostrar hora
      timeClient.update();
      Serial.println(timeClient.getFormattedTime());

    } else {
      timeSynced = false;  // Resetear bandera si se pierde conexión
      Serial.println("Esperando conexión WiFi para sincronizar hora...");
    }

    // 3. Esperar hasta el próximo ciclo de manera segura
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}
void mqttProcessorTask(void* parameter) {
  MQTTMessage* receivedMsg;

  for (;;) {
    if (xQueueReceive(mqttQueue, &receivedMsg, portMAX_DELAY)) {
      // Procesar el mensaje
      Serial.println("dato recibido en cola");
      processMessage(receivedMsg);

      // Liberar memoria después de procesar
      delete[] receivedMsg->topic;
      delete[] receivedMsg->payload;
      delete receivedMsg;
    }
  }
}
/* ******************* FIN TAREAS ******************* */

//------------------------------------------------------------------

/* ******************* RECEPCIÓN PROCESADO Y ENVIO DE MENSAJES MQTT ******************* */
void mqttCallback(char* topic, byte* payload, unsigned int length) {

  MQTTMessage* msg = new MQTTMessage;  // Asigna memoria para el objeto

  // Copiar tópico
  msg->topic = new char[strlen(topic) + 1];  // Reserva memoria para topic
  strcpy(msg->topic, topic);

  // Copiar payload
  msg->payload_len = length;
  msg->payload = new char[length + 1];  // Reserva memoria para payload
  memcpy(msg->payload, payload, length);
  msg->payload[length] = '\0';

  if (xQueueSend(mqttQueue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
    delete[] msg->topic;    // Libera memoria del topic si no se encola
    delete[] msg->payload;  // Libera memoria del payload si no se encola
    delete msg;             // Libera la estructura si no se encola
  }
}
void processMessage(MQTTMessage* msg) {

  JsonDocument doc;

  // Parsear JSON
  DeserializationError error = deserializeJson(doc, msg->payload, msg->payload_len);
  if (error) {
    Serial.printf("Error parsing JSON: %s\n", error.c_str());
    return;
  }
  Serial.println("dato recibido en processMessage");
  // Ejemplo: Procesar tu payload de configuración
  if (strcmp(msg->topic, config.topicConfig) == 0) {
    JsonObject schedules = doc["s"];

    // Iterar días de la semana
    const char* days[] = { "L", "M", "X", "J", "V" };
    for (const char* day : days) {
      JsonArray times = schedules[day];
      for (JsonVariant time : times) {
        Serial.printf("Hora programada %s: %s\n", day, time.as<const char*>());
      }
    }
  }

  // Aquí añadirías más lógica para otros tópicos
}
void sendMqttResponse(const char* topic, const char* message) {
  if (xSemaphoreTake(mqttSendMessage, pdMS_TO_TICKS(250))) {
    if (mqttClient.publish(topic, message)) {
      Serial.printf("Mensaje publicado en %s", topic);
    } else {
      Serial.printf("Error publicando en %s", topic);
    }
    xSemaphoreGive(mqttSendMessage);
  }
}
/* ******************* FIN RECEPCIÓN PROCESADO Y ENVIO DE MENSAJES MQTT ******************* */

//------------------------------------------------------------------

/* ******************* CONTROL DE HARDWARE ******************* */
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
/* ******************* FIN CONTROL DE HARDWARE ******************* */

//------------------------------------------------------------------


void updateSystemState(SystemState newState) {
  static SystemState previousState = SystemState::BELL_INITIALIZING;

  if (xSemaphoreTake(BellSystemState, pdMS_TO_TICKS(250))) {
    if (newState != previousState) {
      previousState = currentState;
      currentState = newState;
    }
    xSemaphoreGive(BellSystemState);
  }
}

void startConfigPortal() {
  updateSystemState(SystemState::BELL_CONFIG_PORTAL_ACTIVE);
  Serial.println("Iniciando portal de configuración");

  wm.setConfigPortalTimeout(120);  // Set config portal timeout

  if (!wm.startConfigPortal("BellControl-AP")) {
    Serial.printf("Error en portal de configuración");
    ESP.restart();
  }

  updateSystemState(SystemState::BELL_WIFI_CONNECTED);
}

void safeDelay(uint32_t ms) {
  const TickType_t xDelay = pdMS_TO_TICKS(ms);
  TickType_t xLastWakeTime = xTaskGetTickCount();
  vTaskDelayUntil(&xLastWakeTime, xDelay);
}