# BellControl-Device

BellControl-Device es un sistema basado en ESP32 diseñado para el control automatizado de una campana escolar. Permite programar y gestionar los horarios de activación mediante una interfaz web intuitiva y comandos MQTT, asegurando una administración eficiente y remota del sistema.

## Características principales

- **Automatización completa** del funcionamiento de la campana según horarios programados.

- **Interfaz web para** 
  configurar horarios y cargar los almacenados en el dispositivo,
  accionar remotamente la campana con tiempo ajustable o un valor predeterminado de 2 segundos,
  visualizar respuestas del dispositivo. 

- **Conectividad MQTT** utilizada como medio de comunicación entre la interfaz web y el dispositivo para la administración y control.

- **Almacenamiento persistente** de la configuración en la memoria NVS del ESP32.

## Tecnologías utilizadas

- **Hardware:** ESP32

- **Firmware:** Arduino (ESP-IDF/Arduino framework)

- **Protocolos:** MQTT, WiFi

- **Interfaz Web:** HTML, CSS, JavaScript (TailwindCSS)

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


| Tópico                              | Descripción                            | Dirección de Comunicación            |
|-------------------------------------|----------------------------------------|--------------------------------------|
| `bellcontrol/v1/configSchedule`     | Configuración de horarios              | Plataforma → Dispositivo            |
| `bellcontrol/v1/configSchedule_reply` | Respuesta a configuración de horarios  | Dispositivo → Plataforma            |
| `bellcontrol/v1/remoteControl`      | Accionar campana                       | Plataforma → Dispositivo            |
| `bellcontrol/v1/remoteControl_reply` | Respuesta a accionar campana           | Dispositivo → Plataforma            |
| `bellcontrol/v1/requestSchedule`    | Obtener configuración de horarios      | Plataforma → Dispositivo            |
| `bellcontrol/v1/requestSchedule_reply` | Respuesta a obtener configuración de horarios | Dispositivo → Plataforma |
| `bellcontrol/v1/status`             | Publica el estado del dispositivo      | Dispositivo → Plataforma            |


### Ejemplo de payload JSON para configuración:
```json
{
  "serialNo": "6w8keif5g6",
  "uuid": "1117QZ",
  "data": {
    "schedule": {
      "L": [
        "07:15",
        "08:00",
        "08:45",
        "09:00",
        "09:15",
        "10:00",
        "10:45",
        "11:00",
        "11:45",
        "12:30"
      ],
      "M": [
        "07:15",
        "08:00",
        "08:45",
        "09:00",
        "09:15",
        "10:00",
        "10:45",
        "11:00",
        "11:45",
        "12:30"
      ],
      "X": [
        "07:15",
        "08:00",
        "08:45",
        "09:00",
        "09:15",
        "10:00",
        "10:45",
        "11:00",
        "11:45",
        "12:30"
      ],
      "J": [
        "07:15",
        "08:00",
        "08:45",
        "09:00",
        "09:15",
        "10:00",
        "10:45",
        "11:00",
        "11:45",
        "12:30"
      ],
      "V": [
        "07:15",
        "08:00",
        "08:45",
        "09:00",
        "09:15",
        "10:00",
        "10:45",
        "11:00",
        "11:45",
        "12:30"
      ]
    }
  }
}
```

## Instalación y Uso

1. Clonar el repositorio
```sh
   git clone https://github.com/dvalle98/BellControl-Device.git
   ```

2. Cargar el firmware en el ESP32

- Abre el código BellControl.ino en Arduino IDE o VS Code con PlatformIO.

- Conecta el ESP32 y sube el firmware.

3. Configuración del WiFi

- No es necesario configurar credenciales WiFi en el código.

- El dispositivo usa WiFiManager para la configuración de red.

- Al encenderse por primera vez (o si no tiene conexión WiFi), el ESP32 creará una red llamada "BellControl-AP".

- Conéctate a esta red desde un móvil o PC y abre un navegador en 192.168.4.1 para ingresar los datos de la red WiFi.

4. Configuración de Horarios

- Accede a la interfaz web cuando el dispositivo esté conectado a la red WiFi y MQTT.

- Ingresa los horarios en los que debe sonar la campana.

- Guarda la configuración para que se almacene en la memoria del dispositivo.

### Los horarios se guardarán en la memoria NVS del ESP32 y serán usados para activar la campana en los momentos programados.

## Contribución

Si deseas contribuir, crea un **issue** o envía un **pull request** con mejoras al proyecto.

## Licencia

Este proyecto se distribuye bajo la licencia **MIT**. Consulta el archivo `LICENSE` para más detalles.

