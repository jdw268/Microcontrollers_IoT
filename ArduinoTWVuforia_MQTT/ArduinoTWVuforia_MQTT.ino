//This program uses MQTT to send and recieve information about an LED. 

#include <ESP8266WiFi.h> // Enables the ESP8266 to connect to the local network (via WiFi)
#include <PubSubClient.h> // Allows us to connect to, and publish to the MQTT broker


const int buttonPinStatus = D7; //pin to read from push button - input pin
const int ledOnOff = D8;  //pin to write to for LED - output pin
const int ledVoltagePin = D2;  //reads input voltage to LED - input pin
const int supplyVoltagePin = D3; //read supply voltage to microcontrol - input pin

//global to keep track if led on or off - initialize to zero
int LEDStatus;
//int TWStatus;

// MQTT broker setup
const char* mqtt_server = "xxx.xxx.x.xx";  //this is on RPi
const char* mqtt_topicLED = "TWTestLED";  //this topic is related to the LED status
const char* mqtt_topicLEDVoltage = "LEDVoltage"; //this topic is used for NodeMCU to publish voltages to TW
const char* mqtt_topicSupplyVoltage = "SupplyVoltage"; //this topic is used for NodeMCU to publish voltages to TW

// The client id identifies the ESP8266 device
const char* clientID = "ESP8266";

// Initialise the WiFi and MQTT Client objects
WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, wifiClient); // 1883 is the listener port for the Broker

//function declarations
void toggleLED(char updatedFrom);

/*
 * This function will be called each time board receives a message.
 * It's a subscriber to the topic.  Payload is the message. Length is size of message
 */
void receivedMessage(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");

  //Update the pin that controls the LED based on input message but only if it's a change
  //if the first character is a 1 then update the pin to be HIGH
  if (((char)payload[0] == '1') && (LEDStatus != 1)) {
    //update the LED to be HIGH
    digitalWrite(ledOnOff, HIGH);   
    
    //update the ledstatus variable
    LEDStatus = 1;

    //update ThingWorx on supply voltage and LED voltage too
   sendVoltageUpdate();
    
   } else if (((char)payload[0] == '0')&& (LEDStatus != 0)){
    //update the LED to be HIGH
    digitalWrite(ledOnOff, LOW);  
    LEDStatus = 0;

    //update ThingWorx on supply voltage and LED voltage too
   sendVoltageUpdate();
    
  } else{
    Serial.println("Invalid input or redundant input");
  }

} //end messageCallback function definition


//run at board startup
void setup() {
  //debuggging tool
  Serial.begin(9600);
  Serial.printf("Starting...\n");
  
  LEDStatus=0;

  //initialize the pins' modes
  pinMode(buttonPinStatus, INPUT);  //pin that is read for Button press status - D7

 /*attach interrupt to the button pint status so that change will take affect immediately
   *issue with this is the interrupt is two edges! rising and falling because button clicked then released
   so use the loop and hold the button (update once get button that holds the state)
  //pin, callback function, interrupt type/mode
  //attachInterrupt(buttonPinStatus, toggleLED, CHANGE);
*/

  pinMode(ledOnOff, OUTPUT);  //pin that either powers on/off LED - pin D8

  //set the led to be off to start
  digitalWrite(ledOnOff, LOW);

  //set pinmodes for voltages
  pinMode(supplyVoltagePin, INPUT);  //D2
  pinMode(ledVoltagePin, INPUT);  //D3
  
  //attachInterrupt(ledVoltage, sendUpdate, CHANGE);

  //connect board to wifi call with 10 second timeout
  connectToWiFi(20);

  //subscribe to messages of a topic mqtt_topicLED (from TW)
  client.subscribe(mqtt_topicLED);
  
  //set the callBack function to message Received function to handled subscriptions
  client.setCallback(receivedMessage);

  // Connect to MQTT Broker
  //more input parameters for connect function if user name and password needed
  if (connectMQTT()) {
    Serial.println("Connected Successfully to MQTT Broker!");  
  }
  else {
    Serial.println("Connection Failed!");
  }

} //end setup


void loop() {
  // If the connection is lost, try to connect again
  if (!client.connected()) {
    connectMQTT();
  }
 
  // client checks for messages
  client.loop();
  // Once it has done all it needs to do for this cycle, go back to checking if we are still connected.

delay(2000);

  //allow for board button press to toggle the current state of the LED and tell TW
  //since the board and TW are both publishers and subscribers...TW will update based on publish
  //TW will also resend a message but nothing will happen on the reception other than the printed result
  
  if (digitalRead(buttonPinStatus) == HIGH){  //this pin status is pulled to 0 with pull-down resistor - only goes high with held in button press
    //check current status of the pin
    //do the opposite of the current status
    if(LEDStatus == 1){
      //turn-off the led
       digitalWrite(ledOnOff, LOW);   
    
      //update the ledstatus variable
      LEDStatus = 0;

      //update TW with change
      client.publish(mqtt_topicLED, "0");
    }
    else{
      //turn-on the led
       digitalWrite(ledOnOff, HIGH);   
    
      //update the ledstatus variable
      LEDStatus = 1;

     //update TW with change
      client.publish(mqtt_topicLED, "1");
      }

   //update ThingWorx on supply voltage and LED voltage too
   sendVoltageUpdate();
   
} //end digitalRead if   

}  //end loop


//This is a helper method to send ThingWorx voltage information with every change on the LED
void sendVoltageUpdate(){
  Serial.println("sending voltage update to TW");
  int supplyVoltage = digitalRead(supplyVoltagePin);
  int ledVoltage = digitalRead(ledVoltagePin);
  
  //fake output b/c only one analog pin
  if(supplyVoltage == 1){
    client.publish(mqtt_topicSupplyVoltage, "3.3V");
    }
   else{
    client.publish(mqtt_topicSupplyVoltage, "0.0V");
   }
   
   if(ledVoltage == 1){
    client.publish(mqtt_topicLEDVoltage, "3.3V");
    }
   else{
     client.publish(mqtt_topicLEDVoltage,  "0.0V");
   }
   
  }


/*
  Description:  Attempts to connect to MQTT server/broker. 
  Inputs:  timeout length
  Return:  boolean
*/
bool connectMQTT() {
  // Connect to MQTT Server and subscribe to the topic
  if (client.connect(clientID)) {
      
      //also subscribe to topic
      client.subscribe(mqtt_topicLED);
      return true;
    }
    else {
      return false;
  }
}

/*
  Description:  Attempt to make a WiFi connection. Checks if connection has been made 
  once per second until timeout is reached returns TRUE if successful or FALSE if timed out
  Inputs:  timeout length
  Return:  boolean
*/
boolean connectToWiFi(int timeout) {
  delay(10);
  //set mode of wifi to STA (station mode) b/c default has an access point setup
  WiFi.mode(WIFI_STA);
  WiFi.begin("xxx", "xxxpassword");

  Serial.println("Connecting...");

  // loop while WiFi is not connected waiting one second between checks
  uint8_t tries = 0; // counter for how many times we have checked

  // stop checking if connection has been made OR we have timed out
  while ((WiFi.status() != WL_CONNECTED) && (tries < timeout)) {
    tries++;
    Serial.print(".");  // print . for progress bar
    Serial.println(WiFi.status());
    delay(500);
  }

  //let user know if board connected
  if (WiFi.status() == WL_CONNECTED) { //check that WiFi is connected, print status and device IP address before returning
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else { //if WiFi is not connected we must have exceeded WiFi connection timeout
    return false;
  }

}
