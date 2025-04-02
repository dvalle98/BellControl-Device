// Configuración de la aplicación
struct AppConfig {
  const char* mqttBroker;
  const int mqttPort;
  const char* mqttUser;
  const char* mqttPassword;
  const char* topicConfigSchedule;
  const char* topicConfigSchedule_reply;
  const char* topicRemoteControl;
  const char* topicRemoteControl_reply;
  const char* topicRequestSchedule;
  const char* topicRequestSchedule_reply;
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
  "18.212.130.131",                        // mqttBroker
  1883,                                    // mqttPort
  "test",                                  // mqttUser
  "CloudTech*",                            // mqttPassword
  "bellcontrol/v1/configSchedule",         // topicConfig
  "bellcontrol/v1/configSchedule_reply",   // topicConfig to reply
  "bellcontrol/v1/remoteControl",          // topicControl
  "bellcontrol/v1/remoteControl_reply",    // topicControl to reply
  "bellcontrol/v1/requestSchedule",        // topicControl
  "bellcontrol/v1/requestSchedule_reply",  // topicControl to reply
  "bellcontrol/v1/status",                 // topicStatus
  5000,                                    // wifiCheckInterval (ms)
  5,                                       // maxReconnectAttempts
  15,                                      // buzzerPin
  4,                                       // relay pin
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
// Estructura para almacenar mensajes MQTT
struct MQTTMessage {
  char* topic = nullptr;
  char* payload = nullptr;
  size_t payload_len = 0;
};

//------------------------------------------------------------------
// Variables globales
//------------------------------------------------------------------
const bool wm_nonblocking = false;  // Change to true to use non-blocking mode
volatile SystemState currentState = SystemState::BELL_INITIALIZING;
WiFiManager wm;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -18000, 10000);  // Offset de -5 horas (18000 segundos)
QueueHandle_t mqttQueue = NULL;
SemaphoreHandle_t BellSystemState = NULL;
SemaphoreHandle_t mqttSendMessage = NULL;
SemaphoreHandle_t serialMutex = NULL;
SemaphoreHandle_t nvsMutex = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t timeTaskHandle = NULL;
TaskHandle_t mqttProcessorTaskHandle = NULL;

//------------------------------------------------------------------
// Prototipos de funciones
//------------------------------------------------------------------
bool initHardware();
void keepWifiTask(void* pvParameters);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void keepMQTTTask(void* parameter);
void activateBell(int durationMs);
void activateBuzzer(int durationMs);
void sendMqttResponse(const char* topic, const char* message);
void updateSystemState(SystemState newState);
void startConfigPortal();
void safeSerialPrint(const char* format, ...);
void safeDelay(uint32_t ms);