// Active/DeActive Brownout problem
// #define DISABLE_BROWNOUT_PROBLEM
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

#include "temp_1px_BW_1bit.h"
#include "temp_2px_BW_1bit.h"
#include "humi_1px_BW_1bit.h"
#include "humi_2px_BW_1bit.h"

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

int8_t matrix_index = 0;
int8_t led_x = 0;
int8_t led_y = 0;

void initSPIFFS();                                                 // Initialize SPIFFS
String readFile(fs::FS &fs, const char *path);                     // Read File from SPIFFS
void writeFile(fs::FS &fs, const char *path, const char *message); // Write file to SPIFFS
bool isWMConfigDefined();                                          // Is Wifi Manager Configuration Defiend?
bool allowsLoop = false;

String *Split(String sData, char cSeparator, int *scnt);
void initLED();
void initWiFi();
void PrintLED(String m1, String m2);
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
unsigned long previousMillis = 0;
unsigned long interval = 30000;
char packetBuffer[60];
int Year;
int Month;
int Day;
int Hour;
int Min;

// Create UDP instance
WiFiUDP Udp;

TaskHandle_t thWork;
String formattedDate;

String *Split(String sData, char cSeparator, int *scnt)
{
  static String charr[17] = {"0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0"};
  int nCount = 0;
  int nGetIndex = 0;

  // 임시저장
  String sTemp = "";

  // 원본 복사
  String sCopy = sData;

  while (true)
  {
    // 구분자 찾기
    nGetIndex = sCopy.indexOf(cSeparator);

    if ((sData.length() - 1) == nGetIndex)
      break;
    // 리턴된 인덱스가 있나?
    if (-1 != nGetIndex)
    {
      // 있다.

      // 데이터 넣고
      sTemp = sCopy.substring(0, nGetIndex);

      // Serial.println( sTemp );

      // 뺀 데이터 만큼 잘라낸다.
      sCopy = sCopy.substring(nGetIndex + 1);
      if (nGetIndex == 0)
        continue;
      charr[nCount] = sTemp;
    }
    else
    {
      // 없으면 마무리 한다.
      // Serial.println( sCopy );
      charr[nCount] = sCopy;
      break;
    }

    // 다음 문자로~
    ++nCount;
  }
  // Serial.println("문자갯수:" + String(nCount));
  *scnt = nCount;
  return charr;
}

void initWiFi()
{
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("Connecting to WiFi ..");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
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

void PrintLED(String m1, String m2)
{
  // matrix.fillRect(2, 2, 60, 10, 0);
  // matrix.fillRect(27, 12, 35, 19, 0);

  // 하우스 정보 표시
  matrix.setCursor(1 + led_x, 1 + led_y);
  matrix.setTextColor(matrix.color444(100, 30, 0));
  // matrix.print(houseId);
  matrix.print(8);

  // 온도 (이미지로된 텍스트)
  matrix.drawBitmap(6 + led_x, 1 + led_y, IMG_temp_1px, 26, 13, 0xffff);
  matrix.swapBuffer(); // 버퍼를 교환하여 화면에 출력

  // 습도 (이미지로된 텍스트)
  matrix.drawBitmap(6 + led_x, 16 + led_y, IMG_humi_1px, 26, 13, 0xffff);
  matrix.swapBuffer(); // 버퍼를 교환하여 화면에 출력

  // // 온도2px (이미지로된 텍스트)
  // matrix.drawBitmap(35 + led_x, 1 + led_y, IMG_temp_2px, 26, 13, 0xffff);
  // matrix.swapBuffer(); // 버퍼를 교환하여 화면에 출력

  // // 습도2px (이미지로된 텍스트)
  // matrix.drawBitmap(35 + led_x, 16 + led_y, IMG_humi_2px, 26, 13, 0xffff);
  // matrix.swapBuffer(); // 버퍼를 교환하여 화면에 출력

  // Print Sensor data - temp
  matrix.setCursor(35 + led_x, 4 + led_y);
  matrix.setTextColor(matrix.color444(15, 9, 12));
  String sTemp = m1;
  sTemp = sTemp.substring(0, sTemp.length() - 1) + "C"; // slice zero
  matrix.print(sTemp.c_str());

  // Print Sensor data - humi
  matrix.setCursor(35 + led_x, 19 + led_y);
  matrix.setTextColor(matrix.color444(15, 9, 12));
  String sHum = m2;
  sHum = sHum.substring(0, sHum.length() - 1) + "%"; // slice zero
  matrix.print(sHum.c_str());

  // *************************************************************************************

  // matrix.setCursor(3 + led_x, 13 + led_y);
  // matrix.setTextColor(matrix.color444(100, 30, 0));
  // matrix.print("temp");

  // matrix.setCursor(3 + led_x, 23 + led_y);
  // matrix.setTextColor(matrix.color444(0, 3, 150));
  // matrix.print("humi");

  // matrix.setCursor(1, 3);
  // matrix.setTextColor(matrix.color444(255, 0, 0));
  // // String strtim = String(Month) + "-" + String(Day) + " " + String(Hour) + ":" + String(Min);
  // matrix.printf("%02d/%02d", Month, Day);

  // matrix.setCursor(34, 3);
  // matrix.printf("%02d:%02d", Hour, Day);

  delay(10); // matrix needs minimum delay
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
  if (houseId == "")
  {
    Serial.println("Undefined House ID...");
    return false;
  }
  else if (ssid == "")
  {
    Serial.println("Undefined Wifi credentials...");
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
    WiFi.softAP("ESP-LED-MANAGER", NULL);

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
            Serial.print("Cam ID set to: ");
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

void loop()
{

  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval && allowsLoop))
  {
    Serial.print(millis());
    Serial.println("ms; Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }

  // // UDP Part
  // int packetSize = Udp.parsePacket();
  // if (packetSize > 0)
  // {
  //   Serial.print("Receive Size:");
  //   Serial.println(packetSize);
  //   int len = Udp.read(packetBuffer, 50);
  //   if (len > 0)
  //   {
  //     int cnt = 0;
  //     packetBuffer[len] = 0;
  //     Serial.print("Message:");
  //     Serial.println(packetBuffer);
  //     String *rStr = Split(packetBuffer, '&', &cnt);
  //     if (cnt >= 2)
  //     {
  //       matrix.fillScreen(0); // 화면 클리어
  //       PrintLED(rStr[0], rStr[1]);
  //     }
  //   }
  // }

  // test; Translate LED
  if (currentMillis - previousMillis >= 5000)
  {
    previousMillis = currentMillis;
    ++matrix_index;
    if (matrix_index == 9)
      matrix_index = 0;

    led_x = Matrix_TranslateLED[matrix_index][0];
    led_y = Matrix_TranslateLED[matrix_index][1];

    matrix.fillScreen(0); // 화면 클리어
    PrintLED(String(18.1), String(35.4));
  }

  // // test; only String
  // PrintLED(String(-18.1), String(35.4));
}
