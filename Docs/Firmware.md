This replacement firmware makes several improvements on the old including:

 - Faster rotation and much faster homing and calibration using the settings from the old firmware.
 - Settings can be customized from the Configurator (details in Configurator.txt)
 - Code redone in a more OOP style and (hopefully) easier to learn.
 - Memory use reduced by a fraction (2%)
 - Flash use about the same.
 What it may not do is operate the Shutter correctly. I have no shutter and I honestly don't know how (or even if) it works.
 I'll try to include all the functionality that was in the original firmware but make no promises.

 The big functional change is the addition of being able to change the settings of the controller manually. To do this I've
 added a lot of communications functions and written the Configurator utility. Under the hood it's still kind of clunky but
 that will improve in a way invisible to the end user.

 The near future plans are to:
- Change the char() based serial buffer system to a string buffer.
- Expand on the EEPROM loading and saving methods to allow single setting saves rather than all at once.
- Create an Observatory class that will contain the rotator and shutter classes (and whatever other accessories appear).

Further afield:
- Revamp communications again once I've dug into the ASCOM driver. I can't be too free spirited with this because it will
crash the existing driver if I send unexpected messages while it is in control.