#include<stdlib.h>

#include <crc16.h>
#include <espduino.h>
#include <FP.h>
#include <mqtt.h>
#include <rest.h>
#include <ringbuf.h>

#include <SoftwareSerial.h>

#include <Timer.h>

//#include <Servo.h>
#include <ServoTimer1.h>


#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 5

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);


//Servo servo;
ServoTimer1 servo;
//Servo servo2;
 
// Pin 13 has an LED connected on most Arduino boards.
// give it a name:
int led = 13;

int pos = 0; 

SoftwareSerial debugPort(2, 3); // RX, TX
ESP esp(&Serial, /*&debugPort,*/ 4);

MQTT mqtt(&esp);

boolean wifiConnected = false;

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
  mqtt.publish("/hubacek/status", "start");

}
void mqttDisconnected(void* response)
{
  debugPort.print("Disconnected!");
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

}

// the setup routine runs once when you press reset:
void setup() {                
  // initialize the digital pin as an output.
  pinMode(led, OUTPUT); 

servo.attach(9);  
//servo2.attach(8);

Serial.begin(19200); 
debugPort.begin(19200);

debugPort.println("ARDUINO: esp reset start");

esp.enable();
  delay(500);
  esp.reset();
  delay(500);
  while(!esp.ready());
  
  debugPort.println("ARDUINO: setup mqtt client");
  if(!mqtt.begin("DVES_duino", "admin", "Isb_C4OGD4c3", 120, 1)) {
    debugPort.println("ARDUINO: fail to setup mqtt");
    while(1);
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
  
  
  sensors.begin();
 
}

uint32_t timestamp = 0;

// The interrupt will blink the LED, and keep
// track of how many times it has blinked.
int ledState = LOW;
volatile unsigned long blinkCount = 0; // use volatile for shared variables

void blinkLED(void)
{
  if (ledState == LOW) {
    ledState = HIGH;
    blinkCount = blinkCount + 1;  // increase when LED turns on
  } else {
    ledState = LOW;
  }
  digitalWrite(led, ledState);
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


 char charVal[10]; 

// the loop routine runs over and over again forever:
void loop() {
 
  
  esp.process();
  
  if(millis() - timestamp > 5000)
  {
    sensors.requestTemperatures();
    float t = sensors.getTempCByIndex(0);
    
    dtostrf(t, 4, 1, charVal); 
    debugPort.print("Temp: ");
    debugPort.println(charVal);
    
    mqtt.publish("/hubacek/temp", charVal);
    timestamp = millis();
  }

   
  
}
