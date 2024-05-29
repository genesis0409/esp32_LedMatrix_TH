// Active/DeActive Brownout problem
#define DISABLE_BROWNOUT_PROBLEM
#if defined(DISABLE_BROWNOUT_PROBLEM)
#include "soc/soc.h"          // Disable brownout problems
#include "soc/rtc_cntl_reg.h" // Disable brownout problems
#endif

#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_GFX.h>
#include <P3RGB64x32MatrixPanel.h>
#include <time.h>
#include <NTPClient.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/GodoM6pt8b.h>

#include "final_Celcius.h"
#include "final_temp_1px.h"
#include "final_humi_1px.h"

#include "Matrix_TranslateLED.h"

#include "SPIFFS.h" // Fast
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Search for parameter in HTTP POST request
const char *PARAM_INPUT_1 = "houseId";
const char *PARAM_INPUT_2 = "ssid";
const char *PARAM_INPUT_3 = "pass";

// Variables to save values from HTML form
String houseId;
String ssid;
String pass;

// File paths to save input values permanently
const char *houseIdPath = "/houseId.txt";
const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";

unsigned long currentMillis = 0;

// LED matrix 좌표, 상대간격
#define LED_X_FONT 9
#define LED_Y_FONT 9
#define LED_X_NUM 7
#define LED_Y_NUM 6

#define LED_X_TH 11
#define LED_Y_T 5

#define LED_X_VALUE 24
#define LED_Y_VALUE LED_Y_T + 1

#define LED_X_SPACE_Between_TH_VALUE 3
#define LED_Y_SPACE_BetweenTH 4
#define LED_Y_SPACE_Between_VALUE LED_Y_SPACE_BetweenTH + 3

int8_t matrix_index = 0;
int8_t led_x_translate = 0;
int8_t led_y_translate = 0;

void initSPIFFS();                                                 // Initialize SPIFFS
String readFile(fs::FS &fs, const char *path);                     // Read File from SPIFFS
void writeFile(fs::FS &fs, const char *path, const char *message); // Write file to SPIFFS
bool isWMConfigDefined();                                          // Is Wifi Manager Configuration Defiend?
bool allowsLoop = false;

String *Split(String sData, char cSeparator, int *scnt);
void initLED();
void initWiFi();
void PrintLED(String m0, String m1, String m2); // 센서값 출력함수
void PrintInvalidData();                        // 데이터 만료(일정 기간 미수신) 출력
void TranslateLedLogic();                       // led 패턴이동 로직
void printLocalTime();
void timeWork(void *para);

// Replace with your network credentials (STATION)
const char *ntpServer = "pool.ntp.org";
uint8_t timeZone = 9;
uint8_t summerTime = 0;

// **********************************************************************************************************
// const char *ssid = "itime";
// const char *pass = "daon7521";
// **********************************************************************************************************

P3RGB64x32MatrixPanel matrix(25, 26, 27, 21, 22, 0, 15, 32, 33, 12, 5, 23, 4);

unsigned long previousInvalidDataMillis = 0; // 데이터 무효화 타이머
unsigned long previousNoPacketMillis = 0;    // 패킷 수신 대기 타이머
unsigned long previousWiFiMillis = 0;
unsigned long interval = 30000; // wifi 재연결 주기
char packetBuffer[60];
int Year;
int Month;
int Day;
int Hour;
int Min;

// For Factory Reset (Erase Flash and Restart)
#define resetButton 13
int buttonState = HIGH;             // 버튼 상태 초기화
int lastButtonState = LOW;          // 이전 버튼 상태 초기화
unsigned long lastDebounceTime = 0; // 마지막 입력 디바운스 시간 초기화
unsigned long debounceDelay = 50;   // 디바운스 시간 설정 (50ms)
unsigned long pressStartTime = 0;   // 버튼을 누른 시작 시간 초기화
bool isResetBtnPressed = false;
bool isRunNextif1 = false;
bool isRunNextif2 = false;
bool isRunNextif3 = false;
bool isRunNextif4 = false;
bool isRunNextif5 = false;

int resetReading; // 리셋 버튼 상태 변수

// 센서 데이터 만료 시간
#define DATA_INVALID_TIME 600000 // 60만: 10분
// 패킷 수신대기 만료 시간
#define WATING_RCV_PACKET_TIME 1800000 // 60만: 10분

// Create UDP instance
WiFiUDP Udp;

TaskHandle_t thWork;
String formattedDate;

String *Split(String sData, char cSeparator, int *scnt)
{
  // 최대 분할 가능한 문자열 개수
  const int MAX_STRINGS = 20;

  // 문자열 배열 동적 할당
  static String charr[MAX_STRINGS];

  // 분할된 문자열의 개수를 저장할 변수
  int nCount = 0;

  // 임시 저장용 문자열
  String sTemp = "";

  // 복사할 문자열
  String sCopy = sData;

  // 문자열을 구분자를 기준으로 분할하여 배열에 저장
  while (true)
  {
    // 구분자를 찾음
    int nGetIndex = sCopy.indexOf(cSeparator);

    // 문자열을 찾지 못한 경우
    if (-1 == nGetIndex)
    {
      // 남은 문자열을 배열에 저장하고 반복문 종료
      charr[nCount++] = sCopy;
      break;
    }

    // 구분자를 찾은 경우
    // 구분자 이전까지의 문자열을 잘라서 배열에 저장
    charr[nCount++] = sCopy.substring(0, nGetIndex);

    // 다음 문자열을 탐색하기 위해 복사된 문자열을 잘라냄
    sCopy = sCopy.substring(nGetIndex + 1);

    // 배열이 가득 차면 반복문 종료
    if (nCount >= MAX_STRINGS)
    {
      Serial.println("Message is too long... You have loss data.");
      break;
    }
  }

  // 분할된 문자열의 개수를 scnt 포인터를 통해 반환
  *scnt = nCount;

  // 분할된 문자열 배열 반환
  return charr;
}

void initWiFi()
{
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("Connecting to WiFi ..");

  while (WiFi.status() != WL_CONNECTED)
  {
    // print "Connect WiFi"
    matrix.setCursor(6 + led_x_translate, 6 + led_y_translate);
    matrix.setTextColor(matrix.color444(255, 255, 255));
    matrix.print("Connect");

    matrix.setCursor(20 + led_x_translate, 18 + led_y_translate);
    matrix.setTextColor(matrix.color444(255, 255, 255));
    matrix.print("WiFi...");

    TranslateLedLogic();

    Serial.print('.');

    delay(700);
    matrix.fillScreen(0);
    delay(300);

    // wifi 연결 중 리셋 기능: 한 번만 눌러도 리셋
    resetReading = digitalRead(resetButton); // 버튼 상태 읽기
    if (resetReading != buttonState)         // 버튼 상태가 바뀌면
    {
      buttonState = resetReading; // 버튼 상태 업데이트
      Serial.println(buttonState);

      // 버튼이 눌렸을 때 (안누름->누름 상태변화)
      if (buttonState == LOW)
      {
        Serial.println("Factory Reset Button Pressed.");

        // Print Reset state
        matrix.setCursor(0, 25);

        matrix.setTextColor(matrix.color444(0, 127, 255)); // 바다색
        matrix.print("Reset");

        delay(1000);

        SPIFFS.remove(houseIdPath);
        SPIFFS.remove(ssidPath);
        SPIFFS.remove(passPath);
        Serial.println("Factory Reset Complete.");

        Serial.println("ESP will restart.");
        delay(1000);
        ESP.restart();
      }
    }
  }
  Serial.print("Connected IP : ");
  Serial.println(WiFi.localIP());
}

void initLED()
{
  // matrix와 폰트 설정
  matrix.begin();
  // matrix.setFont(&GodoM6pt8b);
  // matrix.setFont(&FreeSans9pt7b);
  matrix.setFont();
  matrix.setTextSize(1);     // size 1 == 8 pixels high
  matrix.setTextWrap(false); // Don't wrap at end of line - will do ourselves
  matrix.setAttribute(UTF8_ENABLE, true);
  matrix.setTextColor(matrix.color444(15, 9, 12));
  matrix.fillScreen(0); // 화면 클리어
}

// void PrintLED(String m1, String m2)
// {
//   matrix.fillScreen(0); // 화면 클리어
//   // matrix.fillRect(2, 2, 60, 10, 0);
//   // matrix.fillRect(27, 12, 35, 19, 0);
//   matrix.setCursor(3, 13);
//   matrix.setTextColor(matrix.color444(100, 30, 0));
//   matrix.print("temp");

//   matrix.setCursor(30, 13);
//   matrix.setTextColor(matrix.color444(15, 9, 12));
//   String sTemp = m1 + "C";
//   matrix.print(sTemp.c_str());

//   matrix.setCursor(3, 23);
//   matrix.setTextColor(matrix.color444(0, 3, 150));
//   matrix.print("humi");

//   matrix.setCursor(30, 23);
//   matrix.setTextColor(matrix.color444(15, 9, 12));
//   String sHum = m2 + "%";
//   matrix.print(sHum.c_str());

//   // matrix.setCursor(1, 3);
//   // matrix.setTextColor(matrix.color444(255, 0, 0));
//   // // String strtim = String(Month) + "-" + String(Day) + " " + String(Hour) + ":" + String(Min);
//   // matrix.printf("%02d/%02d", Month, Day);

//   // matrix.setCursor(34, 3);
//   // matrix.printf("%02d:%02d", Hour, Day);

//   delay(10); // matrix needs minimum delay
// }

void PrintLED(String m0, String m1, String m2)
{
  // 하우스 정보 테두리 사각형
  matrix.drawRect(1 + led_x_translate, 1 + led_y_translate, 9, 11, matrix.color444(255, 0, 0));

  // 하우스 정보 표시
  matrix.setCursor(3 + led_x_translate, 3 + led_y_translate);
  matrix.setTextColor(matrix.color444(255, 255, 255));
  if (houseId == "")
  {
    matrix.print(m0);
  }
  else
  {
    matrix.print(houseId);
  }

  // 온도 (이미지로된 텍스트)
  matrix.drawBitmap(LED_X_TH + led_x_translate, LED_Y_T + led_y_translate, IMG_temp_1px_final, LED_X_FONT * 2, LED_Y_FONT, matrix.color444(100, 30, 0));
  // matrix.swapBuffer(); // 버퍼를 교환하여 화면에 출력

  // 습도 (이미지로된 텍스트)
  matrix.drawBitmap(LED_X_TH + led_x_translate, LED_Y_T + LED_Y_FONT + LED_Y_SPACE_BetweenTH + led_y_translate, IMG_humi_1px_final, LED_X_FONT * 2, LED_Y_FONT, 24128);
  // matrix.swapBuffer(); // 버퍼를 교환하여 화면에 출력

  // Print Sensor data - temp
  matrix.setCursor(LED_X_TH + LED_X_FONT * 2 + LED_X_SPACE_Between_TH_VALUE + led_x_translate, LED_Y_VALUE + led_y_translate);
  matrix.setTextColor(0xffff);
  String sTemp = m1;
  sTemp = sTemp.substring(0, sTemp.length());
  matrix.print(sTemp.c_str());

  // Celcius
  matrix.drawBitmap(LED_X_TH + LED_X_FONT * 2 + LED_X_SPACE_Between_TH_VALUE + LED_X_VALUE + led_x_translate, LED_Y_VALUE + led_y_translate, IMG_celcius_1px_final, 8, 7, matrix.color444(100, 30, 0));
  matrix.swapBuffer(); // 버퍼를 교환하여 화면에 출력

  // Print Sensor data - humi
  matrix.setCursor(LED_X_TH + LED_X_FONT * 2 + LED_X_SPACE_Between_TH_VALUE + led_x_translate, LED_Y_VALUE + LED_Y_NUM + LED_Y_SPACE_Between_VALUE + led_y_translate);
  matrix.setTextColor(0xffff);
  String sHum = m2;
  sHum = sHum.substring(0, sHum.length());
  matrix.print(sHum.c_str());

  // Percent mark
  matrix.setCursor(LED_X_TH + LED_X_FONT * 2 + LED_X_SPACE_Between_TH_VALUE + LED_X_VALUE + led_x_translate, LED_Y_VALUE + LED_Y_NUM + LED_Y_SPACE_Between_VALUE + led_y_translate);
  matrix.setTextColor(24128);
  matrix.print("%");

  // *************************************************************************************

  // matrix.setCursor(3 + led_x_translate, 13 + led_y_translate);
  // matrix.setTextColor(matrix.color444(100, 30, 0));
  // matrix.print("temp");

  // matrix.setCursor(3 + led_x_translate, 23 + led_y_translate);
  // matrix.setTextColor(matrix.color444(0, 3, 150));
  // matrix.print("humi");

  // matrix.setCursor(1, 3);
  // matrix.setTextColor(matrix.color444(255, 0, 0));
  // // String strtim = String(Month) + "-" + String(Day) + " " + String(Hour) + ":" + String(Min);
  // matrix.printf("%02d/%02d", Month, Day);

  // matrix.setCursor(34, 3);
  // matrix.printf("%02d:%02d", Hour, Day);

  // *************************************************************************************

  delay(10); // matrix needs minimum delay
}

void PrintInvalidData(String s0, String s1)
{
  matrix.setCursor(11 + led_x_translate, 6 + led_y_translate);
  matrix.setTextColor(matrix.color444(255, 255, 255));
  matrix.print(s0);

  matrix.setCursor(15 + led_x_translate, 18 + led_y_translate);
  matrix.setTextColor(matrix.color444(255, 255, 255));
  matrix.print(s1);

  Serial.println(s0 + +" " + s1);
}

void TranslateLedLogic()
{
  // Led Translate logic
  ++matrix_index;
  if (matrix_index == 9) // 이동 행렬 초기화
    matrix_index = 0;

  led_x_translate = Matrix_TranslateLED[matrix_index][0];
  led_y_translate = Matrix_TranslateLED[matrix_index][1];
}

void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  Year = timeinfo.tm_year + 1900;
  Month = timeinfo.tm_mon + 1;
  Day = timeinfo.tm_mday;
  Hour = timeinfo.tm_hour;
  Min = timeinfo.tm_min;
  String strtim = String(Year) + "-" + String(Month) + "-" + String(Day) + " " + String(Hour) + ":" + String(Min);
  Serial.println(strtim);

  // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void timeWork(void *para)
{
  while (true)
  {
    printLocalTime();
    delay(10000);
  }
}

// Initialize SPIFFS
void initSPIFFS()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}

// Read File from SPIFFS
String readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available())
  {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("- file written");
  }
  else
  {
    Serial.println("- write failed");
  }
}

bool isWMConfigDefined()
{
  if (houseId == "" || ssid == "")
  {
    Serial.println("Undefined House ID or Wifi credentials...");
    return false;
  }
  return true;
}

void setup()
{
#if defined(DISABLE_BROWNOUT_PROBLEM)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector
#endif

  Serial.begin(115200);

  pinMode(resetButton, INPUT_PULLUP); // 리셋버튼 GPIO13; 평상시 HIGH, 누르면 LOW

  initSPIFFS();

  // Load values saved in SPIFFS
  houseId = readFile(SPIFFS, houseIdPath);
  ssid = readFile(SPIFFS, ssidPath);
  pass = readFile(SPIFFS, passPath);

  Serial.print("HouseID in SPIFFS: ");
  Serial.println(houseId);
  Serial.print("SSID in SPIFFS: ");
  Serial.println(ssid);
  Serial.print("pass in SPIFFS: ");
  Serial.println(pass.length() == 0 ? "NO password." : "Password exists.");

  // 설정 안된 상태: AP모드 진입(wifi config reset): softAP() 메소드
  if (!isWMConfigDefined())
  {
    // Connect to Wi-Fi network with SSID and pass
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("ESP-LED-MANAGER-00", NULL);

    IPAddress IP = WiFi.softAPIP(); // Software enabled Access Point : 가상 라우터, 가상의 액세스 포인트
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Web Server Root URL
    // GET방식
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/wifimanager.html", "text/html"); });

    server.serveStatic("/", SPIFFS, "/");
    // POST방식
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
              {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST houseId value
          if (p->name() == PARAM_INPUT_1) {
            houseId = p->value().c_str();
            Serial.print("House ID set to: ");
            Serial.println(houseId);
            // Write file to save value
            writeFile(SPIFFS, houseIdPath, houseId.c_str());
          }
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_2)
          {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(SPIFFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_3)
          {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(SPIFFS, passPath, pass.c_str());
          }
          Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      // ESP가 양식 세부 정보를 수신했음을 알 수 있도록 일부 텍스트가 포함된 응답을 send
      request->send(200, "text/plain", "Done. ESP will restart.");
      delay(3000);
      ESP.restart(); });
    server.begin();
  }

  // 설정 완료 후: wifi 연결
  else
  {
    Serial.println("LED Panel STARTED");
    initLED();
    matrix.fillScreen(0);

    initWiFi();
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());

    configTime(3600 * timeZone, 3600 * summerTime, ntpServer);
    printLocalTime();
    Udp.begin(11000);

    xTaskCreatePinnedToCore(timeWork, "timeWork", 10000, NULL, 0, &thWork, 0);
    allowsLoop = true;
  }
}

String *rStr = nullptr;

void loop()
{
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting every interval seconds
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousWiFiMillis >= interval && allowsLoop))
  {
    Serial.print(millis());
    Serial.println("ms; Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousWiFiMillis = currentMillis;
  }

  // UDP Part
  if (allowsLoop)
  {
    int packetSize = Udp.parsePacket();

    currentMillis = millis();
    if (packetSize > 0)
    {
      previousNoPacketMillis = currentMillis; // 패킷을 받았으면 no data 출력 연기

      Serial.print("Receive Size:");
      Serial.println(packetSize);
      int len = Udp.read(packetBuffer, 50);
      if (len > 0) // 수신된 UDP 패킷이 존재할 때 parsing
      {
        int cnt = 0;
        packetBuffer[len] = 0;
        Serial.print("Message: ");
        Serial.println(packetBuffer);
        rStr = Split(packetBuffer, '&', &cnt);
        if (cnt >= 2)
        {
          if (rStr[0] == houseId) // LED 패널별로 하우스 동번호 선별해서 출력
          {
            previousInvalidDataMillis = currentMillis; // 갱신됐으면 데이터 만료 연기

            matrix.fillScreen(0);                // 화면 클리어
            PrintLED(rStr[0], rStr[1], rStr[2]); // { houseId, temp, humi }
            Serial.println(rStr[0]);
            Serial.println(rStr[1]);
            Serial.println(rStr[2]);

            TranslateLedLogic();
          }

          // 패킷은 받되 일치하는 houseId의 정보가 일정기간동안 갱신되지 않으면
          else if (rStr[0] != houseId && currentMillis - previousInvalidDataMillis >= DATA_INVALID_TIME)
          {
            previousInvalidDataMillis = currentMillis;

            matrix.fillScreen(0); // 화면 클리어
            PrintInvalidData("Invalid", "Data");

            TranslateLedLogic();
          }
        }
      } // end if (len > 0)
    } // end if(packetSize > 0)

    // 패킷올 못받았으면?
    else if (packetSize == 0 && currentMillis - previousNoPacketMillis >= WATING_RCV_PACKET_TIME)
    {
      previousNoPacketMillis = currentMillis;

      matrix.fillScreen(0); // 화면 클리어
      PrintInvalidData("NO", "Packet");

      TranslateLedLogic();
    }
  } // end if(allowsLoop)

  // 공장 초기화 기능 추가 : 버튼-GPIO13
  resetReading = digitalRead(resetButton); // 버튼 상태 읽기
  // 디바운스를 위한 지연 시간; 상태가 변해야 카운트 시작
  if (resetReading != lastButtonState)
  {
    lastDebounceTime = millis();
  }

  // 디바운스 시간이 지난 후 버튼 상태 업데이트
  if (millis() - lastDebounceTime > debounceDelay)
  {
    // 버튼 상태 업데이트
    if (resetReading != buttonState) // 버튼 상태가 바뀌면
    {
      buttonState = resetReading; // 버튼 상태 업데이트
      Serial.println(buttonState);

      // 버튼이 눌렸을 때 (안누름->누름 상태변화)
      if (buttonState == LOW)
      {
        Serial.println("Factory Reset Button Pressed.");

        isResetBtnPressed = true; // bool 옵션넣고 초기 reset문자 띄우고
        isRunNextif1 = true;

        // Print Reset state
        matrix.setCursor(0, 25);

        matrix.setTextColor(matrix.color444(0, 127, 255)); // 바다색
        matrix.print("Reset");

        // udp print영역에서 화면 지우는것 따라 reset 텍스트 유지하면서
        // 이 구역에선 초당 . 하나씩 늘려가면서 푸른색 유지

        pressStartTime = millis(); // 버튼을 누른 시작 시간 기록
      }
      // 버튼이 눌리지 않았을 때 (누름->안누름, 버튼을 뗐을 때)
      else
      {
        // 버튼을 10초이상 누르다가 뗐을 때 & 마지막 버튼 상태가 low (눌림)라면
        if ((millis() - pressStartTime >= 5000))
        {
          // reset 기능 구현
          Serial.println("Reset Button pressed continuously for 5 secs.");
          Serial.println("Running Factory Reset...");

          SPIFFS.remove(houseIdPath);
          SPIFFS.remove(ssidPath);
          SPIFFS.remove(passPath);
          Serial.println("Factory Reset Complete.");

          Serial.println("ESP will restart.");
          delay(1000);
          ESP.restart();
        }
        else
        {
          matrix.fillScreen(0); // 화면 클리어
          if (rStr != nullptr)
            PrintLED(rStr[0], rStr[1], rStr[2]);
        }
      }
    }
  }
  lastButtonState = resetReading; // 이전 버튼 상태 업데이트

  // // test; Translate LED
  // currentMillis = millis();
  // if (currentMillis - previousMillis >= 10000)
  // {
  //   previousMillis = currentMillis;

  //   matrix.fillScreen(0); // 화면 클리어
  //   PrintLED(String(18.9), String(37.4));

  //   ++matrix_index;
  //   if (matrix_index == 9)
  //     matrix_index = 0;

  //   led_x_translate = Matrix_TranslateLED[matrix_index][0];
  //   led_y_translate = Matrix_TranslateLED[matrix_index][1];
  // }

  // // test; only String
  // PrintLED(String(-18.1), String(35.4));

  // 1초마다 '.' 늘려가도록 하드코딩 -.-
  currentMillis = millis();
  if ((1000 <= currentMillis - pressStartTime && currentMillis - pressStartTime < 2000) && buttonState == 0 && isRunNextif1)
  {
    matrix.setCursor(0, 25);
    matrix.setTextColor(matrix.color444(0, 127, 255)); // 바다색
    matrix.print("Reset.");

    isRunNextif1 = false;
    isRunNextif2 = true;
  }
  else if ((2000 <= currentMillis - pressStartTime && currentMillis - pressStartTime < 3000) && buttonState == 0 && isRunNextif2)
  {
    matrix.setCursor(0, 25);
    matrix.setTextColor(matrix.color444(0, 127, 255)); // 바다색
    matrix.print("Reset..");

    isRunNextif2 = false;
    isRunNextif3 = true;
  }
  else if ((3000 <= currentMillis - pressStartTime && currentMillis - pressStartTime < 4000) && buttonState == 0 && isRunNextif3)
  {
    matrix.setCursor(0, 25);
    matrix.setTextColor(matrix.color444(0, 127, 255)); // 바다색
    matrix.print("Reset...");

    isRunNextif3 = false;
    isRunNextif4 = true;
  }
  else if ((4000 <= currentMillis - pressStartTime && currentMillis - pressStartTime < 5000) && buttonState == 0 && isRunNextif4)
  {
    matrix.setCursor(0, 25);
    matrix.setTextColor(matrix.color444(0, 127, 255)); // 바다색
    matrix.print("Reset....");

    isRunNextif4 = false;
    isRunNextif5 = true;
  }
  else if ((5000 <= currentMillis - pressStartTime) && buttonState == 0 && isRunNextif5)
  {
    matrix.setCursor(0, 25);
    matrix.setTextColor(matrix.color444(255, 127, 0)); // 반전, 주황색
    matrix.print("Reset.....");
  }
}
