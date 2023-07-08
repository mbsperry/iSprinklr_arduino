#include "HunterRoam.h"

/* Receives command via serial and uses HunterRoam library to control sprinkler.

Command format:
- Send 0xFE to initiate command
- Terminate command with 0xFF
- Actual command is 3 bytes
- First byte is an integer with values of 1, 2, or 3
	1: Start zone, requires other 2 bytes to specify zone 
	and duration
	2: Stop zone, 2nd byte is zone
	3: run program. Not yet implemented
*/

byte cmdData[5];
boolean newData = false;
byte numByteRecd = 0;

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	Serial.begin(9600);
}


void loop() {
	HunterRoam smartPort(LED_BUILTIN);

	receiveData();

	if (newData == true) {
		if (checkData()) {
			switch (cmdData[0]) {
				case 0x1: 
					smartPort.startZone(cmdData[1], cmdData[2]);
					Serial.print("Start zone:");
					Serial.print(cmdData[1]);
					Serial.print("; Duration of: ");
					Serial.println((int)cmdData[2]);
					break;
				case 0x2:
					Serial.print("Stop zone:");
					Serial.println(cmdData[1]);
					smartPort.stopZone(cmdData[1]);
					break;
				case 0x3:
					Serial.println("Run program");
					break;
				default:
					Serial.println("Unknown cmd");
					break;
			}
			newData = false;
			// Clear old data
			memset(cmdData,0,sizeof(cmdData));
		} else {
			newData = false;
			// Clear old data
			memset(cmdData,0,sizeof(cmdData));
			Serial.println("Invalid Data");
		}
	}
	delay(100);
}

boolean checkData() {
	/* Validate data. First byte must be a 1,2 or 3. 
	Must have received 3 bytes total.
	If extra bytes, returns error because receive function resets
	to 0 if receiving over 3 bytes */

	boolean valid = true;

	if (cmdData[0] != 0x1 && cmdData[0] != 0x2 && cmdData[0] != 0x3) {
		valid = false;
		Serial.println("Invalid command");
		Serial.println(cmdData[0]);
	}

	if (numByteRecd < 3) {
		valid = false;
		Serial.println("Too few bytes");
	}
	return valid;
}

void receiveData() {
	byte recByte;
	byte startLine = 0xFE;
	byte endLine = 0xFF;
	static byte ndx = 0; // must be static because receiving data can be asynchronous
	static boolean recInProcess = false;
	numByteRecd = 0;

	while (Serial.available() > 0 && newData == false) {
		recByte = Serial.read();
		if (recInProcess == true) {
			if (recByte != endLine) {
				Serial.print("Index is: ");
				Serial.print(ndx);
				Serial.print(" Received: ");
				Serial.println(recByte);
				cmdData[ndx] = recByte;
				ndx = ndx + 1;
				if (ndx > 3) {
					Serial.println("Array overrun");
					numByteRecd = 0;
					ndx = 0;
					recInProcess = false;
					newData = true;
				}
			} else {
				Serial.println("Got something!");
				numByteRecd = ndx;
				ndx = 0;
				recInProcess = false;
				newData = true;
			}
		} else if (recByte == startLine) {
			recInProcess = true;
		}
	}

}
				
