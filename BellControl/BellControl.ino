#include <WiFiManager.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "freertos/semphr.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Preferences.h>
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
    Serial.println("Error creando BellSystemState");
    ESP.restart();
  }

  mqttSendMessage = xSemaphoreCreateMutex();
  if (mqttSendMessage == NULL) {
    Serial.printf("Error creando mqttSendMessage");
    ESP.restart();
  }

  serialMutex = xSemaphoreCreateMutex();
  if (serialMutex == NULL) {
    Serial.printf("Error creando serialMutex");
    ESP.restart();
  }

  nvsMutex = xSemaphoreCreateMutex();
  if (nvsMutex == NULL) {
    Serial.printf("Error creando serialMutex");
    ESP.restart();
  }

  mqttQueue = xQueueCreate(5, sizeof(MQTTMessage*));  // Cola de punteros
  if (mqttQueue == NULL) {
    Serial.println("Error creando mqttQueue");
    ESP.restart();
  }

  updateSystemState(SystemState::BELL_INITIALIZING);

  // Creación de tareas
  BaseType_t taskStatus;

  taskStatus = xTaskCreatePinnedToCore(
    keepWifiTask,
    "keepWifiTask",
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

  // Crear tarea para manejo de tiempo
  if (taskStatus != pdPASS) {
    Serial.printf("Error creando tarea MQTT (Código: %d)", taskStatus);
    ESP.restart();
  }

  taskStatus = xTaskCreatePinnedToCore(
    timeTask,
    "TimeTask",
    8192,
    NULL,
    3,
    &timeTaskHandle,
    1);

  if (taskStatus != pdPASS) {
    Serial.printf("Error creando tarea MQTT (Código: %d)", taskStatus);
    ESP.restart();
  }

  // Crear tarea monitorear la conexion mqtt
  taskStatus = xTaskCreatePinnedToCore(
    keepMQTTTask,
    "keepMQTTTask",
    4096,  // Stack mayor para procesamiento JSON
    NULL,
    2,
    &mqttTaskHandle,
    1);

  if (taskStatus != pdPASS) {
    Serial.printf("Error creando tarea MQTT (Código: %d)", taskStatus);
    ESP.restart();
  }
  safeSerialPrint("Inicialización completa");
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
void keepWifiTask(void* pvParameters) {
  uint8_t reconnectAttempts = 0;
  TickType_t xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(config.wifiCheckInterval));

    if (WiFi.status() != WL_CONNECTED) {
      safeSerialPrint("WiFi Desconectado");
      updateSystemState(SystemState::BELL_WIFI_DISCONNECTED);

      if (reconnectAttempts >= config.maxReconnectAttempts) {
        safeSerialPrint("Máximos intentos de conexión alcanzados");
        startConfigPortal();
        reconnectAttempts = 0;
      } else {
        safeSerialPrint("Intentando conexión WiFi (%d/%d)",
                        reconnectAttempts + 1, config.maxReconnectAttempts);

        //WiFi.disconnect(true);
        WiFi.reconnect();
        reconnectAttempts++;
      }
    } else {
      if (currentState != SystemState::BELL_WIFI_CONNECTED) {
        safeSerialPrint("WiFi conectado");
        updateSystemState(SystemState::BELL_WIFI_CONNECTED);
        reconnectAttempts = 0;
      }
    }
  }
}
void keepMQTTTask(void* parameter) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(10000));  // Wait for 10 seconds before checking again

    if (!mqttClient.connected()) {

      safeSerialPrint(F("Attempting MQTT connection..."));
      char nameCliente[50];
      snprintf(nameCliente, sizeof(nameCliente), "BellControl(%s)", WiFi.macAddress().c_str());

      if (mqttClient.connect(nameCliente, config.mqttUser, config.mqttPassword)) {
        safeSerialPrint(F("MQTT connected"));
        mqttClient.subscribe(config.topicConfigSchedule);  // Subscribe to the response topic after connecting
        mqttClient.subscribe(config.topicRemoteControl);   // Subscribe to the response topic after connecting
        mqttClient.subscribe(config.topicRequestSchedule);   // Subscribe to the response topic after connecting
      } else {
        safeSerialPrint(F("failed, rc="));
        safeSerialPrint(mqttClient.state());
        safeSerialPrint(F(" try again in 10 seconds"));
      }
    }
  }
}
void timeTask(void* parameter) {
  const TickType_t xFrequency = pdMS_TO_TICKS(10000);
  TickType_t xLastWakeTime = xTaskGetTickCount();
  bool timeSynced = false;

  for (;;) {
    SystemState currentStateCopy;
    if (xSemaphoreTake(BellSystemState, pdMS_TO_TICKS(100))) {
      currentStateCopy = currentState;
      xSemaphoreGive(BellSystemState);
    } else {
      currentStateCopy = SystemState::BELL_WIFI_DISCONNECTED;
    }

    if (currentStateCopy == SystemState::BELL_WIFI_CONNECTED) {
      if (!timeSynced) {
        if (timeClient.forceUpdate()) {
          safeSerialPrint("Hora sincronizada via NTP");
          timeSynced = true;
        } else {
          safeSerialPrint("Error sincronizando hora");
        }
      }

      timeClient.update();

      //Obtener la hora y el día actual
      String currentTime = timeClient.getFormattedTime().substring(0, 5);  // "HH:MM"
      int currentDay = timeClient.getDay();                                // 0 = Domingo, 1 = Lunes, ..., 6 = Sábado

      //Mapeo de día numérico a clave en JSON
      const char* dayKeys[] = { "", "L", "M", "X", "J", "V", "" };  // 0 y 6 (domingo/sábado) no tienen asignación
      if (currentDay < 1 || currentDay > 5) {
        safeSerialPrint("Hoy no hay horarios programados.");
      } else {
        const char* currentDayKey = dayKeys[currentDay];


        // TOMAR EL MUTEX ANTES DE LEER NVS
        if (xSemaphoreTake(nvsMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
          //Leer configuración desde NVS
          Preferences preferences;
          if (preferences.begin("configSchedule", true)) {
            String jsonSchedule = preferences.getString("jsonSchedule", "{}");
            preferences.end();

            // LIBERAR EL MUTEX DESPUÉS DE LEER
            xSemaphoreGive(nvsMutex);

            //Convertir a JSON
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, jsonSchedule);

            if (error) {
              safeSerialPrint("Error al leer JSON de NVS.");
            } else {
              JsonObject schedule = doc["data"]["schedule"];

              if (schedule[currentDayKey].is<JsonArray>()) {
                JsonArray daySchedule = schedule[currentDayKey];

                // Comparar la hora actual con los horarios guardados
                for (JsonVariant time : daySchedule) {
                  if (time.as<String>() == currentTime) {
                    safeSerialPrint("Activando campana: " + currentTime);
                    activateBuzzer(2000);  // Activar campana 2s
                    break;
                  }
                }
              }
            }
          } else {
            xSemaphoreGive(nvsMutex);
            safeSerialPrint("No se pudo abrir NVS para leer horarios.");
          }
        } else {
          safeSerialPrint("No se pudo tomar el semaforo en processMessage timeTask.");
        }
      }
    } else {
      timeSynced = false;
      safeSerialPrint("Esperando conexión WiFi para sincronizar hora...");
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}
void mqttProcessorTask(void* parameter) {
  MQTTMessage* receivedMsg;

  for (;;) {
    if (xQueueReceive(mqttQueue, &receivedMsg, portMAX_DELAY)) {
      // Procesar el mensaje
      safeSerialPrint("dato recibido en cola");
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
    safeSerialPrint("Error parsing JSON: %s\n", error.c_str());
    return;
  }
  // Verificar el tópico y llamar a la función correspondiente
  if (strcmp(msg->topic, config.topicConfigSchedule) == 0) {
    processConfigSchedule(doc);
  } else if (strcmp(msg->topic, config.topicRemoteControl) == 0) {
    processRemoteControl(doc);
  } else if (strcmp(msg->topic, config.topicRequestSchedule) == 0) {
    sendScheduleConfig();
  }
}
// Procesar configuración del horario
void processConfigSchedule(JsonDocument& doc) {
  if (!doc["data"].is<JsonObject>() || !doc["data"]["schedule"].is<JsonObject>()) {
    safeSerialPrint("Error: JSON inválido. Falta 'data' o 'schedule'.\n");
    sendMqttResponse(config.topicConfigSchedule_reply, R"({"status":"ERROR","message":"Falta 'data' o 'schedule'"})");
    return;
  }

  JsonObject schedules = doc["data"]["schedule"];
  const char* days[] = { "L", "M", "X", "J", "V" };
  bool hasErrors = false;

  safeSerialPrint("Configuración recibida:\n");

  for (const char* day : days) {
    if (!schedules[day].is<JsonArray>()) {
      safeSerialPrint("Advertencia: Falta el día %s en la configuración.\n", day);
      hasErrors = true;
      continue;
    }

    JsonArray times = schedules[day];
    safeSerialPrint("%s: ", day);

    if (times.size() == 0) {
      safeSerialPrint("Sin horarios\n");
      continue;
    }

    for (JsonVariant time : times) {
      if (!time.is<const char*>()) {
        safeSerialPrint("Error: Formato incorrecto en %s, debe ser una cadena de hora.\n", day);
        hasErrors = true;
        break;
      }
      safeSerialPrint("%s ", time.as<const char*>());
    }
    safeSerialPrint("\n");
  }

  if (hasErrors) {
    sendMqttResponse(config.topicConfigSchedule_reply, R"({"status":"ERROR","message":"Errores en la configuración"})");
    return;
  }

  // 🔹 TOMAR EL MUTEX ANTES DE ESCRIBIR EN NVS
  if (xSemaphoreTake(nvsMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
    // 🔹 Guardar en NVS
    Preferences preferences;
    if (!preferences.begin("configSchedule", false)) {
      xSemaphoreGive(nvsMutex);
      safeSerialPrint("Error: No se pudo abrir la NVS.");
      sendMqttResponse(config.topicConfigSchedule_reply, R"({"status":"ERROR","message":"No se pudo acceder a la NVS"})");
      return;
    }

    // 🔹 Convertimos el JSON a string
    String jsonString;
    serializeJson(doc, jsonString);

    // 🔹 Verificamos que la conversión sea válida
    if (jsonString.length() == 0) {
      preferences.end();
      xSemaphoreGive(nvsMutex);
      safeSerialPrint("Error: No se pudo convertir el JSON a cadena.");
      sendMqttResponse(config.topicConfigSchedule_reply, R"({"status":"ERROR","message":"Error al convertir JSON"})");

      return;
    }

    // Guardamos la configuración en la NVS
    size_t nvsSaved = preferences.putString("jsonSchedule", jsonString);
    preferences.end();  //Siempre cerramos NVS
    xSemaphoreGive(nvsMutex);

    if (nvsSaved > 0) {
      safeSerialPrint("Configuración guardada en NVS.");
      sendMqttResponse(config.topicConfigSchedule_reply, R"({"status":"OK","message":"Configuración guardada"})");
    } else {
      safeSerialPrint("Error: No se pudo guardar en NVS.");
      sendMqttResponse(config.topicConfigSchedule_reply, R"({"status":"ERROR","message":"Error al guardar en NVS"})");
    }
  } else {
    safeSerialPrint("No se pudo tomar el semaforo en processMessage.");
  }
}
// Procesar control de la campana
void processRemoteControl(JsonDocument& doc) {
  if (!doc["data"].is<JsonObject>()) {
    safeSerialPrint("Error: JSON inválido. Falta 'data' o no es un objeto.\n");
    sendMqttResponse(config.topicConfigSchedule_reply, R"({"status":"ERROR","message":"JSON inválido. Falta 'data' o no es un objeto."})");
    return;
  }

  JsonObject data = doc["data"];
  if (!data["action"].is<const char*>()) {
    safeSerialPrint("Error: 'action' faltante o no es una cadena de texto.\n");
    sendMqttResponse(config.topicConfigSchedule_reply, R"({"status":"ERROR","message":"JSON inválido. 'action' faltante o no es una cadena de texto."})");
    return;
  }

  const char* action = data["action"];
  if (strcmp(action, "ring") == 0) {
    int duration = data["duration"].is<int>() ? data["duration"].as<int>() : 2000;
    activateBuzzer(duration);
  }
}
void sendScheduleConfig() {
  String jsonSchedule = "{}";  // Valor por defecto
  bool success = false;

  // Intentar tomar el mutex antes de leer la NVS
  if (xSemaphoreTake(nvsMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    Preferences preferences;
    if (preferences.begin("configSchedule", true)) {
      jsonSchedule = preferences.getString("jsonSchedule", "{}");
      preferences.end();
      success = true;  // Indica que la lectura fue exitosa
    } else {
      safeSerialPrint("Error: No se pudo abrir la NVS en sendScheduleConfig.");
    }
    xSemaphoreGive(nvsMutex);
  } else {
    safeSerialPrint("Error: No se pudo obtener acceso a la NVS en sendScheduleConfig.");
  }

  // Enviar la respuesta MQTT
  if (success) {
    sendMqttResponse(config.topicRequestSchedule_reply, jsonSchedule.c_str());
  } else {
    sendMqttResponse(config.topicRequestSchedule_reply, R"({"status":"ERROR","message":"No se pudo leer la configuración"})");
  }
}
void sendMqttResponse(const char* topic, const char* message) {
  if (xSemaphoreTake(mqttSendMessage, pdMS_TO_TICKS(250))) {
    if (mqttClient.publish(topic, message)) {
      safeSerialPrint("Mensaje publicado en %s", topic);
    } else {
      safeSerialPrint("Error publicando en %s", topic);
    }
    xSemaphoreGive(mqttSendMessage);
  }
}
/* ******************* FIN RECEPCIÓN PROCESADO Y ENVIO DE MENSAJES MQTT ******************* */

//------------------------------------------------------------------

/* ******************* CONTROL DE HARDWARE ******************* */
void activateBell(int durationMs) {
  char response[64];
  snprintf(response, sizeof(response),
           "{\"status\":\"OK\",\"action\":\"bell\",\"duration\":%d}",
           durationMs);
  sendMqttResponse(config.topicRemoteControl_reply, response);

  digitalWrite(config.relayPIN, HIGH);
  safeDelay(durationMs);
  digitalWrite(config.relayPIN, LOW);
}
void activateBuzzer(int durationMs) {
  char response[64];
  snprintf(response, sizeof(response),
           "{\"status\":\"OK\",\"action\":\"bell\",\"duration\":%d}",
           durationMs);
  sendMqttResponse(config.topicRemoteControl_reply, response);
  digitalWrite(config.buzzerPin, HIGH);
  safeDelay(durationMs);
  digitalWrite(config.buzzerPin, LOW);
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
  safeSerialPrint("Iniciando portal de configuración");

  wm.setConfigPortalTimeout(120);  // Set config portal timeout

  if (!wm.startConfigPortal("BellControl-AP")) {
    safeSerialPrint("Error en portal de configuración");
    ESP.restart();
  }

  updateSystemState(SystemState::BELL_WIFI_CONNECTED);
}
void safeSerialPrint(const char* format, ...) {
  char buffer[128];  // Ajusta el tamaño según sea necesario

  va_list args;
  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (len > 0) {
    buffer[len] = '\n';  // Agregar salto de línea
    buffer[len + 1] = '\0';
    // Intentar tomar el semáforo antes de escribir
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
      Serial.write((uint8_t*)buffer, len + 1);
      xSemaphoreGive(serialMutex);  // Liberar semáforo
    }
  }
}
void safeSerialPrint(int num) {
  safeSerialPrint("%d", num);
}
void safeSerialPrint(const String& str) {
  String output = str + "\n";  // Agregar salto de línea

  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.write((uint8_t*)output.c_str(), output.length());
    xSemaphoreGive(serialMutex);
  }
}
void safeDelay(uint32_t ms) {
  const TickType_t xDelay = pdMS_TO_TICKS(ms);
  TickType_t xLastWakeTime = xTaskGetTickCount();
  vTaskDelayUntil(&xLastWakeTime, xDelay);
}