# nerfgun
A Arduino project to build a brushless motor nerf gun.

## Configuring ESCs
Brushless motors use Electronic Speed Controllers (ESC) to control them. You can read into them more elsewhere, but essentially the Arduino talks to the ESCs with a 50 Mhz PWM signal (via the Servo library) to tell them what to do. The ESC interprets that signal and drives the motor accordingly.

Generally the PWM signal varies between 900-1100 microsecond duty cycle for minimum speed and 1900-2000 microsecond duty cycle for max speed. Many ESCs can be configured to adjust this range. For this project, I've adjusted the range to be between 1000 and 2000 microseconds.

Programming an ESC varies between makes and models, so consult the information provided by the manufacturer of your motor. The motors and ESCs I bought were unbranded and unlabeled, but I found some documentation that appears to match its functionality. You can find it in [External Docs](External%20Docs/30A_BLDC_ESC_Product_Manual.pdf).

For this project, I have also enabled the break for the motors, so they will spin down quickly instead of free-wheeling when the speed is reduced.