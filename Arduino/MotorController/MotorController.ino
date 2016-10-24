#define DEBUG 1
#define MAGICADDRESS 7
// randomly(or is it!)

// SET VELOCITY:  D,speed_motor_left,speed_motor_right\n
// SET PIDs:      H,P,I,D,1/2\n
// READ ENCODER:  R\n
// SET PWM:       L,speed_motor_left,speed_motor_right\n
// RESET ENCODER: I\n
// READ PID Value:S,1/2\n
// SET DebugRate: Z,rate\n   -- bigger/slower

#include <math.h>
#include <EEPROM.h>
#include <Encoder.h>
#include <PID_v1.h>

Encoder encL(2,4); //inversed pins, to get correct direction
Encoder encR(3,5);

#include "MusafirMotor.h"
MusafirMotor motorL(7, 6, 9);
MusafirMotor motorR(13, 12, 10);
struct motorParams {
  double kp;
  double ki;
  double kd;
};
motorParams motorPIDL;
motorParams motorPIDR;

const float wheel_diameter = 9.15; // in mm
const float circumference = M_PI * wheel_diameter;
const float tickDistance = (float)circumference/1500.0;

unsigned long previousMillis = 0;
const long interval = 10; // in ms

String inputString = "";
boolean stringComplete = false;

double vel1 = 0, vel2 = 0;
double spd1, spd2;
double pwm1, pwm2;
// PID (&input, &output, &setpoint, kp, ki, kd, DIRECT/REVERSE)
PID pidL(&spd1, &pwm1, &vel1, 1,0,0, DIRECT);
PID pidR(&spd2, &pwm2, &vel2, 1,0,0, DIRECT);
boolean pidActive= false;

unsigned int debugCount=0,debugRate=100;

void setup() {
  Serial.begin(115200);
  inputString.reserve(100);
  
  encL.write(0);
  encR.write(0);
  motorL.setDir(FORWARD);
  motorR.setDir(FORWARD);

  initEEPROM();

  pidL.SetMode(MANUAL); // PID CONTROL OFF
  pidR.SetMode(MANUAL);
  pidL.SetTunings(motorPIDL.kp, motorPIDL.ki, motorPIDL.kd);
  pidR.SetTunings(motorPIDR.kp, motorPIDR.ki, motorPIDR.kd);
  pidL.SetSampleTime(interval); // sample time for PID
  pidR.SetSampleTime(interval);
  pidL.SetOutputLimits(0,255);  // min/max PWM
  pidR.SetOutputLimits(0,255);
}

void(* resetFunc) (void) = 0;

void loop() {
  if (stringComplete) {
    interpretSerialData();
    stringComplete = false;
    inputString = "";
  }
  
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    long encCurr1 = encL.read();
    long encCurr2 = -encR.read(); //encoder is reversed
    resetEncoders();

    float distance1 = (float)encCurr1*tickDistance;
    spd1 = (float)distance1*(1000.0/interval);
    float distance2 = (float)encCurr2*tickDistance;
    spd2 = (float)distance2*(1000.0/interval);

    debugCount++;
    if(DEBUG && (debugCount%debugRate==0)){
      debugDump();
    }
  }
  pidL.Compute();
  pidR.Compute();
  if(pidActive){
    if(vel1>0) motorL.setPWM(pwm1);
    else motorL.setPWM(0);
    if(vel2>0) motorR.setPWM(pwm2);
    else motorR.setPWM(0);
  }
}

void debugDump(void){
  // PID Vals for bot motors
  //Sensor(Speed), SetPoint, Error, Output
  Serial.print(spd1);
  Serial.print(", ");
  Serial.print(vel1);
  Serial.print(", ");
  Serial.print(vel1-spd1);
  Serial.print(", ");
  Serial.print(pwm1);
  Serial.print(" | ");
  Serial.print(spd2);
  Serial.print(", ");
  Serial.print(vel2);
  Serial.print(", ");
  Serial.print(vel2-spd2);
  Serial.print(", ");
  Serial.print(pwm2);
  Serial.println();
}

void interpretSerialData(void){
    int c1=1, c2=1;
    int val1=0, val2=0;
    switch(inputString[0]){
      case 'D':
        // COMMAND:  D,speed_motor_left,speed_motor_right\n
        c1 = inputString.indexOf(',')+1;
        c2 = inputString.indexOf(',',c1);
        val1 = inputString.substring(c1,c2).toInt();
        c1 = c2+1;
        val2 = inputString.substring(c1).toInt();
        if(val1<0) { motorL.setDir(BACKWARD); val1 = -val1; }
        else         motorL.setDir(FORWARD);
        if(val2<0) { motorR.setDir(BACKWARD); val2 = -val2; }
        else         motorR.setDir(FORWARD);
        vel1 = val1;
        vel2 = val2;
        if(DEBUG){
          Serial.print("Velocity 1 ");
          Serial.println(vel1);
          Serial.print("Velocity 2 ");
          Serial.println(vel2);
        }         
        Serial.println('d');
        if(vel1>0)
          pidL.SetMode(AUTOMATIC);
        else
          pidL.SetMode(MANUAL);
        if(vel2>0)
          pidR.SetMode(AUTOMATIC);
        else
          pidR.SetMode(MANUAL);
        pidActive= true;
        break;
      case 'H':
        // COMMAND:  H,P,I,D,1/2\n
        float p,i,d;
        c1 = inputString.indexOf(',')+1;
        c2 = inputString.indexOf(',',c1);
        p = inputString.substring(c1,c2).toFloat();
        c1 = c2+1;
        c2 = inputString.indexOf(',',c1);
        i = inputString.substring(c1,c2).toFloat();
        c1 = c2+1;
        c2 = inputString.indexOf(',',c1);
        d = inputString.substring(c1,c2).toFloat();
        c1 = c2+1;
        val1 = inputString.substring(c1).toInt();
        if(val1==1) {
          motorPIDL.kp = p;
          motorPIDL.ki = i;
          motorPIDL.kd = d;
          pidL.SetTunings(motorPIDL.kp, motorPIDL.ki, motorPIDL.kd);
          EEPROM.put((const int)MAGICADDRESS, motorPIDL);
          if(DEBUG) Serial.println("motorPIDL ");
        }
        else if(val1==2){
          motorPIDR.kp = p;
          motorPIDR.ki = i;
          motorPIDR.kd = d;
          pidR.SetTunings(motorPIDR.kp, motorPIDR.ki, motorPIDR.kd);
          EEPROM.put((const int)(MAGICADDRESS+sizeof(motorParams)), motorPIDR);
          if(DEBUG) Serial.println("motorPIDR ");
        }
        Serial.print("h,");
        Serial.print(val1);
        Serial.print(',');
        Serial.println(p);
        Serial.print(',');
        Serial.println(i);
        Serial.print(',');
        Serial.println(d);
        break;
      case 'L':
        // COMMAND:  L,speed_motor_left,speed_motor_right\n
        c1 = inputString.indexOf(',')+1;
        c2 = inputString.indexOf(',',c1);
        val1 = inputString.substring(c1,c2).toInt();
        c1 = inputString.indexOf(',',c2)+1;
        val2 = inputString.substring(c1).toInt();
        if(val1<0) { motorL.setDir(BACKWARD); val1 = -val1; }
        else         motorL.setDir(FORWARD);
        if(val2<0) { motorR.setDir(BACKWARD); val2 = -val2; }
        else         motorR.setDir(FORWARD);
        pidActive= false;
        pidL.SetMode(MANUAL);
        pidR.SetMode(MANUAL);
        pwm1 = val1;
        pwm2 = val2;
        motorL.setPWM(pwm1);
        motorR.setPWM(pwm2);
        if(DEBUG){
          Serial.print("PWM1: "); Serial.println(val1);        
          Serial.print("PWM2: "); Serial.println(val2);
        }
        Serial.println('l');
        break;
      case 'S':
        // COMMAND:  S,1/2\n
        c1 = inputString.indexOf(',')+1;
        c2 = inputString.indexOf(',',c1);
        val1 = inputString.substring(c1).toInt();
        if(val1==1) {
          Serial.print("s,");
          Serial.print(motorPIDL.kp);
          Serial.print(',');
          Serial.print(motorPIDL.ki);
          Serial.print(',');
          Serial.println(motorPIDL.kd);
        }else if(val1==2) {
          Serial.print("s,");
          Serial.print(motorPIDR.kp);
          Serial.print(',');
          Serial.print(motorPIDR.ki);
          Serial.print(',');
          Serial.println(motorPIDR.kd);
        }
        break;
      case 'Z':
        // COMMAND:  Z,rate\n
        c1 = inputString.indexOf(',')+1;
        c2 = inputString.indexOf(',',c1);
        val1 = inputString.substring(c1).toInt();
        if(val1>1) {
          debugRate=val1;
        }
        break;
      case 'R':
        // COMMAND:  R\n
        Serial.print("r,");
        Serial.print(encL.read());
        Serial.print(',');
        Serial.print(encR.read());
        Serial.println();
        break;
      case 'I':
        // COMMAND: I\n
        resetEncoders();
        Serial.println('i');
        break;
      default:
        Serial.print("UNKNOWN COMMAND: ");
        Serial.println(inputString);
        break;
    }
}

void initEEPROM(void){
  int checkEEPROM=0;
  EEPROM.get(0, checkEEPROM);
  if(checkEEPROM==MAGICADDRESS){
    if(DEBUG) Serial.println("Reading from EEPROM.");
    EEPROM.get((const int)MAGICADDRESS, motorPIDL);   //(address, data)
    EEPROM.get((const int)(MAGICADDRESS+sizeof(motorParams)), motorPIDR);
  }
  else{
    // Set default values
    if(DEBUG) Serial.println("Setting Default Values.");
    EEPROM.put(0, MAGICADDRESS);
    motorPIDL.kp = 1.0;
    motorPIDL.ki = 0.0;
    motorPIDL.kd = 0.0;
    EEPROM.put((const int)MAGICADDRESS, motorPIDL);

    motorPIDR.kp = 1.0;
    motorPIDR.ki = 0.0;
    motorPIDR.kd = 0.0;
    EEPROM.put((const int)(MAGICADDRESS+sizeof(motorParams)), motorPIDR);
  }  
}

void resetEncoders(void){
  encL.write(0);
  encR.write(0);
}

void serialEvent() 
{
  while(Serial.available()){
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}
