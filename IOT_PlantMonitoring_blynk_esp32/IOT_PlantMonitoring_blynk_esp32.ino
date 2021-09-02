/* Chương trình đọc nhiệt độ, độ ẩm từ cảm biến DHT
  Thêm chức năng đọc cảm biến ánh sáng
  Thêm chức năng đọc cảm biến độ ẩm đất
  Thêm hiển thị LCD
  Thêm chức năng điều khiển tưới tiêu bằng tay 2 bơm
*/
/* ESP & Blynk */
#define BLYNK_PRINT Serial    

#include <BlynkSimpleEsp32.h>
#include <DHT.h>

WidgetLED PUMP(V0);  // Đèn trạng thái bơm
WidgetLED LAMP(V1);  // Đèn trạng thái đèn sưởi

#define DHTPIN 2

#define SOIL_MOIST_1_PIN 4 
// Relay, nút nhấn
#define PUMP_ON_BUTTON 34 //Nút điều khiển bằng tay bơm
#define LAMP_ON_BUTTON 35 //Nút điều khiển đèn bằng tay
#define SENSORS_READ_BUTTON 36 //only input //Nút lấy dữ liệu tức thời

#define PUMP_PIN 25   // Bơm
#define LAMP_PIN 26   // Đèn
// Uncomment loại cảm biến bạn không sử dụng, nếu DHT22 thì uncomment DHT11 và comment DHT22
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

/* Thông số cho chế độ tự động */
#define DRY_SOIL      66
#define WET_SOIL      85
#define COLD_TEMP     12
#define HOT_TEMP      22
#define TIME_PUMP_ON  15
#define TIME_LAMP_ON  15

/* TIMER */
#define READ_BUTTONS_TM   1L  // Tương ứng với giây
#define READ_SOIL_HUM_TM  10L //Đọc cảm biến ẩm đất
#define READ_AIR_DATA_TM  1L  //Đọc DHT
#define DISPLAY_DATA_TM   10L //Gửi dữ liệu lên terminal
#define SEND_UP_DATA_TM   10L //Gửi dữ liệu lên blynk
#define AUTO_CTRL_TM      60L //Chế độ tư động
//Token Blynk và wifi
char auth[] = ""; // Blynk token
char ssid[] = ""; //Tên wifi
char pass[] = ""; //Mật khẩu

// Biến lưu các giá trị cảm biến
float humDHT = 0;
float tempDHT = 0;
//int lumen;
int soilMoist = 0;
// Biến lưu trạng thái bơm
boolean pumpStatus = 0;
boolean lampStatus = 0;

int timePumpOn = 10; // Thời gian bật bơm nước
// Biến cho timer
long sampleTimingSeconds = 50; // ==> Thời gian đọc cảm biến (s)
long startTiming = 0;
long elapsedTime = 0;
// Khởi tạo timer
SimpleTimer timer;

// Khởi tạo cảm biến
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LAMP_PIN, OUTPUT);
  pinMode(PUMP_ON_BUTTON, INPUT_PULLUP);
  pinMode(LAMP_ON_BUTTON, INPUT_PULLUP);
  pinMode(SENSORS_READ_BUTTON, INPUT_PULLUP);
  aplyCmd();
  // Khởi tạo cổng serial baud 115200
  Serial.begin(115200);

  dht.begin();    // Bắt đầu đọc dữ liệu
  Blynk.begin(auth, ssid, pass);
  PUMP.off();
  LAMP.off();
  startTimers();
}

void loop() {
  timer.run(); // Bắt đầu SimpleTimer
  Blynk.run();
}
/****************************************************************
* Hàm điều khiển nhận tín hiệu từ blynk
****************************************************************/
BLYNK_WRITE(3) // Điều khiển bơm
{
  int i = param.asInt();
  if (i == 1)
  {
    pumpStatus = !pumpStatus;
    aplyCmd();
  }
}

BLYNK_WRITE(4) // Điều khiển đèn
{
  int i = param.asInt();
  if (i == 1)
  {
    lampStatus = !lampStatus;
    aplyCmd();
  }
}

void getSoilMoist(void)
{
  int i = 0;
  soilMoist = 0;
  for (i = 0; i < 10; i++)  //
  {
    soilMoist += analogRead(SOIL_MOIST_1_PIN); //Đọc giá trị cảm biến độ ẩm đất
    delay(50);   // Đợi đọc giá trị ADC
  }
  soilMoist = soilMoist / (i);
  soilMoist = map(soilMoist, 1023, 0, 0, 100); //Ít nước:0%  ==> Nhiều nước 100%

}

void getDhtData(void)
{

  tempDHT = dht.readTemperature();
  humDHT = dht.readHumidity();
  if (isnan(humDHT) || isnan(tempDHT))   // Kiểm tra kết nối lỗi thì thông báo.
  {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
}
void printData(void)
{
  // IN thông tin ra màn hình
  Serial.print("Do am: ");
  Serial.print(humDHT);
  Serial.print(" %\t");
  Serial.print("Nhiet do: ");
  Serial.print(tempDHT);
  Serial.print(" *C\t");

  Serial.print(" %\t");
  Serial.print("Do am dat: ");
  Serial.print(soilMoist);
  Serial.println(" %");
}

/****************************************************************
  Hàm đọc trạng thái bơm và kiểm tra nút nhấn
  (Nút nhấn mặc định là mức "CAO"):
****************************************************************/
void readLocalCmd()
{
  boolean digiValue = debounce(PUMP_ON_BUTTON);
  if (!digiValue)
  {
    pumpStatus = !pumpStatus;
    aplyCmd();
  }

  digiValue = debounce(LAMP_ON_BUTTON);
  if (!digiValue)
  {
    lampStatus = !lampStatus;
    aplyCmd();
  }

  digiValue = debounce(SENSORS_READ_BUTTON);
  if (!digiValue)
  {
    getDhtData();
    getSoilMoist();
    printData();
  }
}
/***************************************************
  Thực hiện điều khiển các bơm
****************************************************/
void aplyCmd()
{
  if (pumpStatus == 1)
  {
    Blynk.notify("NDTRBOT: Canh bao ==>> BOM ON");
    digitalWrite(PUMP_PIN, LOW);
    PUMP.on();
  }

  else {
    digitalWrite(PUMP_PIN, HIGH);
    PUMP.off();
  }

  if (lampStatus == 1)
  {
    Blynk.notify("NDTRBOT: Canh bao ==>> DEN ON");
    digitalWrite(LAMP_PIN, LOW);
    LAMP.on();
  }
  else
  {
    digitalWrite(LAMP_PIN, HIGH);
    LAMP.off();
  }
}
/***************************************************
  Hàm kiểm tra trạng thái phím bấm
****************************************************/
boolean debounce(int pin)
{
  boolean state;
  boolean previousState;
  const int debounceDelay = 60;

  previousState = digitalRead(pin);
  for (int counter = 0; counter < debounceDelay; counter++)
  {
    delay(1);
    state = digitalRead(pin);
    if (state != previousState)
    {
      counter = 0;
      previousState = state;
    }
  }
  return state;
}
/***************************************************
* Chế độ tự động dựa trên thông số cảm biến
****************************************************/
void autoControlPlantation(void)
{
  if (soilMoist < DRY_SOIL)
  {
    turnPumpOn();
  }

  if (tempDHT < COLD_TEMP)
  {
    turnLampOn();
  }
}
/***************************************************
* Bật bơm trong thời gian định sẵn
****************************************************/
void turnPumpOn()
{
  pumpStatus = 1;
  aplyCmd();
  delay (TIME_PUMP_ON * 1000);
  pumpStatus = 0;
  aplyCmd();
}

/***************************************************
* Bật đèn trong thời gian định sẵn
****************************************************/
void turnLampOn()
{
  lampStatus = 1;
  aplyCmd();
  delay (TIME_LAMP_ON * 1000);
  lampStatus = 0;
  aplyCmd();
}

/***************************************************
  Khởi động Timers
****************************************************/
void startTimers(void)
{
  timer.setInterval(READ_BUTTONS_TM * 1000, readLocalCmd);
  timer.setInterval(READ_AIR_DATA_TM * 1000, getDhtData);
  timer.setInterval(READ_SOIL_HUM_TM * 1000, getSoilMoist);
  timer.setInterval(SEND_UP_DATA_TM * 1000, sendUptime);
  timer.setInterval(AUTO_CTRL_TM * 1000, autoControlPlantation);
//  timer.setInterval(DISPLAY_DATA_TM * 1000, printData);
}
/***************************************************
 * Gửi dữ liệu lên Blynk
 **************************************************/
void sendUptime()
{
  Blynk.virtualWrite(10, tempDHT); //Nhiệt độ với pin V10
  Blynk.virtualWrite(11, humDHT); // Độ ẩm với pin V11
  Blynk.virtualWrite(12, soilMoist); // Độ ẩm đất với V12
}
