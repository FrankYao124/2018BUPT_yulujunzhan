/*
实际应用时，所有串口打印均可省略,以提高效率

mCookie是单线程机，灰度模块应用效果不佳，故保留灰度检测代码但不予编译

*/

//_______________________________________________________________________________________电机相关
#include <Microduino_Motor.h>
Motor MotorLeft(MOTOR0_PINA, MOTOR0_PINB);
Motor MotorRight(MOTOR1_PINA, MOTOR1_PINB);
int p=1;//默认小车初始位置为1

//_______________________________________________________________________________________轮询相关
#include <ESP8266.h>
#include <SoftwareSerial.h>
#define EspSerial Serial1
#define EspSerial mySerial
#define SSID        "Honor 10"//WIFI名
#define PASSWORD    "12345678a"//WIFI密码
#define HOST_NAME   "api.heclouds.com"//域名
#define HOST_PORT   (80)
#define UARTSPEED  9600
SoftwareSerial mySerial(2, 3); 
ESP8266 wifi(&EspSerial);
int x=0;

//_______________________________________________________________________________________温湿度相关
#include <Wire.h>                                  
#include <I2Cdev.h>                                  
#include <Microduino_SHT2x.h>
#define DEVICEID    "503191680" //OneNet上的设备ID
#define PROJECTID   "184754" //OneNet上的产品ID
#define sensorPin_1  A0
Tem_Hum_S2 TempMonitor;
String apiKey="6gyWGKvQxQrVG5xn5WK=s6BJyuA=";//与你的设备绑定的APIKey
char buf[10];
float sensor_tem, sensor_hum, sensor_lux;                    //传感器温度、湿度、光照   
char  sensor_tem_c[7], sensor_hum_c[7], sensor_lux_c[7] ;    //换成char数组传输

void setup() 
{
  pinMode(sensorPin_1, INPUT);
  MotorLeft.begin();   
  MotorRight.begin();  
  Wire.begin();
//_______________________________________________________________________________________方便确认联网成功，实际上串口打印可省
  Serial.begin(115200);
    while (!Serial); 
    Serial.print(F("setup begin\r\n"));
    delay(100);

  WifiInit(EspSerial, UARTSPEED);

  Serial.print(F("FW Version:"));
  Serial.println(wifi.getVersion().c_str());

  if (wifi.setOprToStationSoftAP()) {
    Serial.print(F("to station + softap ok\r\n"));
  } else {
    Serial.print(F("to station + softap err\r\n"));
  }

  if (wifi.joinAP(SSID, PASSWORD)) {
    Serial.print(F("Join AP success\r\n"));

    Serial.print(F("IP:"));
    Serial.println( wifi.getLocalIP().c_str());
  } else {
    Serial.print(F("Join AP failure\r\n"));
  }

  if (wifi.disableMUX()) {
    Serial.print(F("single ok\r\n"));
  } else {
    Serial.print(F("single err\r\n"));
  }
  Serial.print(F("setup end\r\n"));
}

void loop() 
{
    online();                                               //获取控制指令的函数
   
    getSensorData();                                        //读串口中传感器数据的函数
   
    updateSensorData();                                     //将数据上传到服务器的函数

    delay(5000);                                            //给微信端充足的时间更新指令，防止命令反复执行
}

void online()
{   
    Serial.print('\n');
    wifi.createTCP(HOST_NAME, HOST_PORT);                   //建立TCP连接，用于获取控制指令
    //if (!wifi.createTCP(HOST_NAME, HOST_PORT))
    //Serial.println(TCP Failure);
  static const byte  GETDATA[]  PROGMEM = 
  {
//_______________________________________________________________________________________协议
  "GET https://api.heclouds.com/devices/505083349/datapoints?datastream_id=x&limit=1 HTTP/1.1\r\nHost:api.heclouds.com\r\napi-key:I=JqSW=ICzaDhnFRd8kJoHeBUBQ=\r\nConnection: close\r\n\r\n"};
/* 
"GET https://api.heclouds.com/devices/505083349/datapoints?datastream_id=x&limit=1 //url+  【 datapoints？datastream id=】 数据名x【&limit=】（几个数据1个表示最近）
HTTP/1.1\r\n
Host:api.heclouds.com//域名
\r\n
api-key:6gyWGKvQxQrVG5xn5WK=s6BJyuA=//API-Key
\r\n
Connection: close\r\n\r\n"
*/
  
wifi.sendFromFlash(GETDATA, sizeof(GETDATA));//从Flash读取发送内容，节约内存
  uint8_t buffer[512] = {0};
  uint32_t len = wifi.recv(buffer, sizeof(buffer), 20000);
  if (len > 0)
  {
     short temp=0;
    for (uint32_t i = 0; i < len-12; i++) {
      //Serial.print(buffer[i]);          //数据包
      if((char)buffer[i]=='v'&&(char)buffer[i+1]=='a'&&(char)buffer[i+2]=='l'&&(char)buffer[i+3]=='u'&&(char)buffer[i+4]=='e')//从value开始读取
      {
          for (uint32_t j = 0; j<1;j++)
              if((char)buffer[i+j+7]>='0'&&(char)buffer[i+j+7]<='9')//只读数字
              {temp=((short)buffer[i+j+7]-48)+temp*10;
              }
              x=temp;
			  Serial.print('\n');
              Serial.println(x);
    } }
}
//_______________________________________________________________________________________指令执行
  if(x==p)
  water();
  if(x==0)
  remain();
  if(x>p)
  forward();
  if(x<p&&x!=0)
  back();
}

void water()//浇水函数
{
  Serial.println("Down!");
  MotorLeft.setSpeed(FREE);  
  MotorRight.setSpeed(-100);  
  delay(11000);
  
  Serial.println("Water!");
  MotorLeft.setSpeed(FREE);  
  MotorRight.Brake();        
  MotorRight.setSpeed(FREE); 
  delay(5000);

  Serial.println("Up!");
  MotorLeft.setSpeed(FREE);  
  MotorRight.setSpeed(100);  
  delay(5500);

  Serial.println("Free!");
  MotorLeft.setSpeed(FREE);  
  MotorRight.setSpeed(FREE); 
  delay(3000);
}

void forward()//前进并浇水函数
{
  Serial.println("Forward!");
  MotorLeft.setSpeed(1000);//(*)
  delay(2000);//(**)

  b_reak();//(***)
//下两行为根据灰度值判断小车是否已在花盆前，是则刹车。要想正确实现必须缩短(**)处delay值，删掉(***)语句并多次执行(*)处至b_reak();语句。实际效果不佳，在此不予编译。下一个函数同理
   //if(analogRead(A2)<200)  
	   //b_reak(); 
  
  water();

  p++;
}

void back()//后退并浇水函数
{
  Serial.println("Forward!");
  MotorLeft.setSpeed(-1000);   
  delay(2000);

  b_reak(); 
  //if(analogRead(A2)<200)  
	   //b_reak(); 
  
  water();

  p--;
}

void b_reak()//刹车函数
{
  Serial.println("Break!");
  MotorLeft.Brake();
}

void remain()//待机函数
{
  Serial.println("Free!");
  MotorLeft.setSpeed(FREE);  
  MotorRight.setSpeed(FREE); 
}

void getSensorData()
{ 
    Serial.print("get SenserData"); 
    sensor_tem = TempMonitor.getTemperature();  
    sensor_hum = TempMonitor.getHumidity();   
    sensor_lux = analogRead(A0);    
    delay(1000);
    dtostrf(sensor_tem, 2, 1, sensor_tem_c);
    dtostrf(sensor_hum, 2, 1, sensor_hum_c);
    dtostrf(sensor_lux, 3, 1, sensor_lux_c);
}

void updateSensorData() 
{
  wifi.createTCP(HOST_NAME, HOST_PORT);           //建立TCP连接，用于发送数据
  
  //if (wifi.createTCP(HOST_NAME, HOST_PORT)) { 
    //Serial.print("create tcp ok\r\n");
//_______________________________________________________________________________________此处强调用局部变量，用全局变量会导致内存溢出
String jsonToSend;                                //用于存储已得数据的字符串，局部变量
String postString;                                //用于存储发送数据的字符串，局部变量

    jsonToSend="{\"Temperature\":";
    dtostrf(sensor_tem,1,2,buf);
    jsonToSend+="\""+String(buf)+"\"";
    jsonToSend+=",\"Humidity\":";
    dtostrf(sensor_hum,1,2,buf);
    jsonToSend+="\""+String(buf)+"\"";
    jsonToSend+=",\"Light\":";
    dtostrf(sensor_lux,1,2,buf);
    jsonToSend+="\""+String(buf)+"\"";
    jsonToSend+="}";

    postString="POST /devices/";
    postString+=DEVICEID;
    postString+="/datapoints?type=3 HTTP/1.1";
    postString+="\r\n";
    postString+="api-key:";
    postString+=apiKey;
    postString+="\r\n";
    postString+="Host:api.heclouds.com\r\n";
    postString+="Connection:close\r\n";
    postString+="Content-Length:";
    postString+=jsonToSend.length();
    postString+="\r\n";
    postString+="\r\n";
    postString+=jsonToSend;
    postString+="\r\n";
    postString+="\r\n";
    postString+="\r\n";

  const char *postArray = postString.c_str();                 //将str转化为char数组
  Serial.println(postArray);
  wifi.send((const uint8_t*)postArray, strlen(postArray));    //send发送命令，参数必须是这两种格式，尤其是(const uint8_t*)
  Serial.println("send success");   
     /*if (wifi.releaseTCP()) {                               //因常出现release err，导致通信不稳定，释放TCP连接没有执行
        Serial.print("release tcp ok\r\n");
        } 
     else {
        Serial.print("release tcp err\r\n");
        }*/
      postArray = NULL;                                       //清空数组，等待下次传输数据
  
  //} else {
    //Serial.print("create tcp err\r\n");
  //}
}
