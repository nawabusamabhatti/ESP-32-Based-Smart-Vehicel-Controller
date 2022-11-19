
#include "WiFi.h"
#include<HTTPClient.h>
const char* ssid = "";
const char* password = "";
WiFiServer server(80);
String header;

unsigned long currentTime = millis();
unsigned long previousTime = 0; 
const long timeoutTime = 2000;

int m1 =  23;
int m2 = 32;

int power = 22;
int rpm = 110;
int br1 = 27;
int br2 = 14;
int j_main = 25;
int j_1 = 33;
int j_2 = 26;
int flight = 12;

int relays[] = {m1,m2,power,br1,br2,j_main,j_1,j_2,flight};
int relays_size = 9;
int unused[] = {12};
int unused_size = 0;

float angle = 90;

int steering_angle_pin = 39;
int target_steering_value = -1;
int last_target_steering_value = -1;
int steering_connected = 1;

int left_bound = -900;
int right_bound = 2400;

int motor_span = 40;
int max_block_time = 1000; 
int t_l = 0;
int t_r = 0;


unsigned long last_beat_time=0;
int max_allowed_ping = 1000;
int ping = 0;

const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;

bool show_wifi_log = true;
bool serial_off = true;
TaskHandle_t Task1;
TaskHandle_t Task2;

void setup() 
{
  Serial.begin(115200);
  
  
  for(int i=0;i< relays_size;i++)
  {
    pinMode(relays[i], OUTPUT);
    digitalWrite(relays[i],LOW);
  }
  
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(power, ledChannel);
  ledcWrite(ledChannel, 0);
    
  for(int i=0;i<unused_size;i++)
  {
    pinMode(unused[i],OUTPUT);
    digitalWrite(unused[i],LOW);
  }

  

  Serial.print("Connecting to ");
  Serial.println(ssid);
  Serial.println("send ? to skip connecting");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
    if(Serial.available())
    {
      char tc = (char)Serial.read();
      if(tc == '?')
      {
        Serial.println("Skipping Wifi Connection Optionally.");
        break;
      }
    }
  }
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED) 
  {
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());



    IPAddress ipAddress = WiFi.localIP();
    String str_ip = String(ipAddress[0]) + String(".") + String(ipAddress[1]) + String(".") + String(ipAddress[2]) + String(".") + String(ipAddress[3]);
    String serverName = "" + String(str_ip);
    HTTPClient http;
    String serverPath = serverName;
      
    http.begin(serverPath.c_str());
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode>0) {
      Serial.print("IP Uploaded...");
    }
    else {
      Serial.print("Error IP Uploading code: ");
      Serial.println(httpResponseCode);
    }
    http.end();




    
    server.begin();
  }
  else
  {
    Serial.println("using wired connection only");
  }
delay(500);
  xTaskCreatePinnedToCore(
                    Task1code,  
                    "Task1",    
                    10000,       
                    NULL,        
                    1,           
                    &Task1,    
                    0);                        
  delay(1000); 
  xTaskCreatePinnedToCore(
                    Task2code,  
                    "Task2",   
                    10000,       
                    NULL,      
                    1,        
                    &Task2,      
                    1);          
    delay(500); 
    Serial.println("Cores Created Successfully");
    Serial.println("You can now continue using the data over the usb");
}

void Task1code(void * pvParameters){

  while(true)
  {
    loop();
    vTaskDelay(1);

    unsigned long current_stamp = millis();
    ping = current_stamp;
  
    WiFiClient client = server.available();  

    if (client) 
    {                        
      currentTime = millis();
      previousTime = currentTime;
      if(show_wifi_log && !serial_off)
        Serial.println("New Client.");          
      String currentLine = "";               
      while (client.connected() && currentTime - previousTime <= timeoutTime) {  
        currentTime = millis();
        if (client.available()) {            
          char c = client.read();             
          if(show_wifi_log && !serial_off)
            Serial.write(c);                    
          header += c;
          if (c == '\n') 
          {                    
            if (currentLine.length() == 0) 
            {
              
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/json");
              client.println("Access-Control-Allow-Origin: *");
              client.println("Access-Control-Allow-Headers: *");
              client.println("Access-Control-Allow-Methods: *");
              client.println("Connection: close");
              client.println();

              client.println(String("{\"angle\":") + int(angle) + String(",\"rpm\":") + int(rpm) + String(",\"ping\":") + String(ping) + String("}")); 
              Serial.println( "angle" + int(angle));
              if(show_wifi_log)
              {
                  Serial.println(header);
                }
              int idx = header.indexOf("/val/");
              int idx2 = header.indexOf("/angle/");
              
              if (idx >= 0) 
              {
                String chr = header.substring(idx + 5, idx + 6);
                if(show_wifi_log)
                {
                  Serial.println(chr[0]);
                }
                process_input(chr[0]);
              } 
              else if(idx2 >= 0)
              {
                int idx3 = header.indexOf("a",idx2 + 7);
                if(idx3 > 0)
                {
                  String chr_angle = header.substring(idx2 + 7, idx3);
                  target_steering_value = chr_angle.toInt();
                  last_target_steering_value = -1;
                }
              }
              
              break;
            } else { 
              currentLine = "";
            }
          } 
          else if (c != '\r') { 
            currentLine += c;     
          }
        }
      }
      header = "";
      client.stop();
      if(show_wifi_log && !serial_off)
      {
        Serial.println("Client disconnected.");
        Serial.println("");
      }
    }
  
    if(Serial.available())
    {
      char c = (char)Serial.read();
      if(c=='~')
      {
        int temp = Serial.parseInt();
        if(temp > 10)
        {
          target_steering_value = temp;
          last_target_steering_value = -1;
          Serial.println(target_steering_value);
        }
      }
      else
      {
        process_input(c);      
      }
      
    }
    
  } 
}

void Task2code(){

  int last_cycle_time = millis();
  while(true)
  {
    update_angle_value();
    int cycle_time = millis() - last_cycle_time;
    last_cycle_time = millis();
    if (t_l == 1 && angle < left_bound && steering_connected==1)
    {
      digitalWrite(m1, LOW);
      digitalWrite(m2, LOW);
      t_l = 0;
    }
 
    if (t_r == 1 && angle > right_bound && steering_connected==1)
    {
      digitalWrite(m1, LOW);
      digitalWrite(m2, LOW);
      t_r = 0;
    }
    if(!serial_off)
      Serial.println(String("(") + int(angle) + String(",") + int(rpm) + String(",") + String(cycle_time) + String(")")); // sample output = "(320,85)" // sending 2 values to serial. angle + current rpm.
    set_steering();
  } 
}

void update_angle_value()
{
  float analogValue = analogRead(steering_angle_pin);
  angle = 3000 - analogValue; 
}

void loop() 
{
}


void process_input(char c)
{
  switch(c)
  {
      case 'w':
          ledcWrite(ledChannel);
      break;
      case 's':
          ledcWrite(ledChannel, 0);
      break;
      case 'a':
          t_l = 1;
          if (angle > left_bound || steering_connected==0)
          {
              digitalWrite(m1, HIGH);
              digitalWrite(m2, LOW);
          }
      break;
      case 'd':
          t_r = 1;
          if (angle < right_bound || steering_connected==0)
          {
          digitalWrite(m1, LOW);
          digitalWrite(m2, HIGH);
          }
      break;
      case 'f':
          t_l = 0;
          t_r = 0;
          digitalWrite(m1, LOW);
          digitalWrite(m2, LOW);
      break;
      case 't':
          rpm += 5;
          if (rpm > 200)
              rpm = 200;
          ledcWrite(ledChannel, rpm);
      break;
      case 'g':
          rpm -= 5;
          if (rpm < 70)
              rpm = 70;
          ledcWrite(ledChannel, rpm);
      break;

      case '[':
          digitalWrite(br1, LOW);
          digitalWrite(br2, HIGH);
      break;
      case ']':
          digitalWrite(br1, HIGH);
          digitalWrite(br2, LOW);
      break;
      case 'p':
          digitalWrite(br1, LOW);
          digitalWrite(br2, LOW);
      break;

      case '8':
          digitalWrite(j_main, HIGH);
          digitalWrite(j_1, HIGH);
          digitalWrite(j_2, LOW);
      break;
      case '9':
          digitalWrite(j_main, HIGH);
          digitalWrite(j_1, LOW);
          digitalWrite(j_2, HIGH);
      break;
      case '0':
          digitalWrite(j_main, LOW);
          digitalWrite(j_1, LOW);
          digitalWrite(j_2, LOW);
      break;
      case 'l':
          digitalWrite(flight, HIGH);
      break;
      case 'm':
          digitalWrite(flight, LOW);
      break;
      case 'z': 
          last_beat_time = millis();
      break;

      default:
         
      break;
  
  }
}

void stop_all()
{
    for(int i=0;i< relays_size;i--)
    {
      digitalWrite(relays[i],LOW);
    }
}

void set_steering()
{
  int current_time_block = millis();

  
  if(target_steering_value != last_target_steering_value)
  {
    last_target_steering_value = target_steering_value;
    while(true)
    {
      update_angle_value();
      if(angle < target_steering_value - motor_span)
      {
        if (angle < right_bound || steering_connected==0)
        {
          digitalWrite(m1, LOW);
          digitalWrite(m2, HIGH);
        }
        else
        {
          digitalWrite(m1, LOW);
          digitalWrite(m2, LOW);
          break;
        }
      }
      else if(angle > target_steering_value + motor_span)
      {
        if (angle > left_bound || steering_connected==0)
        {
          digitalWrite(m1, HIGH);
          digitalWrite(m2, LOW);
        }
        else
        {
          digitalWrite(m1, LOW);
          digitalWrite(m2, LOW);
          break;
        }
      }
      else
      {
        digitalWrite(m1, LOW);
        digitalWrite(m2, LOW);
        break;
      }
      if(millis() - current_time_block > max_block_time)
      {
        digitalWrite(m1, LOW);
        digitalWrite(m2, LOW);
        break;
      }
    }
  }
  
  
}
