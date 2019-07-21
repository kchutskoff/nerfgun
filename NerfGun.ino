#include <Servo.h>

// defining pins for various hardware devices
const uint8_t MOTOR_LEFT_PIN = 9;
const uint8_t MOTOR_RIGHT_PIN = 10;
const uint8_t TRIGGER_PIN = 7;
const uint8_t SOLENOID_PIN = 5;

// the minimum and maximum duty cycle that the ESCs for the motors understand.
// Note, you need to make sure the ESC has been calibrated to use these duty cycles (see "Configuring ESCs" in README for more info)
const int MOTOR_DUTY_MIN = 1000;
const int MOTOR_DUTY_MAX = 2000;

const float MotorLeftToRightRatio = 1; // control the ratio between motors, may have to change to something variable later
const int SolenoidFireMS = 100; // how long the solenoid is fired to send a shot
const int SolenoidResetMS = 150; // how long the solenoid waits after firing before it can fire again
const int ButtonDebounceMS = 50; // how long to ignore inputs afer it changes


Servo MotorLeft;
Servo MotorRight;

// Debounces a button by ignoring input for a small period of time after a change is detected, returns the debounced reading
int CheckDebounceButton(uint8_t pin, int lastValue, int& lastChange, int debounceTimeout){
	int reading = digitalRead(pin);
	int now = millis();
	if(reading != lastValue && now - lastChange >= debounceTimeout){
		lastChange = now;
		return reading;
	}
	return lastValue;
}

void setup() {
	// put your setup code here, to run once:
	MotorLeft.attach(MOTOR_LEFT_PIN);
	MotorRight.attach(MOTOR_RIGHT_PIN);

	// esc expects speed between min and max, so start by writing minimum throttle
	MotorLeft.writeMicroseconds(MOTOR_DUTY_MIN); 
	MotorRight.writeMicroseconds(MOTOR_DUTY_MIN); 

	pinMode(TRIGGER_PIN, INPUT); // set the trigger pin to read
	pinMode(SOLENOID_PIN, OUTPUT); // set the solenoid pin to output

	digitalWrite(SOLENOID_PIN, LOW); // don't turn on the solenoid
}

int prevTriggerState = LOW;
int prevTriggerMS = 0;

// if set to 0, we're in full auto mode
uint8_t numberOfShots = 1;
uint8_t shotsRemaining = 0;

enum ShotStateEnum{
	Ready, // everything is ready to fire
	Firing, // we are in the process of firing a shot (todo: make more states)
	Resetting // resetting for the next shot
};

ShotStateEnum shotState = Ready;

void loop() {
	// put your main code here, to run repeatedly:

	// update button states
	int curTriggerState = CheckDebounceButton(TRIGGER_PIN, prevTriggerState, prevTriggerMS, ButtonDebounceMS);

	// apply values
	prevTriggerState = curTriggerState;

}
