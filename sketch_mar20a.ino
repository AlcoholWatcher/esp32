#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "SeSAC GANGNAM";      // Wi-Fi SSID
const char* password = "sesac01234"; // Wi-Fi 비밀번호

const char* serverUrl = "http://192.168.100.181:8000/sensor-data/";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi Connected!");
  Serial.print("📡 IP Address: ");
  Serial.println(WiFi.localIP());

  delay(5000); // Wi-Fi 안정화를 위해 5초 대기
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(serverUrl);  // 다른 PC의 IP 주소로 HTTP 요청 전송
    http.addHeader("Content-Type", "application/json");  // JSON 형식으로 전송

    // JSON 데이터 생성
    String jsonData = "{\"temperature\": 25.5, \"humidity\": 60}";

    Serial.println("📤 Sending HTTP POST request to remote server...");
    int httpResponseCode = http.POST(jsonData);  // POST 요청 실행

    if (httpResponseCode > 0) {
      Serial.print("✅ Server Response: ");
      Serial.println(http.getString());  // 서버 응답 출력
    } else {
      Serial.print("❌ POST request failed. Error code: ");
      Serial.println(httpResponseCode);
    }

    http.end();  // 요청 종료
  } else {
    Serial.println("❌ WiFi Disconnected. Reconnecting...");
    WiFi.begin(ssid, password);
  }

  delay(10000);  // 10초마다 데이터 전송
}
