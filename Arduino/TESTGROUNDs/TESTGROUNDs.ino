#include <math.h>

#include <Encoder.h>
Encoder encL(2,4);
Encoder encR(3,5);
long encCurrL, encCurrR;

#include "MusafirMotor.h"
MusafirMotor motorL(7, 6, 9);
MusafirMotor motorR(13, 12, 10);

#include "Navigator.h"
Navigator  navigator;
//from https://github.com/solderspot/NavBot/blob/master/NavBot_v1/BlankBot.h
// Navigator defines
#define WHEELBASE               nvMM(190)      // millimeters
#define WHEEL_DIAMETER          nvMM(89)       // millimeters
#define TICKS_PER_REV           1500
#define WHEEL_DIAMETER_CM       8.9           // centi-meters
#define CM_DISTANCE_PER_TICK       (M_PI*WHEEL_DIAMETER_CM)/1500.0     // in CM

// correct for systematic errors
#define WHEEL_RL_SCALER         1.0f  // Ed
#define WHEELBASE_SCALER        1.0f  // Eb
// correct distance 
#define DISTANCE_SCALER         1.0f  // Es

#include <PID_v1.h>
double measuredVelL=0, measuredVelR=0;
double pwmL=0, pwmR=0;
double velL=0, velR=0;
double currentHeading=0, goalHeading=0, angularVelocity=0;
// PID (&input, &output, &setpoint, kp, ki, kd, DIRECT/REVERSE)
PID pidL(&measuredVelL, &pwmL, &velL, 2,1,0, DIRECT);
PID pidR(&measuredVelR, &pwmR, &velR, 2,1,0, DIRECT);
PID pidHeading(&currentHeading, &angularVelocity, &goalHeading, 2,1,0, DIRECT);

boolean pidActive= false;

unsigned long previousMillis = 0;
unsigned int interval = 10; // in ms
unsigned int debugInterval = 100; // in ms
unsigned long debugPreviousMillis = 0;

int goal = 1;
int totalTrajectory = 7;
int trajectoryX[] = {0,  0, 30, 60, 60,  0, 0};
int trajectoryY[] = {0, 30, 30, 60, 30, 30, 0};
const float distThresh  = 1;
const float angleThresh = 0.5;
boolean turnFlag = false;
boolean StraightFlag = false;
const int maxVel=30;
float errorHeading =0;
         
void setup() {
  Serial.begin(115200);
  motorL.setDir(FORWARD);
  motorR.setDir(FORWARD);
  
  navigator.InitEncoder( WHEEL_DIAMETER, WHEELBASE, TICKS_PER_REV );
  navigator.SetDistanceScaler( DISTANCE_SCALER );
  navigator.SetWheelbaseScaler( WHEELBASE_SCALER );
  navigator.SetWheelRLScaler( WHEEL_RL_SCALER );
  navigator.SetMinInterval(interval);
  navigator.Reset(millis());

  initPID();
  pidL.SetMode(AUTOMATIC);
  pidR.SetMode(AUTOMATIC);
  pidHeading.SetMode(AUTOMATIC);
}

void loop() {   
  pidHeading.Compute();
  pidL.Compute();
  pidR.Compute();
  if(velL>0) motorL.setPWM(pwmL);
  else motorL.setPWM(0);
  if(velR>0) motorR.setPWM(pwmR);
  else motorR.setPWM(0);

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    encCurrL = encL.read(); encL.write(0); 
    encCurrR =-encR.read(); encR.write(0);
    navigator.UpdateTicks(encCurrL, encCurrR, currentMillis);
    float distanceL = (float)encCurrL*CM_DISTANCE_PER_TICK;
    measuredVelL = (float)distanceL*(1000.0/interval);
    float distanceR = (float)encCurrR*CM_DISTANCE_PER_TICK;
    measuredVelR = (float)distanceR*(1000.0/interval);
    computeVelocities();
  }

  if (currentMillis - debugPreviousMillis >= debugInterval) {
    debugPreviousMillis = currentMillis;
    Serial.print(goal);
    Serial.print(", ");
    Serial.print(goalHeading);
    Serial.print(", ");
    Serial.print(navigator.Position().x/10);
    Serial.print(", ");
    Serial.print(navigator.Position().y/10);
    Serial.print(", ");
    Serial.println(navigator.Heading());
  }
}

void initPID(void){
  pidL.SetMode(MANUAL); // PID CONTROL OFF
  pidR.SetMode(MANUAL);
  pidL.SetSampleTime(interval); // sample time for PID
  pidR.SetSampleTime(interval);
  pidL.SetOutputLimits(0,250);  // min/max PWM
  pidR.SetOutputLimits(0,250);  // min/max PWM

  pidHeading.SetMode(MANUAL);
  pidHeading.SetSampleTime(interval); // sample time for PID
  pidHeading.SetOutputLimits(0,30);
}

void computeVelocities(void){
  int v = maxVel;
  int x_g = trajectoryX[goal];  
  int y_g = trajectoryY[goal];  
  
  float x = navigator.Position().x/10;
  float y = navigator.Position().y/10;
  currentHeading=navigator.Heading(); // in degrees
  
  float dist = abs(sqrt((y_g-y)*(y_g-y) + (x_g-x)*(x_g-x)));

  //1. Calculate the heading (angle) to the goal.
  //distance between goal and robot in x-direction
  float u_x = x_g-x;     
  //distance between goal and robot in y-direction
  float u_y = y_g-y;
  //angle from robot to goal. Hint: use ATAN2, u_x, u_y here.
  goalHeading = nvRadToDeg(atan2(u_y,u_x));

  errorHeading = nvDegToRad(nvDegToRad(goalHeading)-nvDegToRad(currentHeading));
  errorHeading = nvRadToDeg((atan2(sin(errorHeading),cos(errorHeading))));
  
  if((abs(errorHeading)>angleThresh) && (dist>distThresh)){
    turnFlag = true;
    StraightFlag = false;
    velR  = (2*v+angularVelocity*(WHEELBASE/10.0))/(WHEEL_DIAMETER_CM);
    velL  = (2*v-angularVelocity*(WHEELBASE/10))/(WHEEL_DIAMETER_CM);
  }else if(abs(errorHeading)<angleThresh){
    turnFlag = false;
    StraightFlag = true;
  }
  
  if(StraightFlag){
    if(dist>distThresh){
      velR = v;
      velL = v;
    }else{
      if(goal>totalTrajectory-1){
        velR = 0;
        velL = 0;
        pidR.SetMode(MANUAL);
        pidL.SetMode(MANUAL);
      }else{
        goal++;
        Serial.println("========NEXT POINT=======");
      }
    }
  }
}
