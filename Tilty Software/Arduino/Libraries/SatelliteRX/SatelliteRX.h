/*
SatelliteRX.h - A easy to use interface for reading a Spektrum Satellite R/C receiver
Copyright (C) 2013-2014 Alex Beattie <alexbeattie at tiltyimu dot com>

This program is free software: you can redistribute it and/or modify
it under the terms of the version 2.5 GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

#ifndef SATELLITE_RX_H
#define SATELLITE_RX_H

// Setting defines
//#define DEBUG_RX
#define RX_BINDING_TIMEOUT 30000// Amount of time to wait for binding to occur in milliseconds
//#define TX_MODE_1
#define TX_MODE_2
#define RX_INIT_TIMEOUT 5000

#ifdef TX_MODE_2
	#define AILERON 1
	#define ELEVATOR 2
	#define RUDDER 3
	#define THROTTLE 0
	#define AUX1 4
	#define AUX2 5
	#define AUX3 6
#endif

class SatelliteRX {
	public:
	// Constructors
		SatelliteRX(); // NOT IMPLEMENTED
		SatelliteRX(HardwareSerial& _serial);
		//SatelliteRX(HardwareSerial3& _serial);
		//SatelliteRX(HardwareSerial _serial, uint8_t _pin);
		//SatelliteRX(uint8_t _pin);
		~SatelliteRX();
	
	// Functions
		bool init(); // Starts the serial port
		bool init(bool _bind); // If true, binds the receiver, then starts the serial port. Just starts serial if false.
		
		bool bind();
		
		bool readData();
		bool readData(int32_t* _data); // Takes a 7 variable array to store channel data	NOT IMPLEMENTED
		bool readData(int16_t* _data); // Takes a 7 variable array to store channel data	NOT IMPLEMENTED
		
		void readRawData(); // NOT IMPLEMENTED, NON-RAW SHOULD SCALE TO +- A NUMBER
		void readRawData(int32_t* _data); // Takes a 7 variable array to store channel data	NOT IMPLEMENTED
		void readRawData(int16_t* _data); // Takes a 7 variable array to store channel data	NOT IMPLEMENTED
		
		bool calibrate(); // NOT IMPLEMENTED
		
		void setAileron(int* _data);
		void setElevator(int* _data);
		void setRudder(int* _data);
		void setThrottle(int* _data);
		void setAux1(int* _data);
		void setAux2(int* _data);
		void setAux3(int* _data);
		
		
	// Variables
		uint16_t channel_data[9];// Uses 7 for most satellite receivers, but some support more (will need testing)
		
		
	private:
	// Functions
		void updatePointers();
		
	// Variables
		long lastRead;
		bool synced;
		HardwareSerial& _serialPort; // Serial port to read data from
		uint8_t rx_pin;
		
		int16_t channel_max[7];
		int16_t channel_min[7];
		
		int* _aileron;
		int* _elevator;
		int* _rudder;
		int* _throttle;
		int* _aux1;
		int* _aux2;
		int* _aux3;
		
		bool _set_aileron;
		bool _set_elevator;
		bool _set_rudder;
		bool _set_throttle;
		bool _set_aux1;
		bool _set_aux2;
		bool _set_aux3;
		
		uint8_t buffer_index;
		byte _buffer[16]; // Buffer for serial data
};

#endif