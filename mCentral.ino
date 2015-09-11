/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * Example using Dynamic Payloads 
 *
 * This is an example of how to use payloads of a varying (dynamic) size. 
 */

#define MQTTCLIENT_QOS2 1

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

#include <Ethernet.h>

#include <EthernetStack.h>
#include <Countdown.h>
#include <MQTTClient.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

IPAddress ip(192, 168, 100, 177);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10

RF24 radio(9,53);

// sets the role of this unit in hardware.  Connect to GND to be the 'pong' receiver
// Leave open to be the 'ping' transmitter
const int role_pin = 7;
int led = 13;
const int intrpt = 5;

//
// Topology
//

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[6] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL, 0xF0F0F0F0C3LL, 0xF0F0F0F0B4LL, 0xF0F0F0F0A5LL, 0xF0F0F0F0A6LL };

//
// Role management
//
// Set up role.  This sketch uses the same software for all the nodes
// in this system.  Doing so greatly simplifies testing.  The hardware itself specifies
// which node it is.
//
// This is done through the role_pin
//

// The various roles supported by this sketch
typedef enum { role_ping_out = 1, role_pong_back } role_e;

// The debug-friendly names of those roles
const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};

// The role of the current running sketch
role_e role;

//
// Payload
//

const int min_payload_size = 4;
const int max_payload_size = 32;
const int payload_size_increments_by = 2;
int next_payload_size = min_payload_size;

char receive_payload[max_payload_size+1]; // +1 to allow room for a terminating NULL char


//MQTT
unsigned long interval                = 60000;
unsigned long previousMillis          = 0;

char printbuf[100];

int arrivedcount = 0;

void messageArrived(MQTT::MessageData& md){
  MQTT::Message &message = md.message;
  
  sprintf(printbuf, "Message %d arrived: qos %d, retained %d, dup %d, packetid %d\n", 
    ++arrivedcount, message.qos, message.retained, message.dup, message.id);
  
  Serial.print(printbuf);
  sprintf(printbuf, "Payload %s\n", (char*)message.payload);
  
  Serial.print(printbuf);
}


EthernetStack ipstack;
MQTT::Client<EthernetStack, Countdown> client = MQTT::Client<EthernetStack, Countdown>(ipstack);

const char* topic = "test";
const char* topic_back = "test_back";

void connect(){

  //char hostname[] = "internething.com";
  char hostname[] = "155.94.216.36";
  int port = 1883;
  
  sprintf(printbuf, "Connecting to %s:%d\n", hostname, port);
  Serial.print(printbuf);
  
  int rc = ipstack.connect(hostname, port);
  
  if (rc != 1){
    sprintf(printbuf, "rc from TCP connect is %d\n", rc);
    Serial.print(printbuf);
  }
 
  Serial.println("MQTT connecting");
  
  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
  data.MQTTVersion = 3;
  data.clientID.cstring = (char*)"COMTRADES";
  rc = client.connect(data);
  
  if (rc != 0){
    sprintf(printbuf, "rc from MQTT connect is %d\n", rc);
    Serial.print(printbuf);
  }
  Serial.println("MQTT connected");
  
  rc = client.subscribe(topic_back, MQTT::QOS2, messageArrived);   
  
  if (rc != 0){
    sprintf(printbuf, "rc from MQTT subscribe is %d\n", rc);
    Serial.print(printbuf);
  }
  
  Serial.println("MQTT subscribed");
}

// END MQTT

byte rfSetup (void) {
	 //
  // Setup and configure rf radio
  //

  radio.begin();
	radio.setDataRate(RF24_250KBPS);
  // enable dynamic payloads
  radio.enableDynamicPayloads();

  // optionally, increase the delay between retries & # of retries
  radio.setRetries(15,15);

  //
  // Open pipes to other nodes for communication
  //

  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.
  // Open 'our' pipe for writing
  // Open the 'other' pipe for reading, in position #1 (we can have up to 5 pipes open for reading)

  /*if ( role == role_ping_out )
  {
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
  }
  else
  {
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1,pipes[0]);
  }*/

    radio.openWritingPipe(pipes[1]); // -> ESCRIBE EN 0xF0F0F0F0D2LL
    
    radio.openReadingPipe(1,pipes[0]);
    radio.openReadingPipe(2,pipes[2]);
    radio.openReadingPipe(3,pipes[3]);
    radio.openReadingPipe(4,pipes[4]);
    radio.openReadingPipe(5,pipes[5]);
    

  //
  // Start listening
  //

  radio.startListening();

  //
  // Dump the configuration of the rf unit for debugging
  //

  radio.printDetails();
	
	return 0;
	
}

byte ethSetup (void) {
	Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
	
	return 0;
}

byte mqttSend (void) {
	
	arrivedcount = 0;

  // Send and receive QoS 0 message
  char buf[100];
	
	if (!client.isConnected())
    connect();

  MQTT::Message message;
	
	sprintf(buf, "Hello World! QoS 2 message");
				
	Serial.println(buf);
					
	message.qos = MQTT::QOS2;
	message.retained = false;
	message.dup = false;
			
	/*message.payload = (void *) buf;
	message.payloadlen = strlen(buf)+1;*/
			
	message.payload = (void *) receive_payload;
	message.payloadlen = strlen(receive_payload);
				 
	int rc = client.publish(topic, message);
	//while (arrivedcount == 0)
	client.yield(1000);

	return 0;
}
		

void setup(void){
  //
  // Role
  //

  // set up the role pin
  pinMode(intrpt, INPUT);
  pinMode(role_pin, INPUT);
  digitalWrite(role_pin,HIGH);
  delay(20); // Just to get a solid reading on the role pin

  // read the address pin, establish our role
  /*if ( digitalRead(role_pin) )
    role = role_ping_out;
  else
    role = role_pong_back;*/
  
  role = role_pong_back;
  
  //
  // Print preamble
  //

  Serial.begin(57600);
  printf_begin();
  printf("\n\rRF24/examples/pingpair_dyn/\n\r");
  printf("ROLE: %s\n\r",role_friendly_name[role]);

	rfSetup ();
	ethSetup ();

  //MQTT
  connect();
  //END MQTT
}

void loop(void){
  

  
  
  
  /*sprintf(buf, "Hello World! QoS 0 message");
  
  Serial.println(buf);
  
  message.qos = MQTT::QOS0;
  message.retained = false;
  message.dup = false;
  message.payload = (void*)buf;
  message.payloadlen = strlen(buf)+1;
  
  int rc = client.publish(topic, message);
  while (arrivedcount == 0)
    client.yield(1000);
        
  // Send and receive QoS 1 message
  sprintf(buf, "Hello World!  QoS 1 message");
  
  Serial.println(buf);
  
  message.qos = MQTT::QOS1;
  message.payloadlen = strlen(buf)+1;
  
  rc = client.publish(topic, message);
  while (arrivedcount == 1)
    client.yield(1000);*/
        
  // Send and receive QoS 2 message
 
  //delay(60000);

  //
  // Ping out role.  Repeatedly send the current time
  //
  
  
  
  /*if (role == role_ping_out)
  {
    // The payload will always be the same, what will change is how much of it we send.
    static char send_payload[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ789012";

    // First, stop listening so we can talk.
    radio.stopListening();

    // Take the time, and send it.  This will block until complete
    printf("Now sending length %i...",next_payload_size);
    radio.write( send_payload, next_payload_size );

    // Now, continue listening
    radio.startListening();

    // Wait here until we get a response, or timeout
    unsigned long started_waiting_at = millis();
    bool timeout = false;
    while ( ! radio.available() && ! timeout )
      if (millis() - started_waiting_at > 500 )
        timeout = true;

    // Describe the results
    if ( timeout )
    {
      printf("Failed, response timed out.\n\r");
    }
    else
    {
      // Grab the response, compare, and send to debugging spew
      uint8_t len = radio.getDynamicPayloadSize();
      radio.read( receive_payload, len );

      // Put a zero at the end for easy printing
      receive_payload[len] = 0;

      // Spew it
      printf("Got response size=%i value=%s\n\r",len,receive_payload);
    }
    
    // Update size for next time.
    next_payload_size += payload_size_increments_by;
    if ( next_payload_size > max_payload_size )
      next_payload_size = min_payload_size;

    // Try again 1s later
    delay(1000);
  }*/

  //
  // Pong back role.  Receive each packet, dump it out, and send it back
  //

  if ( role == role_pong_back )  {
    
    html ();
		radio.startListening();
		delay (100);
    
    // if there is data ready
    if ( radio.available() ){
      // Dump the payloads until we've gotten everything
      uint8_t len;
      bool done = false;
      
      while (!done){
          // Fetch the payload, and see if this was the last one.
				len = radio.getDynamicPayloadSize();
				done = radio.read( receive_payload, len );

				// Put a zero at the end for easy printing
				receive_payload[len] = 0;

				// Spew it
				printf("Got payload size=%i value=%s\n\r",len,receive_payload);

				
      }

      // First, stop listening so we can talk
      radio.stopListening();

      // Send the final one back.
      radio.write( receive_payload, len );
      printf("Sent response.\n\r");

      // Now, resume listening so we catch the next packets.
      radio.startListening();
			
			
			//BEGIN MQTT TRANSMITION -->!! 
			
			/*if ((unsigned long)(millis() - previousMillis) >= interval) {
			previousMillis = millis();*/ 
			
			mqttSend ();

      //}
			// END OF MQTT TRANSMISSION
			
    }
		
		
		
  }
}


byte html (void){
  
  //Serial.println("WEB SERVER");
   EthernetClient client = server.available();
  if (client) {
    
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // output the value of each analog input pin
          for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
            int sensorReading = analogRead(analogChannel);
            client.print("analog input ");
            client.print(analogChannel);
            client.print(" is ");
            client.print(sensorReading);
            client.println("<br />");
          }
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  return 0;
}
}
// vim:cin:ai:sts=2 sw=2 ft=cpp
