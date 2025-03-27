document.addEventListener("DOMContentLoaded", () => {

    // Configuración de conexión MQTT (ya existe en el código previo)
    const options = {
        clean: true,
        connectTimeout: 4000,
        clientId: `web_client_${Math.random().toString(16).substr(2, 8)}`,
        username: 'test', // Cambia esto por tus credenciales reales
        password: 'CloudTech*', // Cambia esto por tus credenciales reales
        keepalive: 60,
    };

    const connectUrl = 'wss://mqtt.cloudtechnologys.co:8087/mqtt';
    const client = mqtt.connect(connectUrl, options);

    client.on('connect', () => {
        console.log('conectado a mqtt: ', connectUrl);
    });
    client.on("error", (error) => {
        console.error("Error en MQTT:", error);
    });

    const days = ['Lunes', 'Martes', 'Miércoles', 'Jueves', 'Viernes'];
    const daysContainer = document.getElementById("days");

    days.forEach(day => {
        let dayDiv = document.createElement("div");
        dayDiv.classList.add("day-card");

        dayDiv.innerHTML = `
            <h3 class="text-lg font-semibold text-gray-700">${day}</h3>
            <div class="times-container" data-day="${day}"></div>
            <button class="btn btn-success add-time mt-3" data-day="${day}">Agregar Hora</button>
        `;

        daysContainer.appendChild(dayDiv);
    });

    document.querySelectorAll(".add-time").forEach(button => {
        button.addEventListener("click", (e) => {
            const day = e.target.getAttribute("data-day");
            const container = document.querySelector(`.times-container[data-day='${day}']`);

            const input = document.createElement("input");
            input.type = "time";
            input.classList.add("time-input");
            input.min = "06:00";
            input.max = "17:00";

            container.appendChild(input);
        });
    });

    document.getElementById("ringBell").addEventListener("click", () => {
        console.log("Campana accionada");
        sendMQTTMessage("bellcontrol/v1/control", { action: "ring" });
    });

    document.getElementById("saveSchedule").addEventListener("click", () => {
        let scheduleData = {};

        document.querySelectorAll(".times-container").forEach(container => {
            const day = container.getAttribute("data-day");
            const times = Array.from(container.querySelectorAll("input[type='time']")).map(input => input.value);
            scheduleData[day] = times;
        });

        console.log("Horario guardado:", scheduleData);
        sendMQTTMessage("bellcontrol/v1/config", { schedule: scheduleData });
    });

    function sendMQTTMessage(topic, data) {

            console.log(`Enviando mensaje a ${topic}`);
            client.publish(topic, JSON.stringify({ serialNo: "6w8keif5g6", uuid: "1117QZ", data }), { qos: 1 }, (err) =>{
                if(err) {
                    console.error('Error al enviar mensaje:', err);
                }
            });

    }
});
