# Mecanum-Camera-Rover
Project code to use an ESP32CAM (AI-Thinker) with and Arduino Uno and L239D motor controller shield to control a rover wirelessly with live camera feed and AWD motor control (used for Mecanum Wheels in this case.)

Repository off of which this project is based:
https://github.com/FedericoBusero/ESP32-CAM_ZeppelinCAM
[Thank you Federico!]

Setup:
ESP32CAM-> Runs webserver -> HTML+Processing OV2640 data for transmission, monitors events on websockets and sends status update as string to Uno over serial. 

Update Frame Structure:
   *SliderValue:JoystickXValue:JoystickYValue:ClockwiseButtonBoolean:CounterclockwiseButtonBoolean#

Ranges:
Slider: 0 to 360
Joystick X and Y: -180 to 180 for both
Rotation Buttons: 0 or 1

Notes:
- lower baudrate for esp32cam to arduino transmission seems more robust with no functional loss.
- do not send uodate over serial in main loop for esp firmware: causes issues with data segmentation probably because it is too fast.
- power esp32cam with external source instead of Uno to avoid brownout

 Enjoy.
 -Khizar

