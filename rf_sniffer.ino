/*
  Derived from - Simple example for receiving
  
  https://github.com/sui77/rc-switch/


  Using https://github.com/ThingPulse/esp8266-oled-ssd1306 for OLED
*/

#include <RCSwitch.h>
#include <EEPROM.h>

#include <Wire.h>
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`

// The data object that holds the readings
struct codes {
  long value;             // the code value in decimal
  byte len;               // number of bits in the data
  byte protocol;          // protocol number
  int period;             // what rc-switch calls 'delay'
};

// Momentary push-button. 
// Short press lists memory contents. Long press erases memory
byte switch_pin = 12;

// Interrupt pin for receiver. 
//I found that, with the 3400 Superhet receivers, the default pin 0 did not work
byte interrupt_pin = 13;  

// Initialize the OLED display using Wire library
SSD1306Wire  display(0x3c, D2, D1);   // This uses WeMos/NodeMCU notation

byte EEPROM_length = 160;                           // Nominal storage size
int eeAddr = 0;                                     // Pointer into EEPROM

RCSwitch mySwitch = RCSwitch();                     // New RC Switch object

void setup() {
  pinMode(switch_pin, INPUT_PULLUP);                // Enable  input for momentary switch
  Serial.begin(9600);
  Serial.println("Starting");
  
  display.init();                                   // Initialise the OLED
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_16);
  
  EEPROM.begin(EEPROM_length);                      // Initialise the ESP8266 pseudo EEPROM
  eeprom_dump();                                    // List the contents of the memory
  mySwitch.enableReceive(13);                       // Receiver on interrupt pin #13

  // Set up screen for start
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.clear();
  display.drawString(64, 25, "Waiting for data");
  display.display();
}

bool switch_state = false;
long now;

void loop() {
  if (mySwitch.available()) {
    
    long value = mySwitch.getReceivedValue();
    
    if (value == 0) {
      Serial.print("Unknown encoding");
    } else {
        
      codes myVar = {                                 // initialise the data object
        mySwitch.getReceivedValue(),
        mySwitch.getReceivedBitlength(),
        mySwitch.getReceivedProtocol(),
        mySwitch.getReceivedDelay()
      };
      EEPROM.put(eeAddr, myVar);                      // Write the data object to 'EEPROM'
      EEPROM.commit();                                // Commit the write
      eeAddr += sizeof(myVar);                        // increment the address pointer

      // If we've got to the end of memory, reset the pointer
      if (eeAddr >EEPROM_length - sizeof(myVar)) eeAddr = 0;
      
      print_codes(myVar);                             // Print the received data to the serial port
      display_codes(myVar);                           // Show the received data on the OLED
    }

    mySwitch.resetAvailable();                        // Now we have the data, clear the flag
  }

  
  if (!digitalRead(switch_pin)) {                     // Momentary switch is pressed
    if (!switch_state) {                              // new press
      switch_state = true;
      now = millis();                                 // log the start time
      Serial.println("Switch pressed");
    }
  }else{                                              // momentary switch is open
    if (switch_state) {                               // just released
      switch_state = false;
      if ( (millis() - now) > 5000 ) {                // long press
        eeprom_erase();                               // erase the memory               
        display.clear();                              // Put up the 'Memory Clear' message for a couple of seconds
        display.setTextAlignment(TEXT_ALIGN_CENTER);
        display.drawString(64, 5, "Memory Clear");
        delay(2000);
      }
      if ( (millis() - now) > 200 ) {                 // at least a short press
        eeprom_dump();                                // display the memory contents to Serial and OLED
        display.clear();                              // Put up the 'waiting' message on OLED
        display.setTextAlignment(TEXT_ALIGN_CENTER);
        display.drawString(64, 25, "Waiting for data");
        display.display();
      }
    }
  }
}

void print_codes(codes myVar) {
  // Print the contents of a data object to Serial
  Serial.print("Received ");
  Serial.print(myVar.value );
  Serial.print(' ');
  Serial.print(padded_binary(myVar.value));
  Serial.print(" / ");
  Serial.print( myVar.len );
  Serial.print("bit ");
  Serial.print("Protocol: ");
  Serial.print( myVar.protocol );
  Serial.print(" Period: ");
  Serial.println( myVar.period );
}

void display_codes(codes myVar) {
  // Print the contents of a data object to OLED
  byte lines[] = {0, 20, 40};                             // Y co-ordinate for lines
  display.clear();
  
  // Print the labels
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, lines[0], "Code:");
  display.drawString(0, lines[1], "Bits:");
  display.drawString(0, lines[2], "Period:");

  // Print the values
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(128, lines[0], String(myVar.value));
  display.drawString(128, lines[1], String(myVar.len) + '/' + String(myVar.protocol));
  display.drawString(128, lines[2], String(myVar.period));
  
  display.display();
}

String int_to_string(int i, byte pad) {
  // Convert an integer to string and pad with leading zeroes to size 'pad'
  String result = "";
  for (byte i=0; i<pad;i++) {                             // fill the whole string with zeroes
    result += "0";
  }
  result += String(i);                                    // add on the integer as a String
  return result.substring(result.length()-pad);           // truncate to size of pad
}

String padded_binary(long val) {
  // convert long integer to 24-bit binary string padded with leading zeroes
  String str = "";                                        // start with a blank string
  for (byte i=0; i<24; i++){                              // for 24 bytea
    if ( val & 0x01) {                                    // test the lsb
      str = '1' + str;                                    // add '1' if set
    }else{
      str = '0' + str;                                    // add '0' if clear
    }
    val  >>= 1;                                           // shift right one so that next bit is lsb
    if (i % 4 == 3) str = ' ' + str;                      // chop up with spaces for readability
  }
  return str;                                             // return the result e.g. 0110 1011 1001 1001 1100 1000
}

void eeprom_erase() {
  Serial.println("Erasing data");                         // Show the 'erasing data' message on Serial
  display.setTextAlignment(TEXT_ALIGN_CENTER);            // Show the 'erasing data' message on OLED
  display.clear();
  display.drawString(64, 25, "Erasing data");
  display.display();
  
  for (byte i=0; i<EEPROM_length; i++) {                  // Write 0xff into whole memory
    EEPROM.write(i, 0xff);
  }
  EEPROM.commit();                                        // Save it away to flash
  eeAddr = 0;                                             // reset the memory pointer

}

void eeprom_dump() {
  // Dump out the contents of the memory to Serial and OLED
  codes myVar {                                           // make a variable to get data length
    -1,
    -1,
    -1,
    -1
  };
  
  codes var_array[] = {myVar, myVar, myVar, myVar};       // make an array of four data objects
  
  display.setTextAlignment(TEXT_ALIGN_LEFT);              // left align oled text
  display.setFont(ArialMT_Plain_10);                      // use 10pt
  
  int count[4] = {0, 0, 0, 0};                            // line counter
  
  // step through memory in chunks of size of data object
  for (int i=0; i < EEPROM_length - sizeof(myVar); i += sizeof(myVar)){   
    EEPROM.get(i , myVar);
    if (myVar.value < 0) break;
    var_array[0] = var_array[1];    count[0] = count[1];
    var_array[1] = var_array[2];    count[1] = count[2];
    var_array[2] = var_array[3];    count[2] = count[3];
    var_array[3] = myVar;           count[3]++;
    
    
    display.clear();   
    
    for (int j=0; j< 4; j++) {                            // display the next four lines
      if (var_array[j].value >= 0) {
        String valueString = int_to_string(count[j], 2);
        valueString += ' ';
        valueString += int_to_string(var_array[j].value,8);
        valueString += ' ';
        valueString += String(var_array[j].len);
        valueString += ' ';
        valueString += String(var_array[j].protocol);
        valueString += ' ';
        valueString += String(var_array[j].period);
        display.drawString(0, (15*j + 5), valueString );
      }
    }
    display.display();
    print_codes(myVar);                                 // print the data to Serial
    delay(2000);                                           // wait a second
  }
  display.setFont(ArialMT_Plain_16);                       // restore the font
}
