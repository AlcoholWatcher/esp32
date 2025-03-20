const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Web Server</title>
</head>
<body>
    <h1>ESP32 LED Control</h1>
    <button onclick="toggleLED(1, 'on')">LED 1 ON</button>
    <button onclick="toggleLED(1, 'off')">LED 1 OFF</button>
    <br><br>
    <button onclick="toggleLED(2, 'on')">LED 2 ON</button>
    <button onclick="toggleLED(2, 'off')">LED 2 OFF</button>

    <script>
        function toggleLED(led, state) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/?led" + led + "=" + state, true);
            xhr.send();
        }
    </script>
</body>
</html>
)rawliteral";  