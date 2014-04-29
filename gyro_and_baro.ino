/*
   Gyroscope Initialization code from www.hobbytronics.co.uk/arduino-adxl345-imu3000
   Barometer initialization code from www.sparkfun.com/tutorials/253
*/

//Libraries used: 
#include <Wire.h>

// Header section
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

//setup (includes setup functions)
void setup(){
    Serial.begin(9600); 
    Wire.begin();
    do_gyro_startup();
    do_baro_startup();
}

//loop (includes loop functions)
void loop(){
   gyro_loop(); 
   baro_loop();
   delay(1000);
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
