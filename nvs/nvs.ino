#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <ArduinoJson.h>

const char *ssid = "YOJANA_VALLE";
const char *password = "19780513";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -18000, 10000);  // UTC -5

void setup() {
  pinMode(15, OUTPUT);
  digitalWrite(15, LOW);
  Serial.begin(115200);

  // Conectar a WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");

  // Iniciar NTP
  timeClient.begin();

  // Inicializar NVS
  if (nvs_flash_init() != ESP_OK) {
    Serial.println("Error iniciando NVS");
  }

  // Simular recepción de JSON y guardarlo en NVS (Solo una vez)
  //save_json_to_nvs();
}

void loop() {
  timeClient.update();

  // Obtener la hora actual
  String current_time = timeClient.getFormattedTime().substring(0, 5);  // HH:MM

  // Obtener día actual
  String weekdays[] = { "Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado" };
  String current_day = weekdays[timeClient.getDay()];

  Serial.printf("Hora: %s Dia: %s\n",current_time,current_day);
  // Leer JSON desde NVS y comparar
  String stored_json = read_json_from_nvs();
  if (stored_json.length() > 0) {
    if (check_bell_schedule(stored_json, current_day, current_time)) {
      Serial.println("⏰ ¡Hora de sonar la campana!");
      // Aquí puedes activar un relé o GPIO
    } else {
      Serial.println("No es hora de la campana aún.");
    }
  } else {
    Serial.println("No hay configuración guardada.");
  }

  delay(10000);  // Verificar cada minuto
}

// --- GUARDAR JSON EN NVS ---
void save_json_to_nvs() {
  const char *json_str = R"rawliteral(
    {
      "data": {
        "schedule": {
          "Lunes": ["10:56", "14:56"],
          "Martes": ["14:56","23:40"],
          "Miércoles": ["07:56"],
          "Jueves": [],
          "Viernes": []
        }
      }
    })rawliteral";

  nvs_handle_t my_handle;
  esp_err_t err = nvs_open("config", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    Serial.println("Error abriendo NVS");
    return;
  }

  err = nvs_set_str(my_handle, "schedule", json_str);
  if (err == ESP_OK) {
    nvs_commit(my_handle);
    Serial.println("Configuración guardada en NVS.");
  } else {
    Serial.println("Error al guardar configuración.");
  }

  nvs_close(my_handle);
}

// --- LEER JSON DESDE NVS ---
String read_json_from_nvs() {
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open("config", NVS_READONLY, &my_handle);
  if (err != ESP_OK) return "";

  size_t required_size = 0;
  err = nvs_get_str(my_handle, "schedule", NULL, &required_size);
  if (err != ESP_OK || required_size == 0) {
    nvs_close(my_handle);
    return "";
  }

  char *json_str = (char *)malloc(required_size);
  if (!json_str) {
    nvs_close(my_handle);
    return "";
  }

  err = nvs_get_str(my_handle, "schedule", json_str, &required_size);
  String result = (err == ESP_OK) ? String(json_str) : "";

  free(json_str);
  nvs_close(my_handle);

  return result;
}

// --- COMPARAR HORA ACTUAL CON JSON ---
bool check_bell_schedule(String json_str, String current_day, String current_time) {
  DynamicJsonDocument doc(512);
  
  DeserializationError error = deserializeJson(doc, json_str);
  if (error) {
    Serial.println("Error al parsear JSON");
    return false;
  }

  JsonObject schedule = doc["data"]["schedule"];
  JsonArray day_schedule = schedule[current_day];

  for (JsonVariant time_entry : day_schedule) {
    if (time_entry.as<String>() == current_time) {
      return true;
    }
  }

  return false;
}
