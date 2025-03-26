#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi 설정
const char* ssid = "SeSAC GANGNAM";
const char* password = "sesac01234";

// 서버 설정
const char* serverUrl = "http://192.168.100.181:8000/sensor/post/";
const char* csrfTokenUrl = "http://192.168.100.181:8000/sensor/get-csrf-token/";

String csrfToken = "";
String uartData = ""; // UART로 받은 데이터를 저장할 변수
unsigned long lastSendTime = 0; // 마지막으로 데이터를 보낸 시간

// 센서 데이터 구조체
struct SensorData {
  int alcohol;
  int gyro;
  int motorSpeed;
  bool isValid;
};

SensorData currentSensorData = {0, 0, 0, false};

void handleResponse(HTTPClient& http, int httpResponseCode);
bool isNumeric(String str); 
void readUartData();
void sendDataToServer();
SensorData parseSensorData(String data);
String extractNumberAfter(String data, String keyword);
// CSRF 토큰을 받아오는 함수
void getCsrfToken() {
  HTTPClient http;
  http.begin(csrfTokenUrl);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String response = http.getString();
    int tokenStart = response.indexOf("\"csrf_token\":\"") + 14;
    int tokenEnd = response.indexOf("\"", tokenStart);
    csrfToken = response.substring(tokenStart, tokenEnd);
    Serial.println("CSRF Token: " + csrfToken);
  } else {
    Serial.print("CSRF 토큰 요청 실패, 에러 코드: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // UART2 초기화
  
  delay(1000); // 시리얼 통신 안정화 대기
  
  // WiFi 연결
  WiFi.begin(ssid, password);
  Serial.println("WiFi 연결 중...");
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi 연결 성공!");
    Serial.print("IP 주소: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi 연결 실패. 재부팅합니다...");
    ESP.restart();
  }

  // CSRF 토큰 받아오기
  getCsrfToken();
  
  // 초기 시간 설정
  lastSendTime = millis();
}

void loop() {
  // UART에서 데이터 읽기
  readUartData();
  
  // 현재 시간 확인
  unsigned long currentTime = millis();
  
  // WiFi 연결 상태 확인
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi 연결 끊김. 재연결 시도...");
    WiFi.reconnect();
    delay(1000);
    return;
  }
  
  // 5초마다 데이터 전송
  if (currentTime - lastSendTime >= 1000) {
    // 데이터 전송
    sendDataToServer();
    
    // 타이머 업데이트
    lastSendTime = currentTime;
    
    Serial.println("5초 후 다시 전송됩니다...");
    Serial.println(WiFi.localIP());
  }
  
  // 루프 지연
  delay(100);
}

// UART에서 데이터 읽기
void readUartData() {
  String tempData = "";
  bool newData = false;
  
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n' || c == '\r') {
      // 줄바꿈이나 캐리지 리턴이 오면 데이터 처리 준비 완료
      if(tempData.length() > 0) {
        Serial.print("원시 UART 데이터: [");
        Serial.print(tempData);
        Serial.println("]");
        
        uartData = tempData; // 유효한 데이터만 저장
        
        // 새로운 데이터 파싱
        currentSensorData = parseSensorData(uartData);
        
        tempData = ""; // 임시 버퍼 초기화
      }
    } else {
      tempData += c; // 수신된 문자를 임시 버퍼에 추가
    }
  }
}

// 센서 데이터 파싱 함수 - 실제 수신 형식에 맞게 수정
SensorData parseSensorData(String data) {
  SensorData result = {0, 0, 0, false};
  
  Serial.println("파싱 시작: " + data);
  
  // "Alcohol:%u, Gyro:%u, Motor_speed:%u" 형식인 경우 숫자 추출
  
  // 숫자만 추출하는 방식으로 변경
  int values[3] = {0, 0, 0}; // alcohol, gyro, motorSpeed
  int valueIndex = 0;
  int startPos = 0;
  
  // 숫자 부분 찾기
  for (int i = 0; i < data.length() && valueIndex < 3; i++) {
    if (isDigit(data.charAt(i))) {
      // 숫자의 시작 위치 찾기
      startPos = i;
      
      // 숫자 끝 위치 찾기
      while (i < data.length() && isDigit(data.charAt(i))) {
        i++;
      }
      
      // 숫자 추출 및 변환
      if (i > startPos) {
        String numStr = data.substring(startPos, i);
        values[valueIndex] = numStr.toInt();
        valueIndex++;
        Serial.println("숫자 발견: " + numStr + " = " + String(values[valueIndex-1]));
      }
    }
  }
  
  // 세 개의 숫자를 모두 찾았는지 확인
  if (valueIndex == 3) {
    result.alcohol = values[0];
    result.gyro = values[1];
    result.motorSpeed = values[2];
    result.isValid = true;
    
    Serial.println("파싱 성공 - 알코올: " + String(result.alcohol) + 
                  ", 자이로: " + String(result.gyro) + 
                  ", 모터 속도: " + String(result.motorSpeed));
  } else {
    // 특정 키워드로 구분하는 대체 방법 시도
    int alcoholIdx = data.indexOf("Alcohol:");
    int gyroIdx = data.indexOf("Gyro:");
    int motorIdx = data.indexOf("Motor_speed:");
    
    if (alcoholIdx >= 0 && gyroIdx >= 0 && motorIdx >= 0) {
      // 각 값 추출을 시도
      String alcoholStr = extractNumberAfter(data, "Alcohol:");
      String gyroStr = extractNumberAfter(data, "Gyro:");
      String motorStr = extractNumberAfter(data, "Motor_speed:");
      
      if (alcoholStr.length() > 0 && gyroStr.length() > 0 && motorStr.length() > 0) {
        result.alcohol = alcoholStr.toInt();
        result.gyro = gyroStr.toInt();
        result.motorSpeed = motorStr.toInt();
        result.isValid = true;
        
        Serial.println("대체 파싱 성공 - 알코올: " + String(result.alcohol) + 
                      ", 자이로: " + String(result.gyro) + 
                      ", 모터 속도: " + String(result.motorSpeed));
      } else {
        Serial.println("숫자 추출 실패!");
      }
    } else {
      Serial.println("키워드 찾기 실패!");
    }
  }
  
  return result;
}

// 특정 키워드 이후의 숫자를 추출하는 함수
String extractNumberAfter(String data, String keyword) {
  int startIdx = data.indexOf(keyword);
  if (startIdx < 0) return "";
  
  startIdx += keyword.length();
  
  // 숫자 시작 위치 찾기
  while (startIdx < data.length() && !isDigit(data.charAt(startIdx))) {
    startIdx++;
  }
  
  if (startIdx >= data.length()) return "";
  
  // 숫자 끝 위치 찾기
  int endIdx = startIdx;
  while (endIdx < data.length() && isDigit(data.charAt(endIdx))) {
    endIdx++;
  }
  
  return data.substring(startIdx, endIdx);
}

// 서버에 데이터 전송
void sendDataToServer() {
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  http.addHeader("X-CSRFToken", csrfToken);
  http.setTimeout(1000); // 5초 타임아웃 설정
  
  // JSON 객체 생성
  DynamicJsonDocument doc(1024);
  
  if (currentSensorData.isValid) {
    // 유효한 센서 데이터가 있는 경우
    doc["device"] = "STM32_Sensor";
    doc["alcohol"] = currentSensorData.alcohol;
    doc["gyro"] = currentSensorData.gyro;
    doc["motor_speed"] = currentSensorData.motorSpeed;
  } else {
    // 유효한 센서 데이터가 없는 경우
    doc["device"] = "STM32_Sensor";
    doc["alcohol"] = 0;
    doc["gyro"] = 0;
    doc["motor_speed"] = 0;
    doc["status"] = "no_valid_data";
    
    Serial.println("유효한 센서 데이터 없음. 기본값 사용");
  }
  
  doc["timestamp"] = millis();
  
  String message;
  serializeJson(doc, message);
  
  Serial.print("전송할 센서 데이터: ");
  Serial.println(message);
  
  // 데이터 전송
  Serial.println("HTTP POST 요청 전송 중...");
  int httpResponseCode = http.POST(message);
  Serial.println("HTTP 응답 코드: " + String(httpResponseCode));
  
  // 응답 처리
  handleResponse(http, httpResponseCode);
  
  http.end();
}

// HTTP 응답 처리 함수
void handleResponse(HTTPClient& http, int httpResponseCode) {
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("HTTP 응답 코드: " + String(httpResponseCode));
    Serial.println("응답: " + response);
  } else { 
    Serial.print("HTTP POST 요청 실패, 에러 코드: ");
    Serial.println(httpResponseCode);
    
    // 실패 원인 디버깅
    if (httpResponseCode == -1) {
      Serial.println("연결 실패 또는 전송 오류");
    } else if (httpResponseCode == -2) {
      Serial.println("서버 응답 타임아웃");
    } else if (httpResponseCode == -3) {
      Serial.println("연결 중단");
    } else if (httpResponseCode == -4) {
      Serial.println("연결 손실");
    } else if (httpResponseCode == -5) {
      Serial.println("전송 중 연결 끊김");
    } else if (httpResponseCode == -6) {
      Serial.println("헤더 설정 실패");
    } else if (httpResponseCode == -7) {
      Serial.println("페이로드 전송 실패");
    } else if (httpResponseCode == -8) {
      Serial.println("응답 타임아웃");
    }
  }
}

// 문자열이 숫자인지 확인하는 함수
bool isNumeric(String str) {
  if (str.length() == 0) return false;
  
  boolean isNum = false;
  boolean hasDot = false;
  
  for(unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (i == 0 && (c == '-' || c == '+')) {
      continue; // 첫 번째 문자가 + 또는 -인 경우 허용
    }
    
    if (c == '.' && !hasDot) {
      hasDot = true;
      continue; // 소수점은 한 번만 허용
    }
    
    if (!isDigit(c)) {
      return false;
    }
    
    isNum = true;
  }
  
  return isNum;
}
