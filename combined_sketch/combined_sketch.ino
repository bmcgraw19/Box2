/*
DHT11 Library provided by Virtuabotix, Author Joseph Dattilo
 https://www.virtuabotix.com/product-dht11-temperaturehumidity-sensor-msrp-9-99/dht11_2s0a/
 */

#include <dht11.h>
#include "Notes.h"
#include <SoftwareSerial.h>
#include <Wire.h>



#define LDR_Pin A2 //Light-dependent resistor
#define greenLED A0
#define speaker 13
#define GPSrx 9
#define GPStx 0
#define tempSens 2
#define pinLength 4 //This can be changed here to allow for a diffent code length requirement
#define GYRO 0x68         // gyro I2C address
#define BMP085_ADDRESS 0x77  // I2C address of BMP085
#define REG_GYRO_X 0x1D   // IMU-3000 Register address for GYRO_XOUT_H
#define REG_GYRO_Y 0x1F
#define REG_GRYO_Z 0x21
#define REG_ACCEL_X 0x23
#define REG_ACCEL_Y 0x25
#define REG_ACCEL_Z 0x27
#define CTRL 0x3D


// Globals section
float temp;
float hums;
float longitude;
float latitude;
float angle;
boolean codeSet = false;
boolean alarm;
boolean locked = false;
int gyro_x;
int gyro_y;
int gyro_z;
int accel_x;
int accel_y;
int accel_z;
const unsigned char OSS = 2;  // Oversampling Setting
const float p0 = 101325;     // Pressure at sea level (Pa)
float altitude;
// Calibration values
int ac1;
int ac2; 
int ac3; 
unsigned int ac4;
unsigned int ac5;
unsigned int ac6;
int b1; 
int b2;
int mb;
int mc;
int md;
// b5 is calculated in bmp085GetTemperature(...), this variable is also used in bmp085GetPressure(...)
// so ...Temperature(...) must be called before ...Pressure(...).
long b5; 
short temperature;
long pressure;
float lastKeyCheck = 0;
double photoLevel = 300;
//Keypad pins
//Columns[left, mid, right]
int columns[] = {11,12,10};
//Rows[top, mid, bottom, symbol]
int rows[] = {5, 6, 8, 7};
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
  Serial.begin(9600);
  while (!Serial) 
    ; // wait for serial port to connect. Needed for Leonardo only
  

  //GPS Setup
  gpsSerial.begin(9600);
  Wire.begin();

  //Temp Sensor Setip
  DHT11.attach(tempSens);


  //Setup keypad
  do_keypad_startup();
  do_gyro_startup();
  do_baro_startup();
  
}

void loop()
{
  loop_keypad();
  gyro_loop(); 
  baro_loop();
}

void loop_keypad(){
 if(alarm) {
    checkKeypad();
    if(!hasInput) tone(speaker, NOTE_A4);
  }else{


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
void do_keypad_startup(){
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
// gyro startup
void do_gyro_startup(){
 writeTo(GYRO, 0x16, 0x0B);
}

// barometer startup
void do_baro_startup(){
  bmp085Calibration(); 
}

//gyroscope loop
void gyro_loop(){
    gyro_x = imu3kReadInt(REG_GYRO_X);
    gyro_y = imu3kReadInt(REG_GYRO_Y);
    gyro_z = imu3kReadInt(REG_GRYO_Z);
    

    // Print out what we have
    Serial.print("X: ");
    Serial.print(gyro_x);  // echo the number received to screen
    Serial.print(", Y: ");
    Serial.print(gyro_y);  // echo the number received to screen
    Serial.print(", Z: ");    
    Serial.print(gyro_z);  // echo the number received to screen 
    Serial.println("");     // prints carriage return
  
}


//barometer loop
void baro_loop(){
  
   // Read the Gyro X, Y and Z and Accel X, Y and Z all through the gyro
    
    temperature = bmp085GetTemperature(bmp085ReadUT());
    pressure = bmp085GetPressure(bmp085ReadUP());
    
    altitude = (float)44330 * (1 - pow(((float) pressure/p0), 0.190295));
    Serial.print("Approximate altitude: ");
    Serial.print(altitude, 2);
    Serial.println(" m");

    Serial.print("Temperature: ");
    double tem = temperature;
    tem = tem/10;
    Serial.print(tem, 2);
    Serial.println(" deg C");
    Serial.print("Pressure: ");
    Serial.print(pressure, DEC);
    Serial.println(" Pa");
    Serial.println();
  
}

/*   Auxiliary functions                                       */


// Write a value to address register on device
void writeTo(int device, byte address, byte val) {
  Wire.beginTransmission(device); // start transmission to device 
  Wire.write(address);             // send register address
  Wire.write(val);                 // send value to write
  Wire.endTransmission();         // end transmission
}

int imu3kReadInt(unsigned char address){
 unsigned char msb, lsb;
 Wire.beginTransmission(GYRO); // Tell the device what register to read from
 Wire.write(address);
 Wire.endTransmission();
 
 Wire.requestFrom(GYRO, 2);
 while(Wire.available()<2){
   Serial.print("Wire wanting to give ");
   Serial.print(Wire.available());
   Serial.print(" bytes\n");
   Serial.print("Waiting...\n");
   delay(500);  
 }   
 msb = Wire.read();
 lsb = Wire.read();
 return (int) msb<<8 | lsb; 
}

void bmp085Calibration()
{
  ac1 = bmp085ReadInt(0xAA);
  ac2 = bmp085ReadInt(0xAC);
  ac3 = bmp085ReadInt(0xAE);
  ac4 = bmp085ReadInt(0xB0);
  ac5 = bmp085ReadInt(0xB2);
  ac6 = bmp085ReadInt(0xB4);
  b1 = bmp085ReadInt(0xB6);
  b2 = bmp085ReadInt(0xB8);
  mb = bmp085ReadInt(0xBA);
  mc = bmp085ReadInt(0xBC);
  md = bmp085ReadInt(0xBE);
}

// Read 1 byte from the BMP085 at 'address'
char bmp085Read(unsigned char address)
{
  unsigned char data;
  
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(address);
  Wire.endTransmission();
  
  Wire.requestFrom(BMP085_ADDRESS, 1);
  while(!Wire.available())
    ;
    
  return Wire.read();
}

// Read 2 bytes from the BMP085
// First byte will be from 'address'
// Second byte will be from 'address'+1
int bmp085ReadInt(unsigned char address)
{
  unsigned char msb, lsb;
  
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(address);
  Wire.endTransmission();
  
  Wire.requestFrom(BMP085_ADDRESS, 2);
  while(Wire.available()<2)
    ;
  msb = Wire.read();
  lsb = Wire.read();
  
  return (int) msb<<8 | lsb;
}

// Read the uncompensated temperature value
unsigned int bmp085ReadUT()
{
  unsigned int ut;
  
  // Write 0x2E into Register 0xF4
  // This requests a temperature reading
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF4);
  Wire.write(0x2E);
  Wire.endTransmission();
  
  // Wait at least 4.5ms
  delay(5);
  
  // Read two bytes from registers 0xF6 and 0xF7
  ut = bmp085ReadInt(0xF6);
  return ut;
}

// Read the uncompensated pressure value
unsigned long bmp085ReadUP()
{
  unsigned char msb, lsb, xlsb;
  unsigned long up = 0;
  
  // Write 0x34+(OSS<<6) into register 0xF4
  // Request a pressure reading w/ oversampling setting
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF4);
  Wire.write(0x34 + (OSS<<6));
  Wire.endTransmission();
  
  // Wait for conversion, delay time dependent on OSS
  delay(2 + (3<<OSS));
  
  // Read register 0xF6 (MSB), 0xF7 (LSB), and 0xF8 (XLSB)
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF6);
  Wire.endTransmission();
  Wire.requestFrom(BMP085_ADDRESS, 3);
  
  // Wait for data to become available
  while(Wire.available() < 3)
    ;
  msb = Wire.read();
  lsb = Wire.read();
  xlsb = Wire.read();
  
  up = (((unsigned long) msb << 16) | ((unsigned long) lsb << 8) | (unsigned long) xlsb) >> (8-OSS);
  
  return up;
}

// Calculate temperature given ut.
// Value returned will be in units of 0.1 deg C
short bmp085GetTemperature(unsigned int ut)
{
  long x1, x2;
  
  x1 = (((long)ut - (long)ac6)*(long)ac5) >> 15;
  x2 = ((long)mc << 11)/(x1 + md);
  b5 = x1 + x2;

  return ((b5 + 8)>>4);  
}

// Calculate pressure given up
// calibration values must be known
// b5 is also required so bmp085GetTemperature(...) must be called first.
// Value returned will be pressure in units of Pa.
long bmp085GetPressure(unsigned long up)
{
  long x1, x2, x3, b3, b6, p;
  unsigned long b4, b7;
  
  b6 = b5 - 4000;
  // Calculate B3
  x1 = (b2 * (b6 * b6)>>12)>>11;
  x2 = (ac2 * b6)>>11;
  x3 = x1 + x2;
  b3 = (((((long)ac1)*4 + x3)<<OSS) + 2)>>2;
  
  // Calculate B4
  x1 = (ac3 * b6)>>13;
  x2 = (b1 * ((b6 * b6)>>12))>>16;
  x3 = ((x1 + x2) + 2)>>2;
  b4 = (ac4 * (unsigned long)(x3 + 32768))>>15;
  
  b7 = ((unsigned long)(up - b3) * (50000>>OSS));
  if (b7 < 0x80000000)
    p = (b7<<1)/b4;
  else
    p = (b7/b4)<<1;
    
  x1 = (p>>8) * (p>>8);
  x1 = (x1 * 3038)>>16;
  x2 = (-7357 * p)>>16;
  p += (x1 + x2 + 3791)>>4;
  
  return p;
}
