/*
 * main.h
 *
 *  Created on: Februar 2022
 *      Author: Frank Weichert
 */


/// Detects for a given buttonIndex if the corrosponding button has been triggered or not.
/// @param buttonIndex index of the Button to check
/// @return -1 if a falling edge is detected, +1 if a raising edge detected, 0 if no change occured between two function calls
int detectEdge(int buttonIndex);

///
/// smoothed flickering in home position.  DEADZONE macro used to define the smoothed area
/// @param value the current value of the position
/// @return 0 if absolute of value less than DEADZONE, otherwise value
int16_t respectDeadZone(int16_t value);


/// reads Joystick Calibration Data from NVRAM if available
void readNVRAM();

/// updates the life calibration, safes data in EEPROM if necessary
void updateCalibration();

/// Cleans up NVRAM to be prepared for a new initialization
void clearNVRAM();