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

    client.on("message", (topic, message) => {
        const responsesContainer = document.getElementById("responses");
        const responseItem = document.createElement("div");
        responseItem.classList.add("response-item");

        let parsedMessage;
        try {
            parsedMessage = JSON.parse(message.toString());
        } catch (error) {
            parsedMessage = { error: "Invalid JSON format" };
        }

        if (topic === "bellcontrol/v1/requestSchedule_reply") {
            responseItem.classList.add("success");
            responseItem.innerHTML = `<strong>RequestSchedule:</strong> ${JSON.stringify(parsedMessage, null, 2)}`;
            loadSchedule(parsedMessage.data.schedule); // Load the schedule into the UI
        } else if (topic === "bellcontrol/v1/configSchedule_reply") {
            responseItem.classList.add("success");
            responseItem.innerHTML = `<strong>ConfigSchedule:</strong> ${JSON.stringify(parsedMessage, null, 2)}`;
            loadSchedule(parsedMessage.data.schedule); // Load the schedule into the UI
        } else if (topic === "bellcontrol/v1/remoteControl_reply") {
            responseItem.classList.add("success");
            responseItem.innerHTML = `<strong>RemoteControl:</strong> ${JSON.stringify(parsedMessage, null, 2)}`;
        } else {
            responseItem.classList.add("error");
            responseItem.innerHTML = `<strong>Unknown Topic:</strong> ${topic}`;
        }

        responsesContainer.prepend(responseItem); // Add the response to the top
    });

    client.subscribe("bellcontrol/v1/configSchedule_reply", (err) => {
        if (err) {
            console.error("Error al suscribirse a ConfigSchedule_reply:", err);
        }
    });

    client.subscribe("bellcontrol/v1/remoteControl_reply", (err) => {
        if (err) {
            console.error("Error al suscribirse a remoteControl_reply:", err);
        }
    });

    client.subscribe("bellcontrol/v1/requestSchedule_reply", (err) => {
        if (err) {
            console.error("Error al suscribirse a requestSchedule_reply:", err);
        }
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

            const timeWrapper = document.createElement("div");
            timeWrapper.classList.add("time-wrapper", "flex", "items-center", "gap-2");

            const input = document.createElement("input");
            input.type = "time";
            input.classList.add("time-input");
            input.min = "06:00";
            input.max = "17:00";

            const deleteButton = document.createElement("button");
            deleteButton.innerHTML = `
                <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 text-red-600" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">
                    <path stroke-linecap="round" stroke-linejoin="round" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6M4 7h16M10 3h4m-4 0a1 1 0 00-1 1v1h6V4a1 1 0 00-1-1m-4 0h4" />
                </svg>`;
            deleteButton.classList.add("btn", "btn-danger", "delete-time");
            deleteButton.addEventListener("click", () => {
                timeWrapper.remove();
            });

            timeWrapper.appendChild(input);
            timeWrapper.appendChild(deleteButton);
            container.appendChild(timeWrapper);
        });
    });

    document.getElementById("ringBell").addEventListener("click", () => {
        const inputElement = document.getElementById("actuationTime");
        const actuationTime = inputElement.value ? parseInt(inputElement.value, 10) : null;

        if (actuationTime !== null && (isNaN(actuationTime) || actuationTime < 1 || actuationTime > 5)) {
            inputElement.setCustomValidity("Debe ser un número entre 1 y 5 segundos.");
            inputElement.reportValidity();
            return;
        }

        const actuationTimeMs = actuationTime !== null ? actuationTime * 1000 : undefined; // Convert to milliseconds if provided
        const payload = { action: "ring" };

        if (actuationTimeMs !== undefined) {
            payload.duration = actuationTimeMs;
        }

        console.log("Campana accionada con payload:", payload);
        sendMQTTMessage("bellcontrol/v1/remoteControl", payload);
    });

    document.getElementById("saveSchedule").addEventListener("click", () => {
        let scheduleData = {
            L: [],
            M: [],
            X: [],
            J: [],
            V: []
        };

        document.querySelectorAll(".times-container").forEach(container => {
            const day = container.getAttribute("data-day");
            const times = Array.from(container.querySelectorAll("input[type='time']")).map(input => input.value);

            // Map full day names to abbreviations
            const dayAbbreviations = {
                "Lunes": "L",
                "Martes": "M",
                "Miércoles": "X",
                "Jueves": "J",
                "Viernes": "V"
            };

            const dayAbbreviation = dayAbbreviations[day];
            if (dayAbbreviation) {
                scheduleData[dayAbbreviation] = times;
            }
        });

        const payload = {
                schedule: scheduleData
        };

        console.log("Horario guardado con payload:", payload);
        sendMQTTMessage("bellcontrol/v1/configSchedule", payload);
    });

    document.getElementById("loadSchedule").addEventListener("click", () => {
        const payload = {
            serialNo: "6w8keif5g6",
            uuid: "1117QZ"
        };

        console.log("Solicitando configuración de horario...");
        sendMQTTMessage("bellcontrol/v1/requestSchedule", payload);
    });

    function sendMQTTMessage(topic, data) {

            console.log(`Enviando mensaje a ${topic}`);
            client.publish(topic, JSON.stringify({ serialNo: "6w8keif5g6", uuid: "1117QZ", data }), { qos: 1 }, (err) =>{
                if(err) {
                    console.error('Error al enviar mensaje:', err);
                }
            });

    }

    function loadSchedule(schedule) {
        const dayAbbreviations = {
            L: "Lunes",
            M: "Martes",
            X: "Miércoles",
            J: "Jueves",
            V: "Viernes"
        };

        Object.keys(schedule).forEach(dayAbbreviation => {
            const dayName = dayAbbreviations[dayAbbreviation];
            const container = document.querySelector(`.times-container[data-day='${dayName}']`);

            if (container) {
                container.innerHTML = ""; // Clear existing times
                schedule[dayAbbreviation].forEach(time => {
                    const timeWrapper = document.createElement("div");
                    timeWrapper.classList.add("time-wrapper", "flex", "items-center", "gap-2");

                    const input = document.createElement("input");
                    input.type = "time";
                    input.classList.add("time-input");
                    input.value = time;

                    const deleteButton = document.createElement("button");
                    deleteButton.innerHTML = `
                        <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 text-red-600" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">
                            <path stroke-linecap="round" stroke-linejoin="round" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6M4 7h16M10 3h4m-4 0a1 1 0 00-1 1v1h6V4a1 1 0 00-1-1m-4 0h4" />
                        </svg>`;
                    deleteButton.classList.add("btn", "btn-danger", "delete-time");
                    deleteButton.addEventListener("click", () => {
                        timeWrapper.remove();
                    });

                    timeWrapper.appendChild(input);
                    timeWrapper.appendChild(deleteButton);
                    container.appendChild(timeWrapper);
                });
            }
        });
    }
});
