
/*
DHT11 Library provided by Virtuabotix, Author Joseph Dattilo
 https://www.virtuabotix.com/product-dht11-temperaturehumidity-sensor-msrp-9-99/dht11_2s0a/
 */

#include <dht11.h>
#include "Notes.h"
#include <SoftwareSerial.h>

#define LDR_Pin A2 //Light-dependent resistor
#define greenLED A0
#define speaker 13
#define GPSrx 9
#define GPStx 0
#define tempSens 2
#define pinLength 4 //This can be changed here to allow for a diffent code length requirement

boolean battery = true; //Set to true when setting software

//Sensor values
float temp;
float hums;
float longitude;
float latitude;
float angle;
boolean codeSet = false;
boolean alarm;
boolean locked = false;
//

float lastKeyCheck = 0;

double photoLevel = 300;

//Keypad pins
//Columns[left, mid, right]
int columns[] = {
  11,12,10};
//Rows[top, mid, bottom, symbol]
int rows[] = {
  5, 6, 8, 7};

//GPS variables
SoftwareSerial gpsSerial(GPSrx, GPStx); // RX, TX (TX not used)
const int sentenceSize = 80;
char sentence[sentenceSize];
int i = 0;


//Temp & humid varibles
dht11 DHT11;

//Keypad variables
int input;
boolean hasInput;
boolean resetMode = false;
int count = 0; //Counts input digits
int digits[pinLength];  //Points to array of input digits
int code[pinLength]; //Points to array of stored code
///End Keypad

void setup()
{
  if(!battery) {
   Serial.begin(9600);

  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  }
  //GPS Setup
  gpsSerial.begin(9600);


  //Temp Sensor Setip
  DHT11.attach(tempSens);


  //Setup keypad

  // initialize the digital pins as input_pullup.
  for (int i=0; i<sizeof(columns)/sizeof(int); i++){
    pinMode(columns[i], INPUT_PULLUP);
  }

  for (int i=0; i<sizeof(rows)/sizeof(int); i++){
    pinMode(rows[i], INPUT_PULLUP);
  }
  //End Setup Keypad

  alarm = false;
}

void loop()
{
  if (alarm) {
    checkKeypad();
    if(!hasInput) tone(speaker, NOTE_A4);
  }

  else{


    //Phase 1: BOX Unlocked
    //Step 1: Look for keypad input

    


    //Step 2: Set Code
    if(!locked) {
      //If no code is set, box is unlocked and dont read sensors
      //Steady green LED
      analogWrite(greenLED, 255); 

      if(!codeSet && ((millis() - lastKeyCheck) > 50)) checkKeypad();    //Check keypad every 50 milliseconds
      
      //If user presses pound and code is set, lock box
      else if((!digitalRead(rows[3])) && (!digitalRead(columns[0])) && codeSet) {
        locked = true;
        Serial.println("Box locked"); 
      playSound(key, 100, sizeof(key) / sizeof(int));
    }
    }
    else {
          //Check keypad every 50 milliseconds
        if (millis() - lastKeyCheck > 50) checkKeypad();
    }



    //If code is set, box is locked, checked sensors
    if (locked){
     //Turn off LED 
      analogWrite(greenLED, 0); 

      
      //Phase 2: Box Locked
      //Step 1: Establish WiFly connection, make noise if connection failed

        //Step 2: Check LDR 
      if ((millis() % 100) == 0) checkLDR();

      //Step 4: Check Temp and humidity
      if ((millis() % 1000) == 0) checkTemp();

      //Step 5: Check gyroscope
      //Step 6: Check GPS
      readGPS();
    }
  }
}

boolean readGPS(){
  if (gpsSerial.available())
  {
    char ch = gpsSerial.read();
    if (ch != '\n' && i < sentenceSize)
    {
      sentence[i] = ch;
      i++;
    }
    else
    {
      sentence[i] = '\0';
      i = 0;
      displayGPS();
    }
    return true;
  }
  else return false;
}

void displayGPS()
{
  char field[20];
  getField(field, 0);
  if (strcmp(field, "$GPRMC") == 0)
  {
    Serial.print("Lat: ");
    getField(field, 3);  // number
    Serial.print(field);
    getField(field, 4); // N/S
    Serial.print(field);

    Serial.print(" Long: ");
    getField(field, 5);  // number
    Serial.print(field);
    
    char sign;
    getField(field, 6);  // E/W
    Serial.println(field);
  }
}

void getField(char* buffer, int index)
{
  int sentencePos = 0;
  int fieldPos = 0;
  int commaCount = 0;
  while (sentencePos < sentenceSize)
  {
    if (sentence[sentencePos] == ',')
    {
      commaCount ++;
      sentencePos ++;
    }
    if (commaCount == index)
    {
      buffer[fieldPos] = sentence[sentencePos];
      fieldPos ++;
    }
    sentencePos ++;
  }
  buffer[fieldPos] = '\0';
} 

void checkKeypad(){
  
  lastKeyCheck = millis();
  //if array is full, print the full input, separated by dashes.
  if (count == pinLength) {
    Serial.print("You have entered: ");
    for(int i=0; i<pinLength; i++) {
      Serial.print(digits[i]);
      if (!codeSet) code[i] = digits[i];
      //Add dashes inbetween digits
      if(i != (pinLength-1)) Serial.print("-");   
    }

    Serial.println();

    //If code was just set for first time or reset, set codeSet to true, print confirmation an turn off indicator leds
    if(!codeSet) {
      codeSet = true;
      locked = true;
      alarm = false;
      Serial.println("Code set. Box locked"); 
      analogWrite(greenLED, 0);
      //      analogWrite(redLED, 0);
    }

    //If a code has been set, check the code entered with the saved code
    else {
      //Check entered code with saved code
      boolean correct = true;
      for (int i=0; i<pinLength; i++) {
        //If any digit is wrong, set correct boolean to false
        if (code[i] != digits[i]) correct = false;
      }

      if (correct) {
        Serial.println("Code is correct. Box is unlocked.");
        alarm = false;
        blinkGreen(5, 200);
        locked = false;

        //Check if resetMode is true
        if(resetMode) codeSet = false;
      }
      else {
        Serial.println("Code is incorrect.");
        blinkGreen(5, 200);
      }
      //Set resetMode back to deafult value (false)
      resetMode = false;
    }

    //Reset counter
    count = 0;

    //Free memory
    delete[] digits;
  }
  //If array is not full, look for next input
  else {
    //Default hasInput to false (will set to true if input read)
    hasInput = false;

    //Start input counter at 1, increases during traversal to track which button is being read
    int input = 1;

    //Traverse the keypad by row so that numbers are checked for in order
    for (int i=0; i<sizeof(rows)/sizeof(int); i++){
      for (int j=0; j<sizeof(columns)/sizeof(int); j++){
        //If that row and column are both sending a single, a button is being pressed that corresponds with input counter
        if((!digitalRead(rows[i])) && (!digitalRead(columns[j]))) {

          //Wait for button to be released
          while ((!digitalRead(rows[i])) && (!digitalRead(columns[j])));

          hasInput = true;

          //Check for symbol row input

          ///If * is pressed during code input, input starts over
          if (input == 10) {
            //Set the counter back to zero, print messege and blink yellow
            count = 0;
            Serial.println("Reset input");
            //           blinkYellow(3, 300);
            //Break out of loop so * is not stored
            break;
          }

          else if (input == 11) input = 0;

          //If user enters # at any time, and the code they entered is correct, enter resetMode to change the saved code.
          else if (input == 12) {
            resetMode = true; //Symbol is '#'
            Serial.println("Rest mode enabled");
            break;  
          }

          //Output: Print input button to Serial monitor and light yellow LED
          Serial.print("Input: ");
          Serial.println(input);

          //Store input and increment counter
          digits[count] = input;
          count++;

          //Flash led led to confirm input
          blinkGreen(1, 100);
          break;
        }
        //If no input read at that button, move on and increment the input counter
        else {
          input++;
        }
      }
    }     
  }
}

void blinkGreen(int count, int delayTime) {
  for (int i=0; i<count; i++) {
    analogWrite(greenLED, 255);
    playSound(key, 100, sizeof(key) / sizeof(int));
    delay(delayTime/2);
    analogWrite(greenLED, 0);
    delay(delayTime/2);
  }
}

void checkTemp(){
  int chk = DHT11.read();

  Serial.print("Read sensor: ");
  switch (chk)
  {
  case 0: 
    Serial.println("OK"); 
    break;
  case -1: 
    Serial.println("Checksum error"); 
    break;
  case -2: 
    Serial.println("Time out error"); 
    break;
  default: 
    Serial.println("Unknown error"); 
    break;
  }

  Serial.print("Humidity (%): ");
  Serial.println((float)DHT11.humidity, DEC);

  Serial.print("Temperature (°C): ");
  Serial.println((float)DHT11.temperature, DEC);

  Serial.print("Temperature (°F): ");
  Serial.println(DHT11.fahrenheit(), DEC);

  Serial.print("Temperature (°K): ");
  Serial.println(DHT11.kelvin(), DEC);

  Serial.print("Dew Point (°C): ");
  Serial.println(DHT11.dewPoint(), DEC);

  Serial.print("Dew PointFast (°C): ");
  Serial.println(DHT11.dewPointFast(), DEC);

}

void checkLDR(){
  //Photoresistor
  int LDRReading = analogRead(LDR_Pin); 

//  Serial.println("\n");
//  Serial.print("Photoresistor: ");
//
//  Serial.println(LDRReading);

  if(locked && (LDRReading > photoLevel)) soundAlarm();
}

void sendData(){

}

void soundAlarm(){
  Serial.println("INTRUDER!");
  //Send warning tweet


  //Make noise
  playSound(warn, 100, (sizeof(warn) / sizeof(int)));
  alarm = true;
}


void playSound(int *sound, int milliDelay, int length) {
  for (int i=0; i<length; i++){
    tone(speaker, sound[i]);
    delay(milliDelay);
    noTone(speaker);
  }
}













Aaron’s new whatever
----------------------------------------------------------------------------------------


/*
DHT11 Library provided by Virtuabotix, Author Joseph Dattilo
 https://www.virtuabotix.com/product-dht11-temperaturehumidity-sensor-msrp-9-99/dht11_2s0a/
 */

#include <dht11.h>
#include <SoftwareSerial.h>
#include <Bridge.h>
#include <Temboo.h>
#include "TwitterCreds.h" // contains Twitter account information
#include "TembooAccount.h" // contains Temboo account information

#define LDR_Pin A4 //Light-dependent resistor
#define greenLED A0
#define speaker 13
#define GPSrx 9
#define GPStx 0
#define tempSens 4
#define pinLength 4 //This can be changed here to allow for a diffent code length requirement

boolean battery = false; //Set to true when setting software

int warn[] = {
  1760, 988, 1760, 1760, 988, 1760, 988, 
  1760, 988, 1760, 988, 1760, 988, 1760, 988, 
  1760,988, 1760, 988, 1760, 988, 1760, 988};

int key[] = {1760};

//Sensor values
double temp;
double hums;
float longitude;
float latitude;
double angle;
boolean codeSet = false;
boolean alarm;
boolean locked = false;
//

float lastKeyCheck = 0;

double photoLevel = 300;

//Keypad pins
//Columns[left, mid, right]
int columns[] = {
  11,12,10};
//Rows[top, mid, bottom, symbol]
int rows[] = {
  5, 6, 8, 7};

//GPS variables
SoftwareSerial gpsSerial(GPSrx, GPStx); // RX, TX (TX not used)
const int sentenceSize = 80;
char sentence[sentenceSize];
int i = 0;


//Temp & humid varibles
dht11 DHT11;

//Keypad variables
int input;
boolean hasInput;
boolean resetMode = false;
int count = 0; //Counts input digits
int digits[pinLength];  //Points to array of input digits
int code[pinLength]; //Points to array of stored code
///End Keypad

void setup()
{
  if(!battery) {
    Serial.begin(9600);
  }
  Bridge.begin();
  //GPS Setup
  gpsSerial.begin(9600);


  //Temp Sensor Setup
  DHT11.attach(tempSens);


  //Setup keypad

  // initialize the digital pins as input_pullup.
  for (int i=0; i<sizeof(columns)/sizeof(int); i++){
    pinMode(columns[i], INPUT_PULLUP);
  }

  for (int i=0; i<sizeof(rows)/sizeof(int); i++){
    pinMode(rows[i], INPUT_PULLUP);
  }
  //End Setup Keypad

  alarm = false;
}

void loop()
{
  if (alarm) {
    checkKeypad();
    if(!hasInput) tone(speaker, 440);
  }

  else{


    //Phase 1: BOX Unlocked
    //Step 1: Look for keypad input

    


    //Step 2: Set Code
    if(!locked) {
      //If no code is set, box is unlocked and dont read sensors
      //Steady green LED
      analogWrite(greenLED, 255); 

      if(!codeSet && ((millis() - lastKeyCheck) > 50)) checkKeypad();    //Check keypad every 50 milliseconds
      
      //If user presses pound and code is set, lock box
      else if((!digitalRead(rows[3])) && (!digitalRead(columns[0])) && codeSet) {
        tweet("Your box has been locked");
        locked = true;
        Serial.println("Box locked"); 
      playSound(key, 100, sizeof(key) / sizeof(int));
    }
    }
    else {
          //Check keypad every 50 milliseconds
        if (millis() - lastKeyCheck > 50) checkKeypad();
    }



    //If code is set, box is locked, checked sensors
    if (locked){
     //Turn off LED 
      analogWrite(greenLED, 0); 

      
      //Phase 2: Box Locked
      //Step 1: Establish WiFly connection, make noise if connection failed

        //Step 2: Check LDR 
      if ((millis() % 100) == 0) checkLDR();

      //Step 4: Check Temp and humidity
      if ((millis() % 1000) == 0) checkTemp();

      //Step 5: Check gyroscope
      //Step 6: Check GPS
      readGPS();
    }
  }
}

boolean readGPS(){
  if (gpsSerial.available())
  {
    char ch = gpsSerial.read();
    if (ch != '\n' && i < sentenceSize)
    {
      sentence[i] = ch;
      i++;
    }
    else
    {
      sentence[i] = '\0';
      i = 0;
      displayGPS();
    }
    return true;
  }
  else return false;
}

void displayGPS()
{
  char field[20];
  getField(field, 0);
  if (strcmp(field, "$GPRMC") == 0)
  {
    Serial.print("Lat: ");
    getField(field, 3);  // number
    Serial.print(field);
    getField(field, 4); // N/S
    Serial.print(field);

    Serial.print(" Long: ");
    getField(field, 5);  // number
    Serial.print(field);
    
    char sign;
    getField(field, 6);  // E/W
    Serial.println(field);
  }
}

void getField(char* buffer, int index)
{
  int sentencePos = 0;
  int fieldPos = 0;
  int commaCount = 0;
  while (sentencePos < sentenceSize)
  {
    if (sentence[sentencePos] == ',')
    {
      commaCount ++;
      sentencePos ++;
    }
    if (commaCount == index)
    {
      buffer[fieldPos] = sentence[sentencePos];
      fieldPos ++;
    }
    sentencePos ++;
  }
  buffer[fieldPos] = '\0';
} 

void checkKeypad(){
  
  lastKeyCheck = millis();
  //if array is full, print the full input, separated by dashes.
  if (count == pinLength) {
    Serial.print("You have entered: ");
    for(int i=0; i<pinLength; i++) {
      Serial.print(digits[i]);
      if (!codeSet) code[i] = digits[i];
      //Add dashes inbetween digits
      if(i != (pinLength-1)) Serial.print("-");   
    }

    Serial.println();

    //If code was just set for first time or reset, set codeSet to true, print confirmation an turn off indicator leds
    if(!codeSet) {
      codeSet = true;
      locked = true;
      alarm = false;
      Serial.println("Code set. Box locked"); 
      analogWrite(greenLED, 0);
      //      analogWrite(redLED, 0);
    }

    //If a code has been set, check the code entered with the saved code
    else {
      //Check entered code with saved code
      boolean correct = true;
      for (int i=0; i<pinLength; i++) {
        //If any digit is wrong, set correct boolean to false
        if (code[i] != digits[i]) correct = false;
      }

      if (correct) {
        Serial.println("Code is correct. Box is unlocked.");
        alarm = false;
        locked = false;
        String str = "Your box has been unlocked";
        tweet(str);
        blinkGreen(5, 200);


        //Check if resetMode is true
        if(resetMode) codeSet = false;
      }
      else {
        Serial.println("Code is incorrect.");
        blinkGreen(5, 200);
      }
      //Set resetMode back to deafult value (false)
      resetMode = false;
    }

    //Reset counter
    count = 0;

    //Free memory
    delete[] digits;
  }
  //If array is not full, look for next input
  else {
    //Default hasInput to false (will set to true if input read)
    hasInput = false;

    //Start input counter at 1, increases during traversal to track which button is being read
    int input = 1;

    //Traverse the keypad by row so that numbers are checked for in order
    for (int i=0; i<sizeof(rows)/sizeof(int); i++){
      for (int j=0; j<sizeof(columns)/sizeof(int); j++){
        //If that row and column are both sending a single, a button is being pressed that corresponds with input counter
        if((!digitalRead(rows[i])) && (!digitalRead(columns[j]))) {

          //Wait for button to be released
          while ((!digitalRead(rows[i])) && (!digitalRead(columns[j])));

          hasInput = true;

          //Check for symbol row input

          ///If * is pressed during code input, input starts over
          if (input == 10) {
            //Set the counter back to zero, print messege and blink yellow
            count = 0;
            Serial.println("Reset input");
            //           blinkYellow(3, 300);
            //Break out of loop so * is not stored
            break;
          }

          else if (input == 11) input = 0;

          //If user enters # at any time, and the code they entered is correct, enter resetMode to change the saved code.
          else if (input == 12) {
            resetMode = true; //Symbol is '#'
            Serial.println("Rest mode enabled");
            break;  
          }

          //Output: Print input button to Serial monitor and light yellow LED
          Serial.print("Input: ");
          Serial.println(input);

          //Store input and increment counter
          digits[count] = input;
          count++;

          //Flash led led to confirm input
          blinkGreen(1, 100);
          break;
        }
        //If no input read at that button, move on and increment the input counter
        else {
          input++;
        }
      }
    }     
  }
}

void blinkGreen(int count, int delayTime) {
  for (int i=0; i<count; i++) {
    analogWrite(greenLED, 255);
    playSound(key, 100, sizeof(key) / sizeof(int));
    delay(delayTime/2);
    analogWrite(greenLED, 0);
    delay(delayTime/2);
  }
}

void checkTemp(){
  int chk = DHT11.read();

  Serial.print("Read sensor: ");
  switch (chk)
  {
  case 0: 
    Serial.println("OK"); 
    break;
  case -1: 
    Serial.println("Checksum error"); 
    break;
  case -2: 
    Serial.println("Time out error"); 
    break;
  default: 
    Serial.println("Unknown error"); 
    break;
  }
  
  hums = DHT11.humidity;
  temp = DHT11.fahrenheit();

  Serial.print("Humidity (%): ");
  Serial.println((float)DHT11.humidity, DEC);

  Serial.print("Temperature (°C): ");
  Serial.println((float)DHT11.temperature, DEC);

  Serial.print("Temperature (°F): ");
  Serial.println(DHT11.fahrenheit(), DEC);

  Serial.print("Temperature (°K): ");
  Serial.println(DHT11.kelvin(), DEC);

  Serial.print("Dew Point (°C): ");
  Serial.println(DHT11.dewPoint(), DEC);

  Serial.print("Dew PointFast (°C): ");
  Serial.println(DHT11.dewPointFast(), DEC);

}

void checkLDR(){
  //Photoresistor
  int LDRReading = analogRead(LDR_Pin); 

//  Serial.println("\n");
//  Serial.print("Photoresistor: ");
//
//  Serial.println(LDRReading);

  if(locked && (LDRReading > photoLevel)) soundAlarm();
}

void sendData(){

}

void soundAlarm(){
  Serial.println("INTRUDER!");
  //Send warning tweet


  //Make noise
  playSound(warn, 100, (sizeof(warn) / sizeof(int)));
  alarm = true;
}


void playSound(int *sound, int milliDelay, int length) {
  for (int i=0; i<length; i++){
    tone(speaker, sound[i]);
    delay(milliDelay);
    noTone(speaker);
  }
}


void tweet(String str){
      Serial.println("Running SendATweet - Run #");
  
    // define the text of the tweet we want to send
    String tweetText = str;

    
    TembooChoreo StatusesUpdateChoreo;

    // invoke the Temboo client
    // NOTE that the client must be reinvoked, and repopulated with
    // appropriate arguments, each time its run() method is called.
    StatusesUpdateChoreo.begin();
    
    // set Temboo account credentials
    StatusesUpdateChoreo.setAccountName(TEMBOO_ACCOUNT);
    StatusesUpdateChoreo.setAppKeyName(TEMBOO_APP_KEY_NAME);
    StatusesUpdateChoreo.setAppKey(TEMBOO_APP_KEY);

    // identify the Temboo Library choreo to run (Twitter > Tweets > StatusesUpdate)
    StatusesUpdateChoreo.setChoreo("/Library/Twitter/Tweets/StatusesUpdate");

    // set the required choreo inputs
    // see https://www.temboo.com/library/Library/Twitter/Tweets/StatusesUpdate/ 
    // for complete details about the inputs for this Choreo
 
    // add the Twitter account information
    StatusesUpdateChoreo.addInput("AccessToken", TWITTER_ACCESS_TOKEN);
    StatusesUpdateChoreo.addInput("AccessTokenSecret", TWITTER_ACCESS_TOKEN_SECRET);
    StatusesUpdateChoreo.addInput("ConsumerKey", TWITTER_API_KEY);    
    StatusesUpdateChoreo.addInput("ConsumerSecret", TWITTER_API_SECRET);

    // and the tweet we want to send
    StatusesUpdateChoreo.addInput("StatusUpdate", tweetText);

    // tell the Process to run and wait for the results. The 
    // return code (returnCode) will tell us whether the Temboo client 
    // was able to send our request to the Temboo servers
    unsigned int returnCode = StatusesUpdateChoreo.run();

    // a return code of zero (0) means everything worked
    if (returnCode == 0) {
        Serial.println("Success! Tweet sent!");
    } else {
      // a non-zero return code means there was an error
      // read and print the error message
      while (StatusesUpdateChoreo.available()) {
        char c = StatusesUpdateChoreo.read();
        Serial.print(c);
      }
    } 
    StatusesUpdateChoreo.close();

    // do nothing for the next 90 seconds
    Serial.println("Waiting...");
    delay(2000);
}














Known to tweet
----------------------------------------------------------------------------------

#include <Bridge.h>
#include <Temboo.h>
#include "TembooAccount.h" // contains Temboo account information, as described below
#include "TwitterCreds.h"

void setup() {
  Serial.begin(9600);
  Bridge.begin();
}

void loop()
{
String poo = "habidoobab";
tweet(poo);

}

void tweet(String str){
      Serial.println("Running SendATweet - Run #");
  
    // define the text of the tweet we want to send
    String tweetText = str;

    
    TembooChoreo StatusesUpdateChoreo;

    // invoke the Temboo client
    // NOTE that the client must be reinvoked, and repopulated with
    // appropriate arguments, each time its run() method is called.
    StatusesUpdateChoreo.begin();
    
    // set Temboo account credentials
    StatusesUpdateChoreo.setAccountName(TEMBOO_ACCOUNT);
    StatusesUpdateChoreo.setAppKeyName(TEMBOO_APP_KEY_NAME);
    StatusesUpdateChoreo.setAppKey(TEMBOO_APP_KEY);

    // identify the Temboo Library choreo to run (Twitter > Tweets > StatusesUpdate)
    StatusesUpdateChoreo.setChoreo("/Library/Twitter/Tweets/StatusesUpdate");

    // set the required choreo inputs
    // see https://www.temboo.com/library/Library/Twitter/Tweets/StatusesUpdate/ 
    // for complete details about the inputs for this Choreo
 
    // add the Twitter account information
    StatusesUpdateChoreo.addInput("AccessToken", TWITTER_ACCESS_TOKEN);
    StatusesUpdateChoreo.addInput("AccessTokenSecret", TWITTER_ACCESS_TOKEN_SECRET);
    StatusesUpdateChoreo.addInput("ConsumerKey", TWITTER_API_KEY);    
    StatusesUpdateChoreo.addInput("ConsumerSecret", TWITTER_API_SECRET);

    // and the tweet we want to send
    StatusesUpdateChoreo.addInput("StatusUpdate", tweetText);

    // tell the Process to run and wait for the results. The 
    // return code (returnCode) will tell us whether the Temboo client 
    // was able to send our request to the Temboo servers
    unsigned int returnCode = StatusesUpdateChoreo.run();

    // a return code of zero (0) means everything worked
    if (returnCode == 0) {
        Serial.println("Success! Tweet sent!");
    } else {
      // a non-zero return code means there was an error
      // read and print the error message
      while (StatusesUpdateChoreo.available()) {
        char c = StatusesUpdateChoreo.read();
        Serial.print(c);
      }
    } 
    StatusesUpdateChoreo.close();

    // do nothing for the next 90 seconds
    Serial.println("Waiting...");
    delay(90000);
}


TwitterCreds.h
--------------------------------------------------------
const String  TWITTER_ACCESS_TOKEN = "2443401084-3ZLcrIrabSCG1Qeitnxxn7tuiNBkRfiS4EcCsCJ";
const String  TWITTER_ACCESS_TOKEN_SECRET = "RvEamXSqqzwdSUnngZhKV8XJCis3udRjaKE2if15k0KrD";
const String  TWITTER_API_KEY = "YiCUHyEFu5YGHHW6YFqWcBKLc";
const String  TWITTER_API_SECRET = "M5ukAY6hnBX0Mf4Zwl1KbPJbKAVeEeK6bbMbBAsgfXEh5YIInp";



TembooAccount.h
----------------------------------------------------------------------------------
#define TEMBOO_ACCOUNT "scarrick"  // Your Temboo account name 
#define TEMBOO_APP_KEY_NAME "ardtweeto"  // Your Temboo app key name
#define TEMBOO_APP_KEY "d4daeaf706eb41b6a98329ccba475b1c"  // Your Temboo app key


