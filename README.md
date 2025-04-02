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
├── BellControl/         # Código fuente del firmware
│   ├── BellControl.ino  # Programa principal
│   ├── config.h         # Configuración del dispositivo
├  # Archivos de la interfaz web
│── index.html           # Panel de configuración
│── script.js            # Lógica de la interfaz
│── styles.caa           # Estilos de la interfaz
├── README.md            # Documentación
```

## Configuración MQTT

El dispositivo se comunica mediante MQTT usando los siguientes tópicos:

| Tópico                      | Descripción                           | Descripción                           |
|------------------------------|---------------------------------------| Plataforma -> Dispositivo            |
| `bellcontrol/v1/configSchedule`      | Configuración de horarios  | Plataforma -> Dispositivo            |
| `bellcontrol/v1/configSchedule_reply`     | Respuesta a configuración de horario     | Dispositivo -> Plataforma             |
| `bellcontrol/v1/remoteControl`      | Accionar campana    | Plataforma -> Dispositivo            |
| `bellcontrol/v1/remoteControl_reply`      | Respuesta a accionar campana  | Dispositivo -> Plataforma             |
| `bellcontrol/v1/requestSchedule`     | Obtener configuración de horarios     | Plataforma -> Dispositivo            |
| `bellcontrol/v1/requestSchedule_reply`     | Respuesta a Obtener configuración de horarios     | Dispositivo -> Plataforma             |
| `bellcontrol/v1/status`      | Publica el estado del dispositivo    | Dispositivo -> Plataforma            |

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

