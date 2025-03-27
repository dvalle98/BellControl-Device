# BellControl-Device

BellControl-Device es un sistema basado en ESP32 para el control automatizado de una campana escolar. Permite programar y gestionar los horarios de activación de la campana mediante MQTT y una interfaz web de configuración.

## Características principales

- **Control automatizado** de una campana escolar.
- **Interfaz web** para configurar los horarios de funcionamiento.
- **Conectividad MQTT** para el control remoto.
- **Almacenamiento de configuración** en memoria no volátil (NVS).
- **Accionamiento manual remoto** a través de comandos MQTT.

## Tecnologías utilizadas

- **Hardware**: ESP32
- **Firmware**: Arduino (ESP-IDF/Arduino framework)
- **Protocolos**: MQTT, WiFi
- **Interfaz Web**: HTML, CSS, JavaScript (TailwindCSS)

## Estructura del Proyecto

```
BellControl-Device/
├── src/                 # Código fuente del firmware
│   ├── main.cpp         # Programa principal
│   ├── mqtt_handler.cpp # Manejo de MQTT
│   ├── config.h         # Configuración del dispositivo
├── web/                 # Archivos de la interfaz web
│   ├── index.html       # Panel de configuración
│   ├── app.js          # Lógica de la interfaz
├── README.md            # Documentación
```

## Configuración MQTT

El dispositivo se comunica mediante MQTT usando los siguientes tópicos:

| Tópico                      | Descripción                           |
|------------------------------|---------------------------------------|
| `bellcontrol/v1/config`      | Recibe la configuración de horarios  |
| `bellcontrol/v1/control`     | Comando para accionar la campana     |
| `bellcontrol/v1/status`      | Publica el estado del dispositivo    |

### Ejemplo de payload JSON para configuración:
```json
{
  "s": {
    "L": [
      "06:00",
      "07:00",
      "08:00",
      "09:00",
      "10:00",
      "11:00",
      "13:00",
      "14:00",
      "15:00"
    ],
    "M": [
      "06:00",
      "07:00",
      "08:00",
      "09:00",
      "10:00",
      "11:00",
      "13:00",
      "14:00",
      "15:00"
    ],
    "X": [
      "06:00",
      "07:00",
      "08:00",
      "09:00",
      "10:00",
      "11:00",
      "13:00",
      "14:00",
      "15:00"
    ],
    "J": [
      "06:00",
      "07:00",
      "08:00",
      "09:00",
      "10:00",
      "11:00",
      "13:00",
      "14:00",
      "15:00"
    ],
    "V": [
      "06:00",
      "07:00",
      "08:00",
      "09:00",
      "10:00",
      "11:00",
      "13:00",
      "14:00",
      "15:00"
    ]
  }
}
```

## Instalación y Uso

1. Clonar el repositorio:
   ```sh
   git clone https://github.com/dvalle98/BellControl-Device.git
   ```
2. Abrir el código en Arduino IDE o VS Code con PlatformIO.
3. Configurar las credenciales de WiFi y el broker MQTT en `config.h`.
4. Compilar y cargar el firmware en el ESP32.
5. Acceder a la interfaz web y configurar los horarios.

## Contribución

Si deseas contribuir, crea un **issue** o envía un **pull request** con mejoras al proyecto.

## Licencia

Este proyecto se distribuye bajo la licencia **MIT**. Consulta el archivo `LICENSE` para más detalles.

