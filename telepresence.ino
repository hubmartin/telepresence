#include <stdlib.h>
#include <crc16.h>
#include <espduino.h>
#include <FP.h>
#include <mqtt.h>
#include <rest.h>
#include <ringbuf.h>
#include <SoftwareSerial.h>
#include <ServoTimer1.h>
#include <avr/wdt.h>
#include <string.h>


// EXAMPLE CONFIGURATION
// ---------------------------

// Enable DS18B20 temperature
//#define TEMPERATURE

// Enable sleep after temperature send
//#define POWER_DOWN

// ---------------------------


#ifdef TEMPERATURE
  #include <OneWire.h>
  #include <DallasTemperature.h>
  
  // Data wire is plugged into pin 2 on the Arduino
  #define ONE_WIRE_BUS 5
  // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
  OneWire oneWire(ONE_WIRE_BUS);

  // Pass our oneWire reference to Dallas Temperature. 
  DallasTemperature sensors(&oneWire);
#endif

uint32_t timestamp = 0;

ServoTimer1 servo;
// Pin 13 has an LED
int led = 13;

// Servo actual position
int pos = 0; 

// Software debug serial port
SoftwareSerial debugPort(3, 2); // RX, TX

// Link ESP to HW USART0
ESP esp(&Serial, /*&debugPort,*/ 4);


// MQTT instance
MQTT mqtt(&esp);

// Some helper flags
boolean wifiConnected = false;
boolean mqttConnectedFlag = false;

// Wifi callback
void wifiCb(void* response)
{
  uint32_t status;
  RESPONSE res(response);

  if(res.getArgc() == 1) {
    res.popArgs((uint8_t*)&status, 4);
    if(status == STATION_GOT_IP) {
      debugPort.println("WIFI CONNECTED");
      //mqtt.connect("192.168.100.140", 1883);
      mqtt.connect("iot.eclipse.org", 1883);
      wifiConnected = true;
      //or mqtt.connect("host", 1883); /*without security ssl*/
    } else {
      wifiConnected = false;
      //mqtt.disconnect();
    }

  }
}

void mqttConnected(void* response)
{
  debugPort.println("Connected");
  mqtt.subscribe("/hubacek/servo"); //or mqtt.subscribe("topic"); /*with qos = 0*/
  mqtt.subscribe("/hubacek/led");
  //mqtt.publish("/hubacek/status", "start");
  mqttConnectedFlag = true;
}


void mqttDisconnected(void* response)
{
  debugPort.print("Disconnected!");
  mqttConnectedFlag = false;
  
  /*
  #ifdef POWER_DOWN
  esp.disable();
  wdtlib_pwr_down(WDTO_4S);

  setup();
  #endif
  */
}

void mqttData(void* response)
{
  RESPONSE res(response);

  debugPort.print("Received: topic=");
  String topic = res.popString();
  debugPort.println(topic);

  debugPort.print("data=");
  String data = res.popString();
  debugPort.println(data);
  
  
  if(topic.indexOf("/servo") >= 0)
  {
    debugPort.print("Servo");
    pos = data.toInt();
  }
  
  if(topic.indexOf("/led") >= 0)
  {
    debugPort.print("LED");
    digitalWrite(led, data.toInt());
  }
  

}
void mqttPublished(void* response)
{
  
   debugPort.println("Published");
   
  if(mqttConnectedFlag == false)
  {
     debugPort.println("mqttConnectedFlag false returning");
     return;
  }
  
 
  // Experimental power down after temperature send
  #ifdef POWER_DOWN
  
  debugPort.println("MQTT disconnecting, powering down");
  mqtt.disconnect();
    
  mqttConnectedFlag = false;
  
  digitalWrite(13, HIGH);
  delay(100);
  digitalWrite(13, LOW);

  esp.disable();
  wdtlib_pwr_down(WDTO_2S);
  
  setup();
  #endif
  
}


boolean setupESP()
{
  
debugPort.println("ARDUINO: esp reset start");

esp.enable();
  delay(1000);
  esp.reset();
  delay(500);
  while(!esp.ready());
  
  debugPort.println("ARDUINO: setup mqtt client");
  if(!mqtt.begin("DVES_duino", "admin", "Isb_C4OGD4c3", 120, 1)) {
    debugPort.println("ARDUINO: fail to setup mqtt");
    return 1;
  }
  
  /*setup mqtt events */
  mqtt.connectedCb.attach(&mqttConnected);
  mqtt.disconnectedCb.attach(&mqttDisconnected);
  mqtt.publishedCb.attach(&mqttPublished);
  mqtt.dataCb.attach(&mqttData);
  
  /*setup wifi*/
  debugPort.println("ARDUINO: setup wifi");
  esp.wifiCb.attach(&wifiCb);
  esp.wifiConnect("Internet","heslojeheslo");
  debugPort.println("ARDUINO: system started");
   
   return 0;
}

// the setup routine runs once when you press reset:
void setup() {                
  // initialize the digital pin as an output.
  pinMode(led, OUTPUT); 
  
  servo.attach(9);  
  
  Serial.begin(19200); 
  debugPort.begin(19200);
  
  while(setupESP() == 1)
  {
      debugPort.println("Trying reinit ESP again in 1sec...");
      esp.disable();
      delay(1000);
  }

   
   //set timer2 interrupt at 8kHz
  TCCR2A = 0;// set entire TCCR2A register to 0
  TCCR2B = 0;// same for TCCR2B
  TCNT2  = 0;//initialize counter value to 0
  // set compare match register for 8khz increments
  OCR2A = 249;// = (16*10^6) / (8000*8) - 1 (must be <256)
  // turn on CTC mode
  TCCR2A |= (1 << WGM21);
  // Set CS21 bit for 8 prescaler
  TCCR2B |= (1 << CS21);   
  // enable timer compare interrupt
  TIMSK2 |= (1 << OCIE2A);
  
  #ifdef TEMPERATURE
    sensors.begin();
  #endif
  
  timestamp = millis();
 
}



ISR(TIMER2_COMPA_vect){//timer1 interrupt 8kHz toggles pin 9
//generates pulse wave of frequency 8kHz/2 = 4kHz (takes two cycles for full wave- toggle high then toggle low)
  static int i = 0;
  static int servoActPos = 0;
  i++;
  // 8 1ms
  // 80 10ms
  // 800 100ms
  if(i == 160)
  {
      i = 0;
      
    if(pos > servoActPos)
    {
     servoActPos++;
     servo.write(servoActPos);
    } 
    
    if(pos < servoActPos)
    {
      servoActPos--;
      servo.write(servoActPos);
    } 
  }
}


 

// the loop routine runs over and over again forever:
void loop() {
  // Small buffer for float to char conversion
  #define CHAR_BUFFER_SIZE 20
  static char charVal[CHAR_BUFFER_SIZE]; 
  
  esp.process();
  
  if(millis() - timestamp > 5000 && mqttConnectedFlag)
  {
    #ifdef TEMPERATURE
      sensors.requestTemperatures();
      float t = sensors.getTempCByIndex(0);
      
      dtostrf(t, 4, 1, charVal); 
      debugPort.print("Temp: ");
      debugPort.println(charVal);
    #else
      strncpy(charVal, "Hello MQTT!", CHAR_BUFFER_SIZE);
    #endif
    
    mqtt.publish("/hubacek/temp", charVal);
    timestamp = millis();
  }
 
}
