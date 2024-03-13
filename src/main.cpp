#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_GFX.h>
#include <P3RGB64x32MatrixPanel.h>
#include <time.h>
#include <NTPClient.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/GodoM6pt8b.h>

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

void initSPIFFS();                                                 // Initialize SPIFFS
String readFile(fs::FS &fs, const char *path);                     // Read File from SPIFFS
void writeFile(fs::FS &fs, const char *path, const char *message); // Write file to SPIFFS
bool isCamConfigDefined();                                         // Is Cam Configuration Defiend?

// Replace with your network credentials (STATION)
const char *ntpServer = "pool.ntp.org";
uint8_t timeZone = 9;
uint8_t summerTime = 0;

// **********************************************************************************************************
const char *ssid = "itime";
const char *password = "daon7521";
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

void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void PrintLED(String m1, String m2)
{
  matrix.fillScreen(0); // 화면 클리어
  // matrix.fillRect(2, 2, 60, 10, 0);
  // matrix.fillRect(27, 12, 35, 19, 0);
  matrix.setCursor(3, 13);
  matrix.setTextColor(matrix.color444(100, 30, 0));
  matrix.print("temp");

  matrix.setCursor(30, 13);
  matrix.setTextColor(matrix.color444(15, 9, 12));
  String sTemp = m1 + "C";
  matrix.print(sTemp.c_str());

  matrix.setCursor(3, 23);
  matrix.setTextColor(matrix.color444(0, 3, 150));
  matrix.print("humi");

  matrix.setCursor(30, 23);
  matrix.setTextColor(matrix.color444(15, 9, 12));
  String sHum = m2 + "%";
  matrix.print(sHum.c_str());

  matrix.setCursor(1, 3);
  matrix.setTextColor(matrix.color444(255, 0, 0));
  // String strtim = String(Month) + "-" + String(Day) + " " + String(Hour) + ":" + String(Min);
  matrix.printf("%02d/%02d", Month, Day);

  matrix.setCursor(34, 3);
  matrix.printf("%02d:%02d", Hour, Day);
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

bool isCamConfigDefined()
{
  if (houseId == "")
  {
    Serial.println("Undefined Cam ID or slaveMac or Capture Period.");
    return false;
  }
  return true;
}

void setup()
{
  Serial.begin(115200);

  // Load values saved in SPIFFS
  houseId = readFile(SPIFFS, houseIdPath);

  Serial.print("HouseID in SPIFFS: ");
  Serial.println(houseId);

  // AP모드 진입(cam config reset): softAP() 메소드
  if (!isCamConfigDefined())
  {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("ESP-WIFI-MANAGER Master0", NULL);

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
          Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      // ESP가 양식 세부 정보를 수신했음을 알 수 있도록 일부 텍스트가 포함된 응답을 send
      request->send(200, "text/plain", "Done. ESP will restart.");
      delay(3000);
      ESP.restart(); });
    server.begin();
  }
  else
  {
    Serial.println("CAMERA MASTER STARTED"); // tarted : 시작되다; 자동사인듯?
    // initCamera();                            // init camera
    // initSD();                                 // init sd

    // Set device in STA mode to begin with
    WiFi.mode(WIFI_STA);
    // This is the mac address of the Master in Station Mode
    Serial.print("STA MAC: ");
    Serial.println(WiFi.macAddress());

    // allowsLoop = true;
  }

  initWiFi();
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());
  initLED();

  configTime(3600 * timeZone, 3600 * summerTime, ntpServer);
  printLocalTime();
  Udp.begin(11000);

  xTaskCreatePinnedToCore(timeWork, "timeWork", 10000, NULL, 0, &thWork, 0);
}

void loop()
{

  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval))
  {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }

  int packetSize = Udp.parsePacket();
  if (packetSize > 0)
  {
    Serial.print("Receive Size:");
    Serial.println(packetSize);
    int len = Udp.read(packetBuffer, 50);
    if (len > 0)
    {
      int cnt = 0;
      packetBuffer[len] = 0;
      Serial.print("Message:");
      Serial.println(packetBuffer);
      String *rStr = Split(packetBuffer, '&', &cnt);
      if (cnt >= 2)
      {
        PrintLED(rStr[0], rStr[1]);
      }
    }
  }

  PrintLED(String(18.1), String(35.4));
}
