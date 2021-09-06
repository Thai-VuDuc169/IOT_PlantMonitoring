#define BLYNK_PRINT Serial    

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include "MQ135.h"
#include <WebServer.h>

WebServer server(80);
WidgetLED PUMP(V0); //đèn trạng thái bơm
WidgetLED FAN(V1); //đèn trạng thái quạt

#define DHTPIN 2
#define MQ135_PIN 5
#define SOIL_MOIST_PIN 4

MQ135 mq135_sensor = MQ135(MQ135_PIN);
//Latching relay
#define RELAY_PUMP_ON 25
#define RELAY_PUMP_OFF 26
#define RELAY_FAN_ON 34
#define RELAY_FAN_OFF 35

#define DHTTYPE DHT22

#define DRY_SOIL      66
#define WET_SOIL      85
#define COLD_TEMP     12
#define HOT_TEMP      22

//Timer
#define READ_SOIL_HUM_TM  10L //Đọc cảm biến ẩm đất
#define READ_AIR_DATA_TM  1L  //Đọc DHT MQ135
#define DISPLAY_DATA_TM   10L //Gửi dữ liệu lên terminal
#define SEND_UP_DATA_TM   10L //Gửi dữ liệu lên blynk
#define AUTO_CTRL_TM      60L //Tự động

//Token Blynk và wifi
char auth[] = "sz8QlUjDAHqVp3ugF1MrwPntXxfQA4NN"; // Blynk token
char ssid[] = "Tran van suot"; //Tên wifi
char pass[] = "12345678"; //Mật khẩu

//Biến lưu giá trị sensor
float humDHT = 0;
float tempDHT = 0;
float ppmMQ = 0;
float ppm = 0;
int soilMoist = 0;

boolean pumpStatus = 0;
boolean fanStatus = 0;

SimpleTimer timer;

DHT dht(DHTPIN, DHTTYPE);

void setup()  {
  pinMode(RELAY_PUMP_ON, OUTPUT);
  pinMode(RELAY_PUMP_OFF, OUTPUT);
  pinMode(RELAY_FAN_ON, OUTPUT);
  pinMode(RELAY_FAN_OFF, OUTPUT);
  
  

  Serial.begin(115200);
  digitalWrite(RELAY_PUMP_OFF, HIGH); //Đưa bơm và quạt về trạng thái tắt trong trường hợp mạch bị mất nguồn
  digitalWrite(RELAY_FAN_OFF, HIGH);
  dht.begin();
  Blynk.begin(auth, ssid, pass);
  PUMP.off();
  FAN.off();

  Serial.println("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);

  startTimers();
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  timer.run();
  Blynk.run();
  server.handleClient();
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML(tempDHT, humDHT, ppmMQ, soilMoist, pumpStatus, fanStatus));
}

String SendHTML(float temp, float hum, float ppm, int moist, boolean pump, boolean fan) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>Plant Monitoring by ESP32</title>\n";
  ptr +="<script>\n";
  ptr +="setInterval(loadDoc,200);\n";
  ptr +="function loadDoc() {\n";
  ptr +="var xhttp = new XMLHttpRequest();\n";
  ptr +="xhttp.onreadystatechange = function() {\n";
  ptr +="if (this.readyState == 4 && this.status == 200) {\n";
  ptr +="document.getElementById(\"webpage\").innerHTML =this.responseText}\n";
  ptr +="};\n";
  ptr +="xhttp.open(\"GET\", \"/\", true);\n";
  ptr +="xhttp.send();\n";
  ptr +="}\n";
  ptr +="</script>\n";
  ptr +="</head>\n";

  ptr +="<body>\n";
  ptr +="<h1>Plant Monitoring by ESP32 </h1>\n";
  ptr +="<h2>Temperature: " + String((float)temp,1) + " C </h2>\n";
  //ptr +=(int)temp;
  //ptr +="<h1> Celsius</h1>\n";
  ptr +="<h2>Humidity: " + String((float)hum,1) + " % </h2>";
  //ptr +=(int)hum;
  //ptr +="<h1>%</h1>\n";
  ptr +="<h2>Soil Moisture: " + String((float)moist,1) + " %</h2>";
  //ptr +=(int)moist;
  //ptr +="<h1>%</h1>\n";
  ptr +="<h2>PPM: " + String((float)ppm,1) + " ppm</h2>";
  //ptr +=(int)ppm;
  //ptr +="<h1>ppm</h1>\n";
  //ptr +="<h1>Pump: </h1>";
  if (pump){
    ptr +="<h2>Pump: ON</h2>\n";
  }
  else {
    ptr +="<h2>Pump: OFF</h2>\n";
  }
  //ptr +="<h1>Fan: </h1>";
  if (fan){
    ptr +="<h2>Fan: ON</h2>\n";
  }
  else {
    ptr +="<h2>Fan: OFF</h2>\n";
  }
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}

void getSoilMoist(){
  int i = 0;
  soilMoist = 0;
  for (i = 0; i < 10; i++)  //
  {
    soilMoist += analogRead(SOIL_MOIST_PIN); //Đọc giá trị cảm biến độ ẩm đất
    delay(50);   // Đợi đọc giá trị ADC
  }
  soilMoist = soilMoist / (i);
  soilMoist = map(soilMoist, 0, 1023, 0, 100);
}

void getDhtMqData(){
  tempDHT = dht.readTemperature();
  humDHT = dht.readHumidity();
  if (isnan(humDHT) || isnan(tempDHT))   // Kiểm tra kết nối lỗi thì thông báo.
  {
    Serial.println("Failed to read from DHT sensor!");
  }
  ppm = mq135_sensor.getPPM();
  ppmMQ = mq135_sensor.getCorrectedPPM(tempDHT, humDHT);
}
void printData(){
  Serial.print("Humidity: ");
  Serial.print(humDHT);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(tempDHT);
  Serial.print(" *C\t");
  Serial.print("Soil Moisture: ");
  Serial.print(soilMoist);
  Serial.print(" %\t");
  Serial.print("PPM: ");
  Serial.print(ppmMQ);
  Serial.println(" ppm"); 
  
  Serial.print("Pump: ");
  if(pumpStatus) {
    Serial.println("ON");
  }
  else {
    Serial.println("OFF");
  }
  Serial.print("Fan: ");
  if(fanStatus) {
    Serial.println("ON");
  }
  else {
    Serial.println("OFF");
  }
}

void autoControl (){
  if (soilMoist < DRY_SOIL){
    if (pumpStatus == 0){
      pumpStatus = !pumpStatus;
      digitalWrite(RELAY_PUMP_ON, HIGH);
      PUMP.on();
    }
  }
  if (soilMoist >= WET_SOIL) {
    if (pumpStatus) {
      pumpStatus = !pumpStatus;
      digitalWrite(RELAY_PUMP_OFF, HIGH);
      PUMP.off();
    }
  }
  if (tempDHT < HOT_TEMP){
    if (fanStatus) {
      fanStatus = !fanStatus;
      digitalWrite(RELAY_FAN_OFF, HIGH);
      FAN.off();
    }
  }
  if (tempDHT >= HOT_TEMP) {
    if (fanStatus == 0) {
      fanStatus = !fanStatus;
      digitalWrite(RELAY_FAN_ON, HIGH);
      FAN.on();
    }
  }
}

void startTimers() {
  timer.setInterval(READ_AIR_DATA_TM * 1000, getDhtMqData);
  timer.setInterval(READ_SOIL_HUM_TM * 1000, getSoilMoist);
  timer.setInterval(SEND_UP_DATA_TM * 1000, sendUptime);
  timer.setInterval(AUTO_CTRL_TM * 1000, autoControl);
  timer.setInterval(DISPLAY_DATA_TM * 1000, printData);
}

void sendUptime() {
  Blynk.virtualWrite(10, tempDHT); //Nhiệt độ với pin V10
  Blynk.virtualWrite(11, humDHT); // Độ ẩm với pin V11
  Blynk.virtualWrite(12, soilMoist); // Độ ẩm đất với V12 
  Blynk.virtualWrite(13, ppmMQ); //PPM với V13
}
