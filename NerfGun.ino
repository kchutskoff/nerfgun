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


const int SolenoidFireMS = 100; // how long the solenoid is fired to send a shot
const int SolenoidResetMS = 150; // how long the solenoid waits after firing before it can fire again
const int ButtonDebounceMS = 50; // how long to ignore inputs afer it changes
const int Motor_Wait_Speed = 1100; // the speed to hold the motor at while in Spin_Wait

// defines a collection of timings for the various motor modes
struct MotorSettingsStruct{
	int MotorRampUpMS; // how long the motor will need to ramp up to speed before the solenoid can be engaged
	int MotorFireMS; // how long the motor should maintain its speed before it can be reduced once a shot is fired
};

class ShotTimings{
	public:
		int SolenoidStartTime;
		int SolenoidResetTime;
		int SolenoidEndTime;
		int MotorEndTime;

	ShotTimings(int now, MotorSettingsStruct motorSettings, bool isHot){
		if(isHot){
			SolenoidStartTime = now;
			SolenoidResetTime = now + SolenoidFireMS;
			SolenoidEndTime = now + SolenoidFireMS + SolenoidResetMS;
			MotorEndTime = now + motorSettings.MotorFireMS;
		}else{
			SolenoidStartTime = now + motorSettings.MotorRampUpMS;
			SolenoidResetTime = now + motorSettings.MotorRampUpMS + SolenoidFireMS;
			SolenoidEndTime = now + motorSettings.MotorRampUpMS + SolenoidFireMS + SolenoidResetMS;
			MotorEndTime = now + motorSettings.MotorRampUpMS + motorSettings.MotorFireMS;
		}
	}

	ShotTimings(){
		SolenoidStartTime = SolenoidEndTime = SolenoidResetTime = MotorEndTime = 0;
	}
};

// these values need to be tuned to match the hardware
MotorSettingsStruct SilentMotorSettings = {
	100,
	200
};

MotorSettingsStruct NormalMotorSettings = {
	50,
	200
};

MotorSettingsStruct PerformanceMotorSettings = {
	0,
	200
};

enum ShotStateEnum{
	Ready, // everything is ready to fire
	Firing
};

enum ShotTypeEnum{
	Single,
	Double,
	Triple,
	Full_Auto
};

// Servo classes for the left and right motor. We could technically use both motors on one pin, but for now we will use two in case was want to independantly adjust the PWM signal per motor (if one runs faster, etc)
Servo MotorLeft;
Servo MotorRight;

enum ButtonChangeEnum{
	Held_Low,
	Held_High,
	Low_High,
	High_Low
};

enum TriggerStateEnum{
	Pressed, // trigger is currently being pressed
	Tapped, // trigger was pressed and released before the shot was ready
	Held, // trigger is still being held after shot was registered
	Released // triggere was released after the shot was registered
};

enum MotorStateEnum{
	Wait_Speed, // the motor is spinning at the wait speed
	Shot_Speed, // the motor is spinning at the shot speed
	Hot_Wait_Speed // the motor is spinning at the shot speed, waiting for the next shot
};

enum SolenoidStateEnum{
	At_Rest, // in the ready position
	Moving_Out, // in (or transitioning to) the fire position
	Moving_Back // resetting back to the ready position
};

enum MotorModeEnum{
	Silent, // the motors turn off when not firing
	Normal, // the motors run at a low speed (Spin_Wait) when not firing
	Performance // the motors run at full speed when not firing
};

// Debounces a button by ignoring input for a small period of time after a change is detected, returns the debounced reading
ButtonChangeEnum CheckDebounceButton(uint8_t pin, int now, int& lastValue, int& lastChange, int debounceTimeout){
	int reading = digitalRead(pin);
	ButtonChangeEnum output;
	if(reading != lastValue && now - lastChange >= debounceTimeout){
		// value is different and its been long enough since the last change
		if(lastValue == LOW){
			output = Low_High;
		}else{
			output = High_Low;
		}
		// update the last change time and value
		lastChange = now;
		lastValue = reading;
	}else{
		// not changed, or not long enough, just hold the current value
		if(lastValue == LOW){
			output = Held_Low;
		}else{
			output = Held_High;
		}
	}
	return output;
}


int prevTriggerValue = LOW;
int prevTriggerMS = 0;

int motorStartMS = 0;
int solenoidStartMS = 0;

int motorSpeed = MOTOR_DUTY_MAX; // tood: make this configurable and possibly use a different speed if necessary

TriggerStateEnum triggerState = Released;
ShotStateEnum shotState = Ready;
ShotTypeEnum shotType = Single;
SolenoidStateEnum solenoidState = At_Rest;
MotorStateEnum motorState = Wait_Speed;

// these are assigned together when the motor mode changes
MotorModeEnum motorMode = Silent;
MotorSettingsStruct motorSettings = SilentMotorSettings;

// when we're shooting, we need to keep track of how many we've shot and how many we have remaining
uint8_t shotsRemaining = 0;

ShotTimings shotTimings;

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


void loop() {
	// put your main code here, to run repeatedly:

	// record the current MS (we will need it for a bunch of things)
	int now = millis();

	// determine what happened with the button, internally updates prevTriggerValue and prevTriggerMS
	ButtonChangeEnum triggerChange = CheckDebounceButton(TRIGGER_PIN, now, prevTriggerValue, prevTriggerMS, ButtonDebounceMS);
	if(triggerChange == High_Low){
		// let go of trigger
		if(triggerState == Pressed){
			// was pressed, but released before shot was ready, transition to Tapped
			triggerState = Tapped;
		}else if(triggerState == Held){
			// was released after the shot was registered, transition to Released
			triggerState = Released;
		}
	}else if(triggerChange == Low_High){
		// pressed trigger
		if(triggerState == Released || triggerState == Tapped){
			// either we had released the trigger after the last shot, or we had previously tapped the trigger, and now we are pressing again before the shot is ready, transition back to pressed (since we don't want to queue up more than one shot)
			triggerState = Pressed;
		}
	}

	if(shotState == Ready && (triggerState == Pressed || triggerState == Tapped))
	{
		// ready, and trigger was pressed, fire the gun
		shotState = Firing;
		if(triggerState == Pressed){
			// its being held
			triggerState = Held;
		}else{
			// we were tapped (let go already), so transition to released
			triggerState = Released;
		}
		// spin up motors
		MotorLeft.writeMicroseconds(motorSpeed);
		MotorRight.writeMicroseconds(motorSpeed);
		motorState = Shot_Speed;

		if(shotType == Single){
			shotsRemaining = 1;
		}else if(shotType == Double){
			shotsRemaining = 2;
		}else if(shotType == Triple){
			shotsRemaining = 3;
		}else{
			shotsRemaining = 0; // full auto doesn't have any remaining
		}
		// calculate the timings for this shot
		shotTimings = ShotTimings(now, motorSettings, false);
	}

	if(shotState == Firing){
		if(solenoidState == At_Rest && now >= shotTimings.SolenoidStartTime){
			// motors have had enough time to ramp up, start the solenoid
			digitalWrite(SOLENOID_PIN, HIGH);
			solenoidState = Moving_Out;
		}

		if(motorState == Shot_Speed && now >= shotTimings.MotorEndTime){
			// the shot should have been fired, see if we should be stopping the motors or not
			if(shotType != Full_Auto){
				// not full auto, subtract the shot we just made
				shotsRemaining--;
				if(shotsRemaining == 0){
					// this was the last shot, need to spin down the motors
					motorState = Wait_Speed;
				}else{
					// we have more shots, don't spin down the motor
					motorState = Hot_Wait_Speed;
				}
			}else if(triggerState == Released){
				// full auto, but trigger released, stop the shot
				motorState = Wait_Speed;
			}

			if(motorState == Wait_Speed){
				// motors are going to start waiting, set their speed
				if(motorMode == Silent){
					// stop motors
					MotorLeft.writeMicroseconds(MOTOR_DUTY_MIN);
					MotorRight.writeMicroseconds(MOTOR_DUTY_MIN);
				}else if(motorMode == Normal){
					MotorLeft.writeMicroseconds(Motor_Wait_Speed);
					MotorRight.writeMicroseconds(Motor_Wait_Speed);
				}
				// if motor speed is performance, don't have to do anything
			}
			
		}

		if(solenoidState == Moving_Out && now >= shotTimings.SolenoidResetTime){
			// solenoid should have finished moving, turn it off
			digitalWrite(SOLENOID_PIN, LOW);
			solenoidState = Moving_Back;
		}

		if(solenoidState == Moving_Back && now >= shotTimings.SolenoidEndTime){
			// solenoid is done moving back
			solenoidState = At_Rest;
		}

		if((motorState == Wait_Speed || motorState == Hot_Wait_Speed) && solenoidState == At_Rest){
			// the shot is done since both the motor and the solenoid are ready again
			if(motorState == Hot_Wait_Speed){
				// we are going around for another shot
				shotTimings = ShotTimings(now, motorSettings, true); // this shot is hot, can use different timings
			}else{
				// nothing more to shoot, mark us as ready
				shotState = Ready;
			}
		}
	}
}
