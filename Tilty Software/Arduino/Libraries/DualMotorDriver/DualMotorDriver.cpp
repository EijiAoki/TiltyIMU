/*
DualMotorDriver.cpp - Software library to program and control the TiltyIMU Dual Motor Driver Shield
Copyright (C) 2013-2014 Alex Beattie <alexbeattie at tiltyimu dot com>

This program is free software: you can redistribute it and/or modify
it under the terms of the version 2 GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Arduino.h"

#if !defined(__MK20DX128__) && !defined(__MK20DX256__)

#include "wiring_private.h"
#include "DualMotorDriver.h"


DualMotorDriver MotorDriver;

// Add-on/shield code
DualMotorDriver::DualMotorDriver()
{	
	// Initialize all of the I/O pins
	pinMode(5, OUTPUT);	digitalWrite(5, LOW);// M1
	pinMode(6, OUTPUT);	digitalWrite(6, LOW);// M2
	
	pinMode(7, OUTPUT);	digitalWrite(7, LOW);// M1H
	pinMode(8, OUTPUT);	digitalWrite(8, HIGH);// M1L
	pinMode(4, OUTPUT);	digitalWrite(4, LOW);// M2H
	pinMode(9, OUTPUT);	digitalWrite(9, HIGH);// M2L
	
#ifdef DEBUG_MOTOR_DRIVER
	pinMode(0, OUTPUT);
#endif
	
	pinMode(M1_SENSE, INPUT);
	pinMode(M2_SENSE, INPUT);
	
	pinMode(LED, OUTPUT);	digitalWrite(LED, LOW);
	
	
	pinMode(ENC1A, INPUT);	digitalWrite(ENC1A, HIGH);
	pinMode(ENC1B, INPUT);	digitalWrite(ENC2A, HIGH);
	pinMode(ENC2A, INPUT);	digitalWrite(ENC1B, HIGH);
	pinMode(ENC2B, INPUT);	digitalWrite(ENC2B, HIGH);
	
	// Enable interrupts for encoders
	attachInterrupt(1, readEncoder1, RISING);
	attachInterrupt(0, readEncoder2, RISING);
	EICRA = ENCODER_RESOLUTION;	// Set interrupt for change or rising, depending on encoder reading resolution
	
	
	
#ifdef ENABLE_WATCHDOG_TIMER
	WDTCSR = _BV(WDCE) | _BV(WDE);
	WDTCSR = _BV(WDIE) | _BV(WDE) | LOW_WDP | HIGH_WDP;
#else 
	WDTCSR = _BV(WDCE);
	WDTCSR = 0;
#endif
	
	// Set up Timer2 to automatically update currents and encoders
	// Timer is set up to interrupt every 10ms by default in order to update motors and encoders when necessary
    TCCR2B = 0x00;			// Disbale Timer2 while we set it up
	TCNT2 = 0x00;			// Set initial timer value to 0
	OCR2A = OCR2A_VALUE;	// Set the value to count to for desired refresh rate
	OCR2B = OCR2A_VALUE / 2;// Set compare 2 to half compare 1 rate
	TCCR2A = TIMER2A_MODE;	// Timer2 Control Reg A: Set to mode 7, counts from 0x00 to OCR2A
	TCCR2B = TIMER2B_MODE | TIMER2_PRESCALER;// Timer2 Control Reg B: Timer Prescaler set to 1024 and timer mode set to mode 7

	// Setup the I2C bus
	Wire.begin(DEFAULT_DMD_ADDRESS);	// Begin I2C at slave address I2C_ADDRESS (defaults to 0x03)
#if I2C_FREQ == 100000
	TWBR = 72;							// Set up I2C for 100kHz. Forumla is: Bit Rate = 16MHz / (16 + 2 * TWBR)
#elif I2C_FREQ == 200000
	TWBR = 32;							// Set up I2C for 200kHz. Forumla is: Bit Rate = 16MHz / (16 + 2 * TWBR)
#else
	TWBR = 12;							// Set up I2C for 400kHz. Forumla is: Bit Rate = 16MHz / (16 + 2 * TWBR)
#endif
	digitalWrite(A4, LOW);				// Disable internal pull-ups
	digitalWrite(A5, LOW);				// Disable internal pull-ups
	
	// Setup automatic analog conversion for current sensing
	ADMUX = M1_SENSE_MASK;		// Setup ADC for Vcc reference and to read M1 sense pin (we'll change which pin it read in the ISR to alternate motors)
	ADCSRA = 0xAF;			// Enable ADC and ADC interrupt with 128 prescaler in free-running mode
	ADCSRB = 6;				// Set ADC to free running mode
	ADCSRA |= 0x40;			// Start the ADC conversions
	
	// Setup pin structs
	M1 = (Pin){(uint8_t*)&PORTD, (uint8_t*)&PIND, 1<<5, 5, 5, 0x02};
	M2 = (Pin){(uint8_t*)&PORTD, (uint8_t*)&PIND, 1<<6, 6, 6, 0x04};
	M1H = (Pin){(uint8_t*)&PORTD, (uint8_t*)&PIND, 1<<7, 7, 7, 0x00};
	M1L = (Pin){(uint8_t*)&PORTB, (uint8_t*)&PINB, 1<<0, 0, 8, 0x00};
	M2H = (Pin){(uint8_t*)&PORTB, (uint8_t*)&PINB, 1<<1, 9, 9, 0x00};
	M2L = (Pin){(uint8_t*)&PORTD, (uint8_t*)&PIND, 1<<4, 4, 4, 0x00};
	
	
	
	// Setup motor structs
	motor1.state.control = DEFAULT_CONTROL;
	motor1.state.set_power = 0;
	motor1.state.encoder_value = 0;
	motor1.state.current_rate = 0;
	motor1.state.current_power = 0;
	motor1.state.scaled_power = 0;
	motor1.state.target_rate = 0;
	motor1.state.PID_P = 0;
	motor1.state.PID_I = 0;
	motor1.state.PID_D = 0;
	motor1.old_enc = 0;
	motor1.OCR0x = (uint8_t*)&OCR0B;
	motor1.COM0x = 0x20;
	motor1.speed_pin = &M1;
	motor1.high_pin = &M1H;
	motor1.low_pin = &M1L;
	
	motor2.state.control = DEFAULT_CONTROL;
	motor2.state.set_power = 0;
	motor2.state.encoder_value = 0;
	motor2.state.current_rate = 0;
	motor2.state.current_power = 0;
	motor2.state.scaled_power = 0;
	motor2.state.target_rate = 0;
	motor2.state.PID_P = 0;
	motor2.state.PID_I = 0;
	motor2.state.PID_D = 0;
	motor2.old_enc = 0;
	motor2.OCR0x = (uint8_t*)&OCR0A;
	motor2.COM0x = 0x80;
	motor2.speed_pin = &M2;
	motor2.high_pin = &M2H;
	motor2.low_pin = &M2L;
	
	ramping_rate = 5;
	
	min_power = DEFAULT_MIN_POWER;
	PID_kP = DEFAULT_PID_KP;
	PID_kI = DEFAULT_PID_KI;
	PID_kD = DEFAULT_PID_KD;
	
	setTimerB(&motor1);
	setTimerB(&motor2);
	
	sei();			// Enable global interrupts
}

DualMotorDriver::~DualMotorDriver() 
{
	// Do nothing
}


// Called to initialize the motor driver ISR's
void DualMotorDriver::init()
{
	Wire.onReceive(receiveEvent);
	Wire.onRequest(requestEvent);
	
	// The below timer setups were placed here cause Arduino screws with them otherwise as they'd usually be used by other functions/libraries built into the Arduino code
	
	// Setup Timer0 for the PWM pins
	TCCR0B = 0;				// Disable the timer while we set it up
	TCCR0A = 0x03;			// Setup pins 5 and 6 for phase fast PWM
	TCNT0 = 0;
	TCCR0B = 0x01;			// Set PWM prescaler to 1 (lowest possible)
	
	// Set up Timer1 cause why not
	// 64 prescaler, 250 counts per millisecond, Fast PWM mode
	TCCR1B = 0;
	TCCR1A = 0x03;//_BV(WGM11) | _BV(WGM10);
	TCCR1C = 0;
	TCNT1 = 0;
	OCR1A = 250;
	TIMSK1 = 1;
	TCCR1B = 0x1B;//_BV(WGM13) | _BV(WGM12) | _BV(CS11) | _BV(CS10);
	
	if (MCUSR & 0x08) // Setup Timer1 to flash LED? Pin: PB2, using Timer1A OCR
	{
		TIMSK1 |= 0x04;
	}
	///else delete &reset_led_counter;
	
	loadSettings();
	
	updateMotorControl(&motor1);
	updateMotorControl(&motor2);
	
	digitalWrite(LED, LOW);
}


void DualMotorDriver::saveSettings(uint8_t vals)
{
	// If there are already saved values, add to them. Otherwise, write an initial value.
	uint8_t saved_vals = eeprom_read_byte((uint8_t*)SAVED_VALS_ADDRESS);
	if (!(saved_vals & 0x80)) vals |= saved_vals; 
	
	eeprom_write_byte((uint8_t*)SAVED_VALS_ADDRESS, vals);
	if (vals & 0x01) 
	{
		eeprom_write_byte((uint8_t*)M1_CONTROL_ADDRESS, motor1.state.control);
		eeprom_write_byte((uint8_t*)M2_CONTROL_ADDRESS, motor2.state.control);
	}
	if (vals & 0x02) eeprom_write_byte((uint8_t*)RAMPING_RATE_ADDRESS, ramping_rate);
	if (vals & 0x04) eeprom_write_byte((uint8_t*)MIN_POWER_ADDRESS, min_power);
	if (vals & 0x08) eeprom_write_byte((uint8_t*)I2C_ADDR_ADDRESS, TWAR);
	if (vals & 0x10) eeprom_write_block((void*)&PID_kP, (void*)PID_SCALARS_ADDRESS, 12);
}

void DualMotorDriver::loadSettings()
{
	uint8_t vals = eeprom_read_byte((uint8_t*)SAVED_VALS_ADDRESS);
	if (!(vals & 0x80))
	{
		if (vals & 0x01)
		{
			motor1.state.control = eeprom_read_byte((uint8_t*)M1_CONTROL_ADDRESS);
			motor2.state.control = eeprom_read_byte((uint8_t*)M2_CONTROL_ADDRESS);
		}
		if (vals & 0x02) ramping_rate = eeprom_read_byte((uint8_t*)RAMPING_RATE_ADDRESS);
		if (vals & 0x04) min_power = eeprom_read_byte((uint8_t*)MIN_POWER_ADDRESS);
		if (vals & 0x08) TWAR = eeprom_read_byte((uint8_t*)I2C_ADDR_ADDRESS);
		if (vals & 0x10) eeprom_read_block((void*)&PID_kP, (void*)PID_SCALARS_ADDRESS, 12);
	}
	
	if (blahblah > 20) 
	{
		updateMotorControl(&motor1);
		updateMotorControl(&motor2);
	}	
}


// Function to force reset the motor driver. Reset will activate ~16ms after funtion call
inline void DualMotorDriver::reset()
{
	TCCR1B &= ~0x07;				// Disable timer to reset watchdog timer
	WDTCSR = _BV(WDCE) | _BV(WDE);	// Setup wtachdog register for writing
	WDTCSR = _BV(WDE);				// Set watchdog timer to expire and reset in minimum time (16ms)
}


// Handles reading in I2C data
uint8_t DualMotorDriver::getData(int bytes)
{
	active_var = Wire.read();
	
	while (Wire.available())
	{
		switch (active_var)
		{
			case M1_CONTROL:	wireToVar(&motor1.state.control);  		updated_vars |= 1 << active_var;	break;
			case M2_CONTROL:	wireToVar(&motor2.state.control);	 	updated_vars |= 1 << active_var;	break;
			case M1_POWER: 		wireToVar(&motor1.state.set_power); 	updated_vars |= 1 << active_var;	break;
			case M2_POWER: 		wireToVar(&motor2.state.set_power);		updated_vars |= 1 << active_var;	break;
			case M1_ENCODER: 	wireToVar(&motor1.state.encoder_value); updated_vars |= 1 << active_var;	break;
			case M2_ENCODER: 	wireToVar(&motor2.state.encoder_value);	updated_vars |= 1 << active_var;	break;
			case M1_RATE: 		wireToVar(&motor1.state.target_rate);	updated_vars |= 1 << active_var;	break;
			case M2_RATE: 		wireToVar(&motor2.state.target_rate);	updated_vars |= 1 << active_var;	break;
			case PID_KP: 		wireToVar(&PID_kP);						updated_vars |= 1 << active_var;	break;
			case PID_KI: 		wireToVar(&PID_kI);						updated_vars |= 1 << active_var;	break;
			case PID_KD: 		wireToVar(&PID_kD);						updated_vars |= 1 << active_var;	break;
			case RAMPING_RATE: 	wireToVar(&ramping_rate);				break;
			case MIN_POWER: 	wireToVar(&min_power);					break;
			case DEVICE_ADDRESS: 	TWAR = Wire.read() << 1; 			saveSettings(0x08);					break;
			case EEPROM_SAVE: 	saveSettings(Wire.read());				break;
			case M1_STATE: 		wireToVar(&motor1.state);				break;// Needs to be updated to reflect in updated_vars
			case M2_STATE: 		wireToVar(&motor2.state);				break;// Needs to be updated to reflect in updated_vars
		}
		if (Wire.available()) active_var++;
	}
	
	switch (active_var)
	{
		case M1_CONTROL: 	active_var_ptr = &motor1.state.control;				break;
		case M2_CONTROL: 	active_var_ptr = &motor2.state.control;				break;
		case M1_POWER: 		active_var_ptr = &motor1.state.current_power;		break;
		case M2_POWER: 		active_var_ptr = &motor2.state.current_power;		break;
		case M1_ENCODER: 	active_var_ptr = &motor1.state.encoder_value;		break;
		case M2_ENCODER: 	active_var_ptr = &motor2.state.encoder_value;		break;
		case M1_RATE: 		active_var_ptr = &motor1.state.current_rate;		break;
		case M2_RATE: 		active_var_ptr = &motor2.state.current_rate;		break;
		case M1_CURRENT:	active_var_ptr = &motor1.state.current_draw;		break;
		case M2_CURRENT:	active_var_ptr = &motor1.state.current_draw;		break;
		case PID_KP: 		active_var_ptr = &PID_kP;							break;
		case PID_KI: 		active_var_ptr = &PID_kI;							break;
		case PID_KD: 		active_var_ptr = &PID_kD;							break;
		case RAMPING_RATE: 	active_var_ptr = &ramping_rate;						break;
		case MIN_POWER: 	active_var_ptr = &min_power;						break;
		case EEPROM_LOAD: 	loadSettings();										break;
		case RESET:			reset();											break;
		case M1_STATE: 		active_var_ptr = &motor1.state;						break;
		case M2_STATE: 		active_var_ptr = &motor2.state;						break;
	}
	updateVars();
}


void DualMotorDriver::sendData()
{
#ifdef DEBUG_MOTOR_DRIVER
	if (active_var > 100)
	{
		switch (active_var)
		{
			// Debug code for reading values not in the data register array
			/*
			case m1_scaled_power: Wire.write(motor1.scaled_pwr, 1); break;
			case m1_current_power: Wire.write(motor1.current_power, 1); break;
			case m1_power: Wire.write(motor1.power, 1); break;
			case m1_encoder: Wire.write((uint8_t*)motor1.enc_val, 4); break;
			case m1_rate: Wire.write((uint8_t*)motor1.cur_rate, 4); break;
			case m1_target_rate: Wire.write((uint8_t*)motor1.target_rate, 2); break;
			case ms: Wire.write((uint8_t*)MS, 4); break;
			case m1_p: Wire.write((uint8_t*)&motor1.PID_P, 4); break;
			case m1_i: Wire.write((uint8_t*)&motor1.PID_I, 4); break;
			case m1_d: Wire.write((uint8_t*)&motor1.PID_D, 4); break;
			
			case m2_scaled_power: Wire.write(motor2.scaled_pwr, 1); break;
			case m2_current_power: Wire.write(motor2.current_power, 1); break;
			case m2_power: Wire.write(motor2.power, 1); break;
			case m2_encoder: Wire.write((uint8_t*)motor2.enc_val, 4); break;
			case m2_rate: Wire.write((uint8_t*)motor2.cur_rate, 4); break;
			case m2_target_rate: Wire.write((uint8_t*)motor2.target_rate, 2); break;
			case m2_p: Wire.write((uint8_t*)&motor2.PID_P, 4); break;
			case m2_i: Wire.write((uint8_t*)&motor2.PID_I, 4); break;
			case m2_d: Wire.write((uint8_t*)&motor2.PID_D, 4); break;
			
			case pin1s: Wire.write((uint8_t*)&M1, sizeof(M1)); break;
			case pin2s: Wire.write((uint8_t*)&M1, sizeof(M2)); break;
			case pin1H: Wire.write((uint8_t*)&M1, sizeof(M1H)); break;
			case pin1L: Wire.write((uint8_t*)&M1, sizeof(M1L)); break;
			case pin2H: Wire.write((uint8_t*)&M1, sizeof(M2H)); break;
			case pin2L: Wire.write((uint8_t*)&M1, sizeof(M2L)); break;

			case mcusr: Wire.write(MCUSR); break;
			case admux: Wire.write(ADMUX); break;
			case adcsra: Wire.write(ADCSRA); break;
			*/
			case elapsed_millis: Wire.write((uint8_t*)&blahblah, 4); break;
			case 203: Wire.write((uint8_t*)&motor1.state, sizeof(motor1.state)); break;
			
			case led: ledToggle(); break;
		}
	}
	else
	{
		Wire.write((uint8_t*)active_var_ptr, TX_BUFFER_SIZE);// Holy s**t this worked first try! Pointers FTW!
	}
#else
	Wire.write((uint8_t*)active_var_ptr, TX_BUFFER_SIZE);// Holy s**t this worked first try! Pointers FTW!
#endif
}


// Updates everything based on updated_vars
void DualMotorDriver::updateVars()
{
	if (updated_vars & (1 << M1_CONTROL))	updateMotorControl(&motor1);
	if (updated_vars & (1 << M2_CONTROL))	updateMotorControl(&motor2);
	if (updated_vars & (1 << M1_POWER)) 	updateMotor(&motor1);
	if (updated_vars & (1 << M2_POWER)) 	updateMotor(&motor2);
	
	updated_vars = 0;
}


inline void DualMotorDriver::updateMotorControl(Motor *motor)
{
	if (!(motor->state.control & SPEED)) setMotorDirection(motor);
	if ((motor->state.control & 0x0C) != 12)// If not in RPM mode, reset the PID values and target rate setting
	{
		motor->state.PID_P = 0;
		motor->state.PID_I = 0;
		motor->state.PID_D = 0;
		motor->state.target_rate = 0;
		updateMotor(motor);
	}
	else // If in RPM mode, disable braking and enable the encoder
	{
		motor->state.control &= ~BRAKE;
		motor->state.control |= ENCODER;
		setMotorDirection(motor);
	}
	
	if (motor->state.control & ENCODER) EIMSK |= 1 << (motor->speed_pin->number % 2);// 1 for M1, 0 for M2
	else EIMSK &= ~(1 << (motor->speed_pin->number % 2));// 1 for M1, 0 for M2
	
	setTimerB(motor);
}


void DualMotorDriver::updateMotor(Motor *motor)
{
	if (motor->state.control & ENCODER)
	{
		updateEncoder(motor);
	}
	
	if (motor->state.control & SPEED) 
	{
		if (motor->state.control & MODE) { updateMotorRPM(motor);}
		else 
		{
			motor->state.set_power ? motor->state.scaled_power = map(motor->state.set_power, 0, 255, min_power, 255) : motor->state.scaled_power = 0;
			updateMotorPower(motor);
		}
	}
	else if (motor->state.control & MODE)
	{
		motor->state.set_power ? motor->state.current_power = map(motor->state.set_power, 0, 255, min_power, 255) : motor->state.current_power= 0;
		setMotorPWM(motor);
	}
	else 
	{
		motor->state.current_power = motor->state.set_power;
		setMotorPWM(motor);
	}
}


inline void DualMotorDriver::updateMotorRPM(Motor *motor)
{
	float old_rate = motor->state.current_rate;
	
	motor->state.PID_P = PID_kP * motor->state.target_rate;
	motor->state.PID_I += PID_kI * (motor->state.target_rate - motor->state.current_rate);
	motor->state.PID_D = PID_kD * (old_rate - motor->state.current_rate);
	
	float PID_power = motor->state.PID_P + motor->state.PID_I + motor->state.PID_D;
	
	motor->state.current_power = constrain(abs(PID_power), min_power, 255);

	if (PID_power < 0 && !(motor->state.control & DIRECTION))
	{
		motor->state.control |= DIRECTION;
		setMotorDirection(motor);
	}
	else if (PID_power > 0 && motor->state.control & DIRECTION)
	{
		motor->state.control &= ~DIRECTION;
		setMotorDirection(motor);
	}
	setMotorPWM(motor);
}


inline void DualMotorDriver::updateMotorPower(Motor *motor)
{
	if (motor->state.set_power) {
		if (!(digitalRead(motor->high_pin->number) ^ (motor->state.control & DIRECTION)))// Check to see if motor is turning the same direction as indicated by motor control register.
		{
			if (motor->state.current_power < motor->state.scaled_power) motor->state.current_power = constrain(motor->state.current_power + ramping_rate, min_power, motor->state.scaled_power);
			else if (motor->state.current_power > motor->state.scaled_power) motor->state.current_power = constrain(motor->state.current_power - ramping_rate, motor->state.scaled_power, 255);
		}
		else // If motor direction and control direction differ, slow down to minimum power then change direction and continue as normal
		{
			motor->state.current_power = constrain(motor->state.current_power - ramping_rate, min_power, 255);
			if (motor->state.current_power == min_power) setMotorDirection(motor);
		}
	}
	else
	{
		if (motor->state.current_power != 0) motor->state.current_power = constrain(motor->state.current_power - ramping_rate, 0, 255);
	}
	setMotorPWM(motor);
}


inline void DualMotorDriver::updateEncoder(Motor *motor)
{	
	motor->state.current_rate = ((float)(motor->state.encoder_value - motor->old_enc) / TICKS_PER_ROT) * REFRESH_FREQ * 60;
	motor->old_enc = motor->state.encoder_value;
}


// More efficient analogWrite for motor control pins
inline void DualMotorDriver::setMotorPWM(Motor *motor)
{
	switch (motor->state.current_power)
	{
		case 0: 	TCCR0A &= ~motor->COM0x; setMotorBraking(motor);						break;
		case 255:	TCCR0A &= ~motor->COM0x; sbi(PORTD, motor->speed_pin->number);			break;
		default: 	TCCR0A |= motor->COM0x;  *motor->OCR0x = motor->state.current_power;	break;
	}
}

// More efficent digitalWrite for just the motor direction/braking pins
inline void DualMotorDriver::setMotorDirection(Motor *motor)
{
	//if (!(motor->state.control & BRAKE))
	//{
	if (motor->state.control & DIRECTION)
	{
		*motor->high_pin->out_port |= motor->high_pin->bit_mask;
		*motor->low_pin->out_port &= ~motor->low_pin->bit_mask;
	}
	else
	{
		*motor->high_pin->out_port &= ~motor->high_pin->bit_mask;
		*motor->low_pin->out_port |= motor->low_pin->bit_mask;
	}
	//}
}

// More efficent digitalWrite for just the motor direction/braking pins
inline void DualMotorDriver::setMotorBraking(Motor *motor)
{
	if (motor->state.control & BRAKE && ~motor->state.set_power)
	{
		*motor->speed_pin->out_port |= motor->speed_pin->bit_mask;
		*motor->high_pin->out_port |= motor->high_pin->bit_mask;
		*motor->low_pin->out_port |= motor->low_pin->bit_mask;
	}
	else
	{
		*motor->speed_pin->out_port &= ~motor->speed_pin->bit_mask;
		setMotorDirection(motor);
	}
}

// Enables and disables the interrupts that control motor and encoder updates
inline void DualMotorDriver::setTimerB(Motor *motor)
{
	if (motor->state.control & (SPEED | ENCODER))// Enable the interrupt routine that updates the given motor
	{
		TCCR2B = 0x00;			// Disbale Timer2 while we set it up
		TIMSK2 |= motor->speed_pin->TIMSK2_mask;
		TCCR2B = TIMER2B_MODE | TIMER2_PRESCALER;// Re-enable Timer2
	}
	else // Disable the interrupts that control motor updates if they are unnecessary
	{
		TCCR2B = 0x00;			// Disbale Timer2 while we set it up
		TIMSK2 &= ~motor->speed_pin->TIMSK2_mask;
		TCCR2B = TIMER2B_MODE | TIMER2_PRESCALER;// Re-enable Timer2
	}
}


// Takes a pointer to a variable and reads a new value into it from I2C
void DualMotorDriver::wireToVar(uint8_t *var)
{
	*var = Wire.read();
}

// Takes a pointer to a variable and reads a new value into it from I2C
void DualMotorDriver::wireToVar(int16_t *var)
{
	*var = ((int16_t)Wire.read() << 8) | Wire.read();
}

void DualMotorDriver::wireToVar(uint16_t *var)
{
	*var = ((int16_t)Wire.read() << 8) | Wire.read();
}

// Takes a pointer to a variable and reads a new value into it from I2C
void DualMotorDriver::wireToVar(int32_t *var)
{
	*var = ((int32_t)Wire.read() << 24) | ((int32_t)Wire.read() << 16) | (Wire.read() << 8) | Wire.read();
}

// Takes a pointer to a variable and reads a new value into it from I2C
void DualMotorDriver::wireToVar(float *var)
{
	data_union.bytes[0] = Wire.read();
	data_union.bytes[1] = Wire.read();
	data_union.bytes[2] = Wire.read();
	data_union.bytes[3] = Wire.read();
	//*var = (Wire.read() << 24) | (Wire.read() << 16) | (Wire.read() << 8) | Wire.read();
	*var = data_union.float32;
}

// Takes a pointer to a motor state variable and reads a new set of values into it from I2C
void DualMotorDriver::wireToVar(MotorState *var)
{
	/*
	uint8_t *ptr = (uint8_t*) var;
	for (int i = 0; i < sizeof(MotorState); i++)
	{
		*ptr = Wire.read();
		ptr++;
	}
	*/
}
// ========== End of class ==========

// I2C receive event
void receiveEvent(int bytes) {
	MotorDriver.getData(bytes);
}

// I2C request event
void requestEvent() {
	MotorDriver.sendData();
}

// Interrupt Service Routine attached to INT1 vector
void readEncoder1()
{
	PIND |= 0x01;
#if ENCODER_RESOLUTION == SINGLE
	PINC & 0x01 ? MotorDriver.motor1.state.encoder_value++ : MotorDriver.motor1.state.encoder_value--;
#elif ENCODER_RESOLUTION == DOUBLE
	if (EICRA & 0x04) PINC & 0x01 ? MotorDriver.motor1.state.encoder_value++ : MotorDriver.motor1.state.encoder_value--;
	else PINC & 0x01 ? MotorDriver.motor1.state.encoder_value-- : MotorDriver.motor1.state.encoder_value++;
	EICRA ^= 0x04;
#endif
}

// Interrupt service routine attached to INT0 vector
void readEncoder2()
{
#if ENCODER_RESOLUTION == SINGLE
	PINC & 0x02 ? MotorDriver.motor2.state.encoder_value++ : MotorDriver.motor2.state.encoder_value--;
#elif ENCODER_RESOLUTION == DOUBLE
	if (EICRA & 0x01) PINC & 0x02 ? MotorDriver.motor2.state.encoder_value++ : MotorDriver.motor2.state.encoder_value--;
	else PINC & 0x02 ? MotorDriver.motor2.state.encoder_value-- : MotorDriver.motor2.state.encoder_value++;
	EICRA ^= 0x01;
#endif
}

volatile unsigned long blahblah;

void delayMillis(unsigned long time)
{
	short i;
	uint32_t start = blahblah;
	while (blahblah - start < time)
	{
		i++;
	}
	return;
}

ISR(WDT_vect)
{
	sbi(PORTB, 4);
}

ISR(TIMER1_OVF_vect)
{
	blahblah++;
	__asm__ __volatile__ ("wdr");
}

ISR(TIMER1_COMPB_vect)
{
	MotorDriver.reset_led_counter++;
	if (MotorDriver.reset_led_counter > 500) 
	{
		ledToggle();
		MotorDriver.reset_led_counter = 0;
	}
}

ISR(TIMER2_COMPA_vect, ISR_NOBLOCK)
{
	// Motor 1 updates go here
	MotorDriver.updateMotor(&MotorDriver.motor1);
}

ISR(TIMER2_COMPB_vect, ISR_NOBLOCK)
{
	// Motor 2 updates go here
	MotorDriver.updateMotor(&MotorDriver.motor2);
}

ISR(ADC_vect, ISR_NOBLOCK)
{
	ADMUX ^= 0x01;	// Toggles between motor sense pins
	
	if (ADMUX & 0x01)
	{
		MotorDriver.motor1.state.control |= 0x40;					// Set new current data ready bit
		MotorDriver.motor1.state.current_draw = ADCL | (ADCH << 8);	// Read bottom 8 bits of current sense into motor current variable
	}
	else
	{
		MotorDriver.motor2.state.control |= 0x40;					// Set new current data ready bit
		MotorDriver.motor2.state.current_draw = ADCL | (ADCH << 8);	// Read bottom 8 bits of current sense into motor current variable
	}
}



#ifdef DEBUG_MOTOR_DRIVER
void ledOn() {	sbi(PORTB, 2);}
void ledOff() {	cbi(PORTB, 2);}
void ledToggle() {	PINB |= 0x04;}
#endif
#endif