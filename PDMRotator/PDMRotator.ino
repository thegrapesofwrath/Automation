/*
* PDM NexDome Rotation kit firmware. NOT compatible with original NexDome ASCOM driver.
*
* Copyright � 2018 Patrick Meloy
* Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
*  files (the �Software�), to deal in the Software without restriction, including without limitation the rights to use, copy,
*  modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
*  is furnished to do so, subject to the following conditions:
*  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED �AS IS�, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
*  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
*  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
*  OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*  Inspired by the original official NexDome firmware by grozzie2 but completely incompatible.
*  https://github.com/grozzie2/NexDome
*/

// Uncomment #define DEBUG in RotatorClass.h to enable printing debug messages in serial

#include "RotatorClass.h"
#include "RemoteShutterClass.h"
//#include <XBeeClass.h>
#include <AccelStepper.h>

//todo: Fix calibration so it stops at home then goes positive only.
//todo: Implement low voltage safety
//todo: Rain sensor flakey, maybe fast polling too?
//todo: Make debug prints show up in ASCOM "Traffic" form?
//todo: Decide on a "comment" char to start all debug printing with so
// ASCOM definately won't be affected (all messages validated anyway but
// that'll stop mistaken "Invalid" messages.

#pragma region Devices
RotatorClass Rotator  = RotatorClass();
RemoteShutterClass RemoteShutter = RemoteShutterClass();
//XBeeClass XBee = XBeeClass();

const String VERSION = "0.5.0";

#define Computer Serial
String computerBuffer;

#define Wireless Serial1
String wirelessBuffer;

#pragma endregion

#pragma region Declarations and Variables

// Flag to doe XBee startup on first boot in loop(). Could do in setup but
// serial may not be ready so debugging prints won't show. Also used
// to make sure the XBee has started and configured itself before
// trying to send any wireless messages.
bool XBeeStarted = false;

// Once booting is done and XBee is ready, broadcast a hello message
// so a shutter knows you're around if it is already running. If not,
// the shutter will send a hello when it's booted up.
bool SentHello = false;

// Period between checks for rain. Sensor pin pulled high and active low so if nothing
// is connected it never reports rain. The value has been flakey so I've disabled for now.
// TODO: Fix rain sensor readings.
// Status is checked periodically (see Rotator.RainCheckInterval()) but only sent when
// the status changes (hence the lastIsRaining).
unsigned long nextRainCheck;
bool lastRainStatus = false, GetRainStatus = false;
#pragma endregion

#pragma region command constants
/* Shutter talks asyncronously so any immediate return shows the old
// value since only the Setup form changes the settings and it has to close
// to do so, that doesn't matter. Once the Setup dialog OK is received,
// Grab all the settings from the Shutter and store in RemoteShutter so
// next time the Setup form is loaded it has the new values.
// Shutter is usually uppercase of rotator version.
// Name order is weird just because auto-complete is easier this way.
// had to use char because switch{} can't handle strings.
//
// All responses are preceeded by the command char and while it is needed by
// the shutter, ASCOM talks synchronously and actually has to dispose of that
// first character. Leaving it in for now since it's easier to read when just
// using a serial terminal to talk to the rotator. Plus who knows what other
// programs people might write that may use async comms.
//
// Originally tried to make the letters make sense but there are too many. May
// just re-do to go in alphabetical order since I use the constant labels anyway.
// TODO: Alphabetize the characters if I get motivated enough.
// NOTE: Used labels because it's nearly impossible to remember all the character
// meanings. Plus this lets me change the letters without changing code. Have
// to make the same changes in the shutter and ASCOM but cut & paste makes that easy.
*/
const String DEBUGStart = ""; // Come up with a good character to use after alphabetizing
const String WIRELESS_DEBUG_COMMENT		= "B"; // Handy debug messages sent to Shutter which can serial print. Used directly, not in case statements
const char ACCELERATION_ROTATOR_CMD		= 'e'; // Get/Set stepper acceleration
const char ABORT_MOVE_CMD				= 'a'; // Tell everything to STOP!
const char CALIBRATE_ROTATOR_CMD		= 'c'; // Calibrate the dome
const char ERROR_AZ_ROTATOR_GET			= 'o'; // Azimuth error when I finally implement it
const char GOTO_ROTATOR_CMD				= 'g'; // Get/set dome azimuth
const char HOME_ROTATOR_CMD				= 'h'; // Home the dome
const char HOMEAZ_ROTATOR_CMD			= 'i'; // Get/Set home position
const char HOMESTATUS_ROTATOR_GET		= 'z'; // Get homed status
const char MOVE_RELATIVE_ROTATOR_CMD	= 'b'; // Move relative - steps from current position +/-
const char PARKAZ_ROTATOR_CMD			= 'l'; // Get/Set park azimuth
const char POSITION_ROTATOR_GET			= 'p'; // Get/Set step position
const char RAIN_ROTATOR_CMD				= 'f'; // Get rain sensor state /Set rain check interval
const char REVERSED_ROTATOR_CMD			= 'y'; // Get/Set stepper reversed status 
const char SEEKSTATE_GET				= 'd'; // None, homing, calibration steps.
const char SLEW_ROTATOR_GET				= 'm'; // Get Slewing status/direction
const char SPEED_ROTATOR_CMD			= 'r'; // Get/Set step rate (speed)
const char STEPSPER_ROTATOR_CMD			= 't'; // GetSteps per rotation
const char SYNC_ROTATOR_CMD				= 's'; // Sync to telescope
const char VERSION_ROTATOR_GET			= 'v'; // Get Version string
const char VOLTS_ROTATOR_CMD			= 'k'; // Get volts and get/set cutoff

const char ACCELERATION_SHUTTER_CMD		= 'E'; // Get/Set stepper acceleration
const char CALIBRATE_SHUTTER_CMD		= 'L'; // Calibrate the shutter
const char CLOSE_SHUTTER_CMD			= 'C'; // Close shutter
const char ELEVATION_SHUTTER_CMD		= 'G'; // Get/Set altitude
const char HELLO_CMD					= 'H'; // Let shutter know we're here
const char HOMESTATUS_SHUTTER_GET		= 'Z'; // Get homed status (has it been closed)
const char INACTIVE_SHUTTER_CMD			= 'X'; // Get/Set how long before shutter closes
const char OPEN_SHUTTER_CMD				= 'O'; // Open the shutter
const char POSITION_SHUTTER_GET			= 'P'; // Get step position
const char RAIN_SHUTTER_CMD					= 'F'; // Tell shutter if it'raining or not
const char SPEED_SHUTTER_CMD			= 'R'; // Get/Set step rate (speed)
const char REVERSED_SHUTTER_CMD			= 'Y'; // Get/Set stepper reversed status
const char SLEEP_SHUTTER_CMD			= 'S'; // Get/Set radio sleep settings
const char STATE_SHUTTER_GET			= 'M'; // Get shutter state
const char STEPSPER_SHUTTER_CMD			= 'T'; // Get/Set steps per stroke
const char VERSION_SHUTTER_GET			= 'V'; // Get version string
const char VOLTS_SHUTTER_CMD			= 'K'; // Get volts and get/set cutoff

#pragma endregion

/*
** An XBee may or may not be present and we don't want to waste time trying to
** talk to an XBee that isn't there. Good thing is that all serial comms are asychronous
** so just try to start a read-only check of it's configuration then if it responds go
** ahead and use it. if it doesn't respond, there's nothing that will talk to it. Config
** routine sets XBee.present to true if the XBee responds.
*/
#pragma region "Arduino Setup and Loop"
void setup()
{
	Computer.begin(9600);
	Wireless.begin(9600);
	delay(2000);
}
void loop()
{
	Rotator.run();

	XBeeStartup();

	CheckForCommands();

	CheckForRain();
}
#pragma endregion

#pragma region Periodic and Helper functions

//<SUMMARY>Start configuration routine then send Hello broadcast</SUMMARY>
void XBeeStartup()
{
	if (XBeeStarted == false)
	{
		DBPrintln("Rain interval is " + String(Rotator.rainCheckInterval));
		ConfigXBee();
		XBeeStarted = true;
	}
	if (SentHello == false && XBeeStarted == true)	SendHello();
}

void ConfigXBee()
{
	delay(1000); // Guard time
	Wireless.println("+++");
	delay(1000); // Guard time
	Wireless.println("ATCE0AP0SM0PL0CN");
}

// <SUMMARY>Broadcast that you exist</SUMMARY>
void SendHello()
{
	DBPrintln("Sending Hello");
	Wireless.print(String(HELLO_CMD) + "#");
	SentHello = true;
}

//<SUMMARY>Check for Serial and Wireless data</SUMMARY>
void CheckForCommands()
{
	if (Computer.available())
	{
		ReceiveComputer();
	}
	if (Wireless.available())
	{
		ReceiveWireless();
	}
}


//<SUMMARY>Tells shutter the rain sensor status</SUMMARY>
void CheckForRain()
{
	// Only check periodically (fast reads seem to mess it up)
	// Disable by setting rain check interval to 0;
	if (millis() > nextRainCheck)
	{
		int rainingNow = (int)Rotator.GetRainStatus();
		DBPrintln("Rain sensor says " + String(rainingNow));
		if (lastRainStatus != rainingNow)
		{
			GetRainStatus = lastRainStatus = rainingNow;
			Wireless.print(String(RAIN_SHUTTER_CMD) + String(GetRainStatus) + "#");
		}
		nextRainCheck = millis() + (Rotator.rainCheckInterval * 1000);
	}
}

//<SUMMARY>Send debug comment to shutter</SUMMARY>
//Handy when rotator is connected to something else so you
//can't print to serial.
// TODO: Get rid of this once ASCOM can handle debug comments
void WirelessComment(String comment)
{
	Wireless.print(WIRELESS_DEBUG_COMMENT + comment + "#");
}
#pragma endregion

#pragma region Serial handling
// All ASCOM comms are terminated with # but left if the \r\n for compatibility
// with other programs.
void ReceiveComputer()
{
	char computerCharacter = Computer.read();

	if (computerCharacter == '\r' || computerCharacter == '\n' || computerCharacter == '#')
	{
		// End of message
		if (computerBuffer.length() > 0)
		{
			ProcessSerialCommand();
			computerBuffer = "";
		}
	}
	else
	{
		computerBuffer += String(computerCharacter);
	}
}
void ProcessSerialCommand()
{
	float localFloat;
	char command, localChar;
	String value, wirelessMessage;
	int localInt;
	String serialMessage, localString;
	bool hasValue = false, localBool = false;
	long localLong;

	// Split the buffer into command char and value
	// Command character
	command = computerBuffer.charAt(0);
	// Payload
	value = computerBuffer.substring(1);
	// payload has data (better one comparison here than many in code. Even though
	// it's still executed just once per loop.
	if (value.length() > 0) hasValue = true;

	// Grouped by Rotator and Shutter then put in alphabetical order
	switch (command)
	{
#pragma region Rotator commands
	case ABORT_MOVE_CMD:
		localString = String(ABORT_MOVE_CMD);
		serialMessage = localString;
		wirelessMessage = localString;
		Rotator.stop();
		break;
	case ACCELERATION_ROTATOR_CMD:
		if (hasValue == true) Rotator.SetAcceleration(value.toInt());
		serialMessage = String(ACCELERATION_ROTATOR_CMD) + String(Rotator.GetAcceleration());
		break;
	case CALIBRATE_ROTATOR_CMD:
		Rotator.StartCalibrating();
		serialMessage = String(CALIBRATE_ROTATOR_CMD);
		break;
	case ERROR_AZ_ROTATOR_GET:
		// todo: See if azimuth error is needed (when passing home switch check to see if the
		// actual position matches where the stepper thinks it is.
		serialMessage = String(ERROR_AZ_ROTATOR_GET) + "0";
		break;
	case GOTO_ROTATOR_CMD:
		if (hasValue == true)
		{
			localFloat = value.toFloat();
			if ((localFloat >= 0.0) && (localFloat <= 360.0))
			{
				Rotator.SetAzimuth(localFloat);
			}
		}
		serialMessage = String(GOTO_ROTATOR_CMD) + String(Rotator.GetAzimuth());
		break;
	case HELLO_CMD:
		SendHello();
		break;
	case HOME_ROTATOR_CMD:
		Rotator.startHoming();
		serialMessage = String(HOME_ROTATOR_CMD);
		break;
	case HOMEAZ_ROTATOR_CMD:
		if (hasValue == true)
		{
			localFloat = value.toFloat();
			if ((localFloat >= 0) && (localFloat < 360)) Rotator.SetHomeAzimuth(localFloat);
		}
		serialMessage = String(HOMEAZ_ROTATOR_CMD) + String(Rotator.GetHomeAzimuth());
		break;
	case HOMESTATUS_ROTATOR_GET:
		serialMessage = String(HOMESTATUS_ROTATOR_GET) + String(Rotator.GetHomeStatus());
		break;
	case MOVE_RELATIVE_ROTATOR_CMD:
		if (hasValue == true)
		{
			localLong = value.toInt();
			if (localLong > 0) Rotator.moveRelative(localLong);
		}
		serialMessage = String(MOVE_RELATIVE_ROTATOR_CMD);
		break;
	case PARKAZ_ROTATOR_CMD:
		// Get/Set Park Azumith
		localString = String(PARKAZ_ROTATOR_CMD);
		if (hasValue == true)
		{
			localFloat = value.toFloat();
			if ((localFloat >= 0) && (localFloat < 360))
			{
				Rotator.SetParkAzimuth(localFloat);
			}
			else
			{
				serialMessage = localString + "E";
			}
		}
		if (serialMessage.length() == 0) serialMessage = localString + String(Rotator.GetParkAzimuth());
		break;
	case POSITION_ROTATOR_GET:
		serialMessage = String(POSITION_ROTATOR_GET) + String(Rotator.GetPosition());
		break;
	case RAIN_ROTATOR_CMD: // TODO: Have Shutter ask for rain status
		if (hasValue == true)
		{
			localInt = value.toInt();
			DBPrintln("Set rain timer to " + String(localInt));
			if (localInt < 0) localInt = 0;
			Rotator.rainCheckInterval = localInt;
		}
		serialMessage = String(RAIN_ROTATOR_CMD) + String(Rotator.rainCheckInterval);
		break;
	case SPEED_ROTATOR_CMD:
		if (hasValue == true) Rotator.SetMaxSpeed(value.toInt());
		serialMessage = String(SPEED_ROTATOR_CMD) + String(Rotator.GetMaxSpeed());
		break;
	case REVERSED_ROTATOR_CMD:
		if (hasValue == true) Rotator.SetReversed(value.toInt());
		serialMessage = String(REVERSED_ROTATOR_CMD) + String(Rotator.GetReversed());
		break;
	case SEEKSTATE_GET:
		serialMessage = String(SEEKSTATE_GET) + String(Rotator.GetSeekMode());
		break;
	case SLEW_ROTATOR_GET: // TODO: See if it's better for ASCOM to figure this out
		serialMessage = String(SLEW_ROTATOR_GET) + String(Rotator.GetDirection());
		break;
	case STEPSPER_ROTATOR_CMD:
		if (hasValue == true) Rotator.SetStepsPerRotation(value.toInt());
		serialMessage = String(STEPSPER_ROTATOR_CMD) + String(Rotator.GetStepsPerRotation());
		break;
	case SYNC_ROTATOR_CMD:
		localFloat = value.toFloat();
		serialMessage = String(SYNC_ROTATOR_CMD);
		if (localFloat >= 0 && localFloat < 360)
		{
			Rotator.syncHome(localFloat);
			Rotator.syncPosition(localFloat);
		}
		else
		{
			// Outside of boundaries, attach error character.
			serialMessage += "o";;
		}
		break;
	case VERSION_ROTATOR_GET:
		serialMessage = String(VERSION_ROTATOR_GET) + VERSION;
		break;
	case VOLTS_ROTATOR_CMD:
		// value only needs infrequent updating. 
		if (hasValue == true)
		{
			DBPrintln("Rotator LVC " + value);
			Rotator.SetLowVoltageCutoff(value.toInt());
		}
		serialMessage = String(VOLTS_ROTATOR_CMD) + String(Rotator.GetVoltString());
		break;
#pragma endregion

#pragma region Shutter Commands
	case ACCELERATION_SHUTTER_CMD:
		localString = String(ACCELERATION_SHUTTER_CMD);
		if (hasValue == true)
		{
			RemoteShutter.acceleration = value;
			wirelessMessage = localString + RemoteShutter.acceleration;
		}
		serialMessage = localString + RemoteShutter.acceleration;
		break;
	case CALIBRATE_SHUTTER_CMD:
		localString = String(CALIBRATE_SHUTTER_CMD);
		serialMessage = localString;
		wirelessMessage = localString;
		break;
	case CLOSE_SHUTTER_CMD:
		localString = String(CLOSE_SHUTTER_CMD);
		wirelessMessage = localString;
		serialMessage = localString;
		break;
	case ELEVATION_SHUTTER_CMD:
			localString = String(ELEVATION_SHUTTER_CMD);
			if (hasValue == true) wirelessMessage = localString + value;
			serialMessage = localString + RemoteShutter.elevation;
			break;
	case HOMESTATUS_SHUTTER_GET: // TODO: Figure this out if it's necessary
			// todo: Create shutter calibration and get that status here
			serialMessage = String(HOMESTATUS_SHUTTER_GET) + String(RemoteShutter.homedStatus);
			break;
	case OPEN_SHUTTER_CMD:
		localString = String(OPEN_SHUTTER_CMD);
		wirelessMessage = localString;
		serialMessage = localString;
		//RemoteShutter.state = RemoteShutter.OPENING;
		break;
	case POSITION_SHUTTER_GET:
			serialMessage = String(POSITION_SHUTTER_GET) + String(RemoteShutter.position);
			break;
	case REVERSED_SHUTTER_CMD:
		localString = String(REVERSED_SHUTTER_CMD);
		if (hasValue == true)
		{
			RemoteShutter.reversed = value;
			wirelessMessage = localString + value;
		}
		serialMessage = localString + RemoteShutter.reversed;
		break;
	case SLEEP_SHUTTER_CMD:
		localString = String(SLEEP_SHUTTER_CMD);
		if (hasValue == true)
		{
			RemoteShutter.sleepSettings = value;
			wirelessMessage = localString + value;
		}
		serialMessage = localString + RemoteShutter.sleepSettings;
		break;
	case SPEED_SHUTTER_CMD:
		localString = String(SPEED_SHUTTER_CMD);
		if (hasValue == true)
		{
			RemoteShutter.speed = value;
			wirelessMessage = localString + String(value.toInt());
		}
		serialMessage = localString + RemoteShutter.speed;
		break;
	case STATE_SHUTTER_GET:
		serialMessage = String(STATE_SHUTTER_GET) + RemoteShutter.state;
		break;
	case STEPSPER_SHUTTER_CMD:
		localString = String(STEPSPER_SHUTTER_CMD);
		if (hasValue == true)
		{
			RemoteShutter.stepsPerStroke = value;
			wirelessMessage = localString + value;
		}
		serialMessage = localString + RemoteShutter.stepsPerStroke;
		break;
	case VERSION_SHUTTER_GET:
		// Rotator gets this upon Hello and it's not going to change so don't ask for it wirelessly
		serialMessage = String(VERSION_SHUTTER_GET) + RemoteShutter.version;
		break;
	case VOLTS_SHUTTER_CMD:
		localString = String(VOLTS_SHUTTER_CMD);
		if (hasValue == true) wirelessMessage = localString + value;
		serialMessage = localString + RemoteShutter.volts;
		break;

#pragma endregion

	default:
		break;
	}

	// Send messages if they aren't empty.
	if (serialMessage.length() > 0)
	{
		Computer.print(serialMessage + "#");
	}

	if (wirelessMessage.length() > 0)
	{

		Wireless.print(wirelessMessage + "#");
	}

}
#pragma endregion

#pragma region Wireless Communications
void ReceiveWireless()
{
	char wirelessCharacter = Wireless.read();

	if (wirelessCharacter == '\r' || wirelessCharacter == '\n' || wirelessCharacter == '#')
	{
		if (wirelessBuffer.equals("OK") == true) DBPrintln("OK received");
		if (wirelessBuffer.length() > 0)
		{
			ProcessWireless();
			wirelessBuffer = "";
		}
	}
	else
	{
		wirelessBuffer += String(wirelessCharacter);
	}
}
void ProcessWireless()
{
	char sender, command, localChar;
	String value, serialMessage, wirelessMessage;
	int localInt;
	long localLong;
	float localFloat;
	bool hasValue = false;

	command = wirelessBuffer.charAt(0);
	value = wirelessBuffer.substring(1);
	if (value.length() > 0) hasValue = true;
	switch (command)
	{
	case ACCELERATION_SHUTTER_CMD:
		RemoteShutter.acceleration = value;
		DBPrintln("Got acceleration " + value);
		break;
	case CALIBRATE_SHUTTER_CMD:
		break;
	case ELEVATION_SHUTTER_CMD:
		DBPrintln("Got Elevation " + value);
		RemoteShutter.elevation = value;
		break;
		// Shutter said hello, ask for all the basically static information
		// Shutter sends the rest periodically without us asking.
	case HELLO_CMD:
		Wireless.print(String(ACCELERATION_SHUTTER_CMD) + "#");
		Wireless.print(String(ELEVATION_SHUTTER_CMD) + "#");
		Wireless.print(String(POSITION_SHUTTER_GET) + "#");
		Wireless.print(String(SPEED_SHUTTER_CMD) + "#");
		Wireless.print(String(REVERSED_SHUTTER_CMD) + "#");
		//Wireless.print(String(SLEEP_SHUTTER_CMD) + "#");
		Wireless.print(String(STATE_SHUTTER_GET) + "#");
		Wireless.print(String(STEPSPER_SHUTTER_CMD) + "#");
		Wireless.print(String(VERSION_SHUTTER_GET) + "#");
		Wireless.print(String(VOLTS_SHUTTER_CMD) + "#");
		break;
	//case HOMESTATUS_SHUTTER_GET:
	//	RemoteShutter.homedStatus = value;
	//	break;
	case POSITION_SHUTTER_GET:
		DBPrintln("Got Position " + value);
		RemoteShutter.position = value;
		break;
	case SPEED_SHUTTER_CMD:
		DBPrintln("Got Speed " + value);
		RemoteShutter.speed = value;
		break;
	case REVERSED_SHUTTER_CMD:
		DBPrintln("Got Reversed " + value);
		RemoteShutter.reversed = value;
		break;
	case SLEEP_SHUTTER_CMD: // Sleep settings mode,period,delay
		DBPrintln("Got Sleep " + value);
		RemoteShutter.sleepSettings = value;
		break;
	case STATE_SHUTTER_GET: // Dome status
		DBPrintln("Got Shutter State " + value);
		RemoteShutter.state = value;
		break;
	case STEPSPER_SHUTTER_CMD:
		DBPrintln("Got StepsPer " + value);
		RemoteShutter.stepsPerStroke = value;
		break;
	case VERSION_SHUTTER_GET:
		DBPrintln("Got Version " + value);
		RemoteShutter.version = value;
		break;
	case VOLTS_SHUTTER_CMD: // Sending battery voltage and cutoff
		DBPrintln("Got Volts " + value);
		RemoteShutter.volts = value;
		break;
	}
}
#pragma endregion


