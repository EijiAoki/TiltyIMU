#include <FastServo.h>
#include <PID.h>

#include <i2c_t3.h>
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"

#define USE_BT

MPU6050 mpu;
Servo roll_servo, pitch_servo, yaw_servo, throttle_servo;

float ypr[3]; // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

#define ROLL_PIN 2 // Servo header 1
#define ROLL_INDEX 2 // ypr[] data index
float roll_kP = 10;
float roll_kI = 1;
float roll_kD = 0.025;
float roll_value = 1500;
PID roll_pid = PID(&ypr[ROLL_INDEX], &roll_value, roll_kP, roll_kI, roll_kD, 1);

#define PITCH_PIN 3 // Servo header 2
#define PITCH_INDEX 1
float pitch_kP = 10;
float pitch_kI = 1;
float pitch_kD = 0.025;
float pitch_value = 1500;
PID pitch_pid = PID(&ypr[PITCH_INDEX], &pitch_value, pitch_kP, pitch_kI, pitch_kD, 1);

#define YAW_INDEX 0

#define YAW_PIN 23 // Servo header 4
#define THROTTLE_PIN 22 // Servo header 3

void setup() {
	#ifdef USE_BT
		Serial1.begin(115200);
		#define myPort Serial1
	#else
		Serial.begin(115200);
		#define myPort Serial
	#endif
	
	roll_servo.attach(ROLL_PIN); // Attach aileron servo to aileron servo header
	roll_pid.setLimits(-500, 500); // Sets servo output limits for PID
	pitch_servo.attach(PITCH_PIN); // Attach elevator servo to elevator servo header
	pitch_pid.setLimits(-500, 500); // Sets servo output limits for PID
	
	Wire.begin(I2C_MASTER, 0x00, I2C_PINS_18_19, I2C_PULLUP_EXT, I2C_RATE_400);//  Starts I2C on Teensy
	delay(5);
	
	while (!myPort.available()) {}
	while (myPort.available()) {	myPort.read();}
	
	setupDMP();
}

void loop() {
	readDMP();
	
	roll_pid.update();
	pitch_pid.update();
	
	roll_servo.writeMicroseconds(1500 + roll_value);
	pitch_servo.writeMicroseconds(1500 + pitch_value);
	
	if (myPort.available()) {
		if (myPort.read() == '0') {
			roll_pid.reset();
			pitch_pid.reset();
		}
	}
	
	myPort.print("Ailerons: ");
	myPort.print(roll_value);
	myPort.print("\tElevator: ");
	myPort.print(pitch_value);
	myPort.println();
}