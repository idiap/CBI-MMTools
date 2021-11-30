/**
* Copyright (c) 2021 Idiap Research Institute, http://www.idiap.ch/
* Written by François Marelli <francois.marelli@idiap.ch>
* 
* This file is part of CBI-MMTools.
* 
* CBI-MMTools is free software: you can redistribute it and/or modify
* it under the terms of the 3-Clause BSD License.
* 
* CBI-MMTools is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* 3-Clause BSD License for more details.
* 
* You should have received a copy of the 3-Clause BSD License along
* with CBI-MMTools. If not, see https://opensource.org/licenses/BSD-3-Clause.
* 
* SPDX-License-Identifier: BSD-3-Clause 
*
* This code is designed for an Arduino Nano with Atmega328
* It will not run on Nano Every!
*/

#include <Arduino.h>
#define BAUDRATE 9600

// Communication variables
constexpr auto RCV_LENGTH = 16;
constexpr auto MOD_LENGTH = 250;
char rcv_buffer[RCV_LENGTH];
size_t read_bytes;
constexpr char FIRMWARE_ID[] = "MM-AC";
constexpr unsigned int VERSION = 1;

constexpr unsigned char ASCII_START = 1;
constexpr unsigned char ASCII_END = 4;
constexpr unsigned char ASCII_ACK = 6;
constexpr unsigned char ASCII_NACK = 21;

constexpr unsigned char HEADER_ANAMOD = 'A';
constexpr unsigned char HEADER_ACQUIRE = 'B';
constexpr unsigned char HEADER_DIGMOD = 'D';
constexpr unsigned char HEADER_EXPOSURE = 'E';
constexpr unsigned char HEADER_FIRMWARE = 'F';
constexpr unsigned char HEADER_GATE = 'G';
constexpr unsigned char HEADER_ENABLE = 'H';
constexpr unsigned char HEADER_AMPLITUDE = 'I';
constexpr unsigned char HEADER_LOOPF = 'L';
constexpr unsigned char HEADER_NFRAMES = 'M';
constexpr unsigned char HEADER_NSTEPS = 'N';
constexpr unsigned char HEADER_MODA = 'O';
constexpr unsigned char HEADER_MODD = 'P';
constexpr unsigned char HEADER_RESET = 'R';
constexpr unsigned char HEADER_TRIGGER = 'S';
constexpr unsigned char HEADER_DELTAT = 'T';
constexpr unsigned char HEADER_VERSION = 'V';
constexpr unsigned char HEADER_WAITBEFORE = 'W';
constexpr unsigned char HEADER_WAITAFTER = 'X';

// Enable pin (inverted logic)
constexpr unsigned int PIN_EN = 13;

// Digital trigger pins
constexpr unsigned int PIN_T_MASTER = 12;
constexpr unsigned int PIN_T_C = 8;
constexpr unsigned int PIN_T_O1 = 5;
constexpr unsigned int PIN_T_O2 = 7;
constexpr unsigned int PIN_T_P1 = 6;
constexpr unsigned int PIN_T_P2 = 4;

// Multiplexer channel select
constexpr unsigned int PIN_C = 2;
constexpr unsigned int PIN_B = A4;
constexpr unsigned int PIN_A = A3;

// Trigger in
constexpr unsigned int PIN_IN = A5;

// Analog pins
constexpr unsigned int PIN_M_P1 = 10;
constexpr unsigned int PIN_M_P2 = 11;
constexpr unsigned int PIN_M_O1 = 3;
constexpr unsigned int PIN_M_O2 = 9;

// Edge detection
bool old_trigger;
bool new_trigger;
bool rising_edge;

// Control booleans
bool is_digital_mod;
bool is_analog_mod;
bool is_modulation;
bool loop_over_frames;

// this is loop_over_frames OR manual long exposure (loop is mandatory then)
bool looping_master;

// Modulation tables
byte digital_table[MOD_LENGTH];
byte analog_table[4][MOD_LENGTH];

// Preloaded values for next step
byte next_P1;
byte next_P2;
byte next_O1;
byte next_O2;
byte next_OR;
byte next_AND;

// What outputs are on
byte digital_shutters;
// Non modulated amplitudes
// Channels:
//  0: P2
//  1: O1
//  2: P1
//  3: O2
byte analog_A[4];

// What trigger is selected
byte trigger_channel;

// Number of frames and steps per frame
byte n_steps;
byte n_frames;

// Current frame and step
byte current_frame;
byte current_step;
byte last_step;

// Modulation step time, microseconds
unsigned long step_length_us;

// Timestamps for acquisitions
unsigned long next_frame_time;
unsigned long prev_frame_time;
unsigned long next_acq_time;
unsigned long prev_acq_time;
unsigned long temp_time;

// Software-triggered acquisition parameters
// N frames to acquire, infinite if negative
// Set exposure to 0 for manual exposure
int acq_n_frames;
unsigned long exposure_us;

// Wait time between acquired manual frames
unsigned long wait_before_us;
unsigned long wait_after_us;

int select_channel(byte channel)
{
  /* Select which channel is forwarded by the multiplexer 
      0: Aux. IN
      1: Camera FIRE 1
      2: Camera FIRE N
      3: Camera Aux. Out. 1
      4: Internal trigger
  */
  int error = 0;

  // Save the enable state
  bool enable = digitalRead(PIN_EN);

  // Disable multiplexer before switching
  digitalWrite(PIN_EN, HIGH);

  if (channel < 5)
  {
    digitalWrite(PIN_C, (channel >> 2) & 1);
    digitalWrite(PIN_B, (channel >> 1) & 1);
    digitalWrite(PIN_A, channel & 1);
    trigger_channel = channel;

    // Turn on camera if internal trigger
    digitalWrite(PIN_T_C, channel != 4);
  }
  else
  {
    // This channel does not exist
    error = 1;
  }

  // Re-enable the multiplexer if needed
  digitalWrite(PIN_EN, enable);

  return error;
}

void reset()
{
  // Disable multiplexer
  digitalWrite(PIN_EN, HIGH);

  // Set all PWM outputs to 0
  OCR1B = 0;
  OCR2A = 0;
  OCR2B = 0;
  OCR1A = 0;

  digitalWrite(PIN_T_MASTER, 0);

  // Trigger controls are inverted logic!
  digitalWrite(PIN_T_C, 1);
  digitalWrite(PIN_T_O1, 1);
  digitalWrite(PIN_T_O2, 1);
  digitalWrite(PIN_T_P1, 1);
  digitalWrite(PIN_T_P2, 1);

  // Default to internal trigger channel
  select_channel(4);

  // This prevents immediately detecting rising edge
  old_trigger = 1;

  // Set variables default values
  is_digital_mod = false;
  is_analog_mod = false;
  is_modulation = false;
  loop_over_frames = false;
  looping_master = false;

  // Default values: do not affect anything
  next_OR = B00000000;
  next_AND = B11111111;

  // Default mask: all HIGH (disabled)
  digital_shutters = B11111111;

  // Default amplitudes: all 0
  for (int i = 0; i < 4; i++)
  {
    analog_A[i] = 0;
  }

  // Default times
  exposure_us = 10000;
  step_length_us = 0;

  // Reset sequence
  n_steps = 0;
  n_frames = 0;

  reset_tables();

  // Default wait time: 10ms (for Andor camera)
  // After and before to simulate rolling shutter
  wait_before_us = 10000;
  wait_after_us = 10000;
  acq_n_frames = 0;
}

void setup()
{
  digitalWrite(PIN_EN, HIGH);
  pinMode(PIN_EN, OUTPUT);

  pinMode(PIN_T_MASTER, OUTPUT);
  pinMode(PIN_T_C, OUTPUT);
  pinMode(PIN_T_O2, OUTPUT);
  pinMode(PIN_T_P1, OUTPUT);
  pinMode(PIN_T_O1, OUTPUT);
  pinMode(PIN_T_P2, OUTPUT);
  pinMode(PIN_C, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  pinMode(PIN_A, OUTPUT);

  pinMode(PIN_M_P2, OUTPUT);
  pinMode(PIN_M_P1, OUTPUT);
  pinMode(PIN_M_O2, OUTPUT);
  pinMode(PIN_M_O1, OUTPUT);

  pinMode(PIN_IN, INPUT);

  // Set PWM frequency to 62.5 kHz (timer divider of 1, fast PWM)
  // This does not affect pins 5/6, as doing so messes up time functions
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM10);
  TCCR1B = _BV(CS10) | _BV(WGM12);

  TCCR2A = _BV(COM2A1) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS20);

  reset();

  Serial.setTimeout(500);
  Serial.begin(BAUDRATE);
}

inline void wait_till(unsigned long target_time, unsigned long current_time) __attribute__((always_inline));
void wait_till(unsigned long target_time, unsigned long current_time)
{

  if (target_time < current_time)
  {
    while (current_time < micros())
    {
      // Detect target_time overflow
    }
  }

  // Wait till specified time
  while (micros() < target_time)
  {
    // Or till micros overflow
    if (micros() < current_time)
    {
      break;
    }
  }
}

inline void load_mod_values() __attribute__((always_inline));
void load_mod_values()
{
  // Load next step values
  if (is_digital_mod)
  {
    next_AND = digital_table[current_step] | digital_shutters;
  }
  else
  {
    next_AND = digital_shutters;
  }
  next_OR = next_AND & B11110000;

  if (is_analog_mod)
  {
    next_P1 = analog_table[2][current_step];
    next_P2 = analog_table[0][current_step];
    next_O1 = analog_table[1][current_step];
    next_O2 = analog_table[3][current_step];
  }
  else
  {
    next_P1 = analog_A[2];
    next_P2 = analog_A[0];
    next_O1 = analog_A[1];
    next_O2 = analog_A[3];
  }
}

void reset_tables()
{
  for (int i = 0; i < MOD_LENGTH; i++)
  {
    digital_table[i] = B00001111;
    for (int j = 0; j < 4; j++)
    {
      analog_table[j][i] = 255;
    }
  }
  is_digital_mod = false;
  is_analog_mod = false;
  current_frame = 0;
  current_step = 0;
}

void purge_serial()
{
  do
  {
    read_bytes = Serial.readBytesUntil(ASCII_END, rcv_buffer, RCV_LENGTH);
  } while (read_bytes == RCV_LENGTH);
  Serial.write(ASCII_NACK);
}

bool conclude_serial()
{
  read_bytes = Serial.readBytes(rcv_buffer, 1);
  if (read_bytes != 1 || rcv_buffer[0] != ASCII_END)
  {
    purge_serial();
    return false;
  }
  return true;
}

bool conclude_serial_variable(unsigned long &value, size_t valSize)
{
  read_bytes = Serial.readBytes(rcv_buffer, valSize + 1);
  if (read_bytes != valSize + 1 || rcv_buffer[valSize] != ASCII_END)
  {
    purge_serial();
    return false;
  }

  rcv_buffer[valSize] = 0;
  char *end_ptr;
  unsigned long temp_value;
  temp_value = strtoul((const char *)rcv_buffer, &end_ptr, 16);
  if (end_ptr != rcv_buffer + valSize)
  {
    Serial.write(ASCII_NACK);
    return false;
  }
  value = temp_value;
  return true;
}

bool conclude_serial_long(unsigned long &value)
{
  unsigned long temp;
  bool ret = conclude_serial_variable(temp, 8);
  if (ret)
  {
    value = temp;
  }
  return ret;
}

bool conclude_serial_int(int &value)
{
  unsigned long temp;
  bool ret = conclude_serial_variable(temp, 4);
  if (ret)
  {
    value = temp;
    if (value == 65535)
    {
      value = -1;
    }
  }
  return ret;
}

bool conclude_serial_byte(byte &value)
{
  unsigned long temp;
  bool ret = conclude_serial_variable(temp, 2);
  if (ret)
  {
    value = temp;
  }
  return ret;
}

bool conclude_serial_bool(bool &value)
{
  unsigned long temp;
  bool ret = conclude_serial_variable(temp, 1);
  if (ret)
  {
    value = temp;
  }
  return ret;
}

void loop()
{
  // Define our own loop to avoid function calls to loop(), few cycles gained
  while (1)
  {
    ///////////////////////////////////////////////////////////////////////////
    // Check for messages
    if (Serial.available())
    {
      read_bytes = Serial.readBytes(rcv_buffer, 2);

      if (read_bytes == 2 && rcv_buffer[0] == ASCII_START)
      {
        switch (rcv_buffer[1])
        {
        case HEADER_FIRMWARE:
        {
          if (conclude_serial())
          {
            Serial.write(ASCII_ACK);
            Serial.println(FIRMWARE_ID);
          }
          break;
        }

        case HEADER_VERSION:
        {
          if (conclude_serial())
          {
            Serial.write(ASCII_ACK);
            Serial.println(VERSION);
          }
          break;
        }

        case HEADER_RESET:
        {
          if (conclude_serial())
          {
            Serial.write(ASCII_ACK);
            reset();
          }
          break;
        }

        case HEADER_EXPOSURE:
        {
          if (conclude_serial_long(exposure_us))
          {
            Serial.write(ASCII_ACK);
          }
          break;
        }

        case HEADER_DELTAT:
        {
          if (conclude_serial_long(step_length_us))
          {
            Serial.write(ASCII_ACK);
          }
          break;
        }

        case HEADER_WAITBEFORE:
        {
          if (conclude_serial_long(wait_before_us))
          {
            Serial.write(ASCII_ACK);
          }
          break;
        }

        case HEADER_WAITAFTER:
        {
          if (conclude_serial_long(wait_after_us))
          {
            Serial.write(ASCII_ACK);
          }
          break;
        }

        case HEADER_ANAMOD:
        {
          if (conclude_serial_bool(is_analog_mod))
          {
            if (is_analog_mod && (n_frames * n_steps == 0 || step_length_us == 0))
            {
              is_analog_mod = false;
              Serial.write(ASCII_NACK);
            }
            else
            {
              Serial.write(ASCII_ACK);
            }
          }
          break;
        }

        case HEADER_DIGMOD:
        {
          if (conclude_serial_bool(is_digital_mod))
          {
            if (is_digital_mod && (n_frames * n_steps == 0 || step_length_us == 0))
            {
              is_digital_mod = false;
              Serial.write(ASCII_NACK);
            }
            else
            {
              Serial.write(ASCII_ACK);
            }
          }
          break;
        }

        case HEADER_LOOPF:
        {
          if (conclude_serial_bool(loop_over_frames))
          {
            Serial.write(ASCII_ACK);
          }
          break;
        }

        case HEADER_ENABLE:
        {
          bool enable;
          if (conclude_serial_bool(enable))
          {
            Serial.write(ASCII_ACK);
            digitalWrite(PIN_EN, !enable);
          }
          break;
        }

        case HEADER_TRIGGER:
        {
          byte channel;
          if (conclude_serial_byte(channel))
          {
            if (select_channel(channel))
            {
              Serial.write(ASCII_NACK);
            }
            else
            {
              Serial.write(ASCII_ACK);
            }
          }
          break;
        }

        case HEADER_NFRAMES:
        {
          if (conclude_serial_byte(n_frames))
          {
            Serial.write(ASCII_ACK);
            reset_tables();
          }
          break;
        }

        case HEADER_NSTEPS:
        {
          if (conclude_serial_byte(n_steps))
          {
            Serial.write(ASCII_ACK);
            reset_tables();
          }
          break;
        }

        case HEADER_ACQUIRE:
        {
          if (conclude_serial_int(acq_n_frames))
          {
            if (acq_n_frames && trigger_channel != 4)
            {
              Serial.write(ASCII_NACK);
              acq_n_frames = 0;
            }
            else
            {
              Serial.write(ASCII_ACK);
            }
          }
          break;
        }

        case HEADER_GATE:
        {
          read_bytes = Serial.readBytes(rcv_buffer, 1);
          if (read_bytes != 1)
          {
            purge_serial();
            break;
          }
          int o_channel = rcv_buffer[0] - '0';
          if (o_channel < 0 || o_channel >= 4)
          {
            purge_serial();
            break;
          }

          bool gate_val;
          if (conclude_serial_bool(gate_val))
          {
            Serial.write(ASCII_ACK);

            byte mask = bit(o_channel + 4);

            // Inverted logic
            if (gate_val)
            {
              digital_shutters &= ~mask;
            }
            else
            {
              digital_shutters |= mask;
            }
          }

          break;
        }

        case HEADER_AMPLITUDE:
        {
          read_bytes = Serial.readBytes(rcv_buffer, 1);
          if (read_bytes != 1)
          {
            purge_serial();
            break;
          }
          int o_channel = rcv_buffer[0] - '0';
          if (o_channel < 0 || o_channel >= 4)
          {
            purge_serial();
            break;
          }

          byte amp_val;
          if (conclude_serial_byte(amp_val))
          {
            Serial.write(ASCII_ACK);

            analog_A[o_channel] = amp_val;
          }

          break;
        }

        case HEADER_MODA:
        {
          read_bytes = Serial.readBytes(rcv_buffer, 1);
          if (read_bytes != 1)
          {
            purge_serial();
            break;
          }

          int o_channel = rcv_buffer[0] - '0';
          if (o_channel < 0 || o_channel >= 4)
          {
            purge_serial();
            break;
          }

          bool error = false;

          for (int i = 0; i < n_frames * n_steps; i++)
          {
            read_bytes = Serial.readBytes(rcv_buffer, 2);
            if (read_bytes != 2)
            {
              error = true;
              break;
            }

            rcv_buffer[2] = 0;
            char *end_ptr;
            unsigned long temp_value;
            temp_value = strtoul((const char *)rcv_buffer, &end_ptr, 16);
            if (end_ptr != rcv_buffer + 2)
            {
              error = true;
              break;
            }

            analog_table[o_channel][i] = (byte)temp_value;
          }

          if (error)
          {
            purge_serial();
          }

          if (!conclude_serial() || error)
          {
            for (int i = 0; i < MOD_LENGTH; i++)
            {
              analog_table[o_channel][i] = 255;
            }
            break;
          }

          current_frame = 0;
          current_step = 0;

          Serial.write(ASCII_ACK);

          break;
        }

        case HEADER_MODD:
        {
          read_bytes = Serial.readBytes(rcv_buffer, 1);
          if (read_bytes != 1)
          {
            purge_serial();
            break;
          }
          int o_channel = rcv_buffer[0] - '0';
          if (o_channel < 0 || o_channel >= 4)
          {
            purge_serial();
            break;
          }

          byte mask = bit(o_channel + 4);

          bool error = false;
          int bit_value = -1;
          for (int i = 0; i < n_frames * n_steps; i++)
          {
            read_bytes = Serial.readBytes(rcv_buffer, 1);
            if (read_bytes != 1)
            {
              error = true;
              break;
            }

            bit_value = rcv_buffer[0] - '0';

            if (bit_value < 0 || bit_value > 1)
            {
              error = true;
              break;
            }

            // Inverted logic
            if (bit_value)
            {
              digital_table[i] &= ~mask;
            }
            else
            {
              digital_table[i] |= mask;
            }
          }

          if (error)
          {
            purge_serial();
          }

          if (!conclude_serial() || error)
          {
            for (int i = 0; i < MOD_LENGTH; i++)
            {
              digital_table[i] &= ~mask;
            }
            break;
          }

          current_frame = 0;
          current_step = 0;

          Serial.write(ASCII_ACK);

          break;
        }

        default:
        {
          purge_serial();
          break;
        }
        }
      }
      else
      {
        // Wrong message header, send NACK and clear buffer till end of message
        purge_serial();
      }

      // Update modulation control
      is_modulation = is_digital_mod || is_analog_mod;

      looping_master = loop_over_frames || (exposure_us == 0);

      // Load and pre-apply next values
      load_mod_values();

      OCR1B = next_P1;
      OCR2A = next_P2;
      OCR2B = next_O1;
      OCR1A = next_O2;

      if (trigger_channel != 4 && !is_modulation)
      {
        // Turn on the required triggers
        PORTD &= digital_shutters;
      }
      else
      {
        // Turn off all triggers (inverted logic)
        PORTD |= B11110000;
      }

      // Force waiting for trigger to 0 for rising edge
      old_trigger = 1;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Update state

    // Internal trigger generation
    if (acq_n_frames)
    {
      // Turn on the master trigger
      PORTB |= B00010000;
      prev_acq_time = micros();

      old_trigger = 1;
      rising_edge = true;
      next_acq_time = prev_acq_time + wait_before_us;

      wait_till(next_acq_time, prev_acq_time);
    }

    // External trigger detection
    else
    {
      // Detect trigger rising edge
      new_trigger = PINC & B00010000;
      rising_edge = !old_trigger && new_trigger;
      old_trigger = new_trigger;
    }

    if (rising_edge)
    {
      // Start illumination sequence on edge
      if (is_modulation)
      {
        // Turn on the required triggers, other values are pre-applied
        PORTD &= next_AND;

        next_frame_time = micros();

        // Current step index
        current_step = current_frame * n_steps;
        last_step = current_step + n_steps;

        // Step 0 is done, iterate over the next ones
        current_step++;

        // Compute end of exposure time in manual mode
        if (acq_n_frames && exposure_us)
        {
          prev_acq_time = next_frame_time;
          next_acq_time = prev_acq_time + exposure_us;
        }

        // Loop over all the steps
        while (current_step < last_step || looping_master)
        {
          // Loop over frame if needed
          if (current_step >= last_step)
          {
            current_step -= n_steps;
          }

          // Load next step values
          load_mod_values();

          // Update time
          prev_frame_time = next_frame_time;
          next_frame_time += step_length_us;

          wait_till(next_frame_time, prev_frame_time);

          // Apply values: turn off with OR
          PORTD |= next_OR;

          // PWM registers
          // OCR1B => pin 10;
          // OCR2A => pin 11;
          // OCR2B => pin 3;
          // OCR1A => pin 9;

          OCR1B = next_P1;
          OCR2A = next_P2;
          OCR2B = next_O1;
          OCR1A = next_O2;

          // Turn on with AND
          PORTD &= next_AND;

          // Manual trigger timer
          if (acq_n_frames)
          {
            if (exposure_us)
            {
              // cond 1: no overflow
              // cond 2: both overflow
              // cond 3: only micros overflow
              if ((next_frame_time > next_acq_time && next_acq_time > prev_acq_time) ||
                  (next_frame_time > next_acq_time && next_frame_time < prev_acq_time) ||
                  (next_frame_time < prev_acq_time && prev_acq_time < next_acq_time))
              {
                old_trigger = 0;
              }
            }
            else if (Serial.available())
            {
              read_bytes = Serial.readBytes(rcv_buffer, 2);

              if (read_bytes == 2 && rcv_buffer[0] == ASCII_START && rcv_buffer[1] == HEADER_ACQUIRE && conclude_serial_int(acq_n_frames))
              {
                Serial.write(ASCII_ACK);
                acq_n_frames++;
                old_trigger = 0;
              }
              else if (read_bytes == 2 && rcv_buffer[0] == ASCII_START && rcv_buffer[1] == HEADER_RESET && conclude_serial())
              {
                Serial.write(ASCII_ACK);
                reset();
                break;
              }
              else
              {
                purge_serial();
              }
            }
          }
          else
          {
            // Check for trigger stop
            old_trigger = PINC & B00010000;
          }

          // Stop modulation if trigger is 0
          if (!old_trigger)
          {
            // Turn off all triggers (inverted logic)
            PORTD |= B11110000;
            // Including the master (normal logic)
            PORTB &= B11101111;

            // Register end time
            next_frame_time = micros();
            break;
          }

          // Increase step counter
          current_step++;
        }

        // END OF FRAME

        // Step to next frame
        current_frame = (current_frame + 1) % n_frames;
        current_step = current_frame * n_steps;

        // Load next values
        load_mod_values();

        // If trigger is off, sequence is already stopped
        if (old_trigger)
        {
          // Update time
          prev_frame_time = next_frame_time;
          next_frame_time += step_length_us;

          wait_till(next_frame_time, prev_frame_time);
          // Turn off all triggers (inverted logic)
          PORTD |= B11110000;
          // Including the master (normal logic)
          PORTB &= B11101111;
        }

        // Pre-apply next modulation values
        OCR1B = next_P1;
        OCR2A = next_P2;
        OCR2B = next_O1;
        OCR1A = next_O2;

        // If manual acquisition in progress: update counter and wait
        if (acq_n_frames)
        {
          prev_acq_time = next_frame_time;
          next_acq_time = prev_acq_time + wait_after_us;
          acq_n_frames--;

          wait_till(next_acq_time, prev_acq_time);
        }
      }

      // Start unmodulated manual acquisition
      else if (acq_n_frames)
      {
        prev_acq_time = micros();

        // Turn on the required triggers
        PORTD &= digital_shutters;

        if (exposure_us)
        {
          next_acq_time = prev_acq_time + exposure_us;

          wait_till(next_acq_time, prev_acq_time);
        }
        else
        {
          while (true)
          {
            if (Serial.available())
            {
              read_bytes = Serial.readBytes(rcv_buffer, 2);

              if (read_bytes == 2 && rcv_buffer[0] == ASCII_START && rcv_buffer[1] == HEADER_ACQUIRE && conclude_serial_int(acq_n_frames))
              {
                Serial.write(ASCII_ACK);
                acq_n_frames++;
                next_acq_time = micros();
                break;
              }
              else if (read_bytes == 2 && rcv_buffer[0] == ASCII_START && rcv_buffer[1] == HEADER_RESET && conclude_serial())
              {
                Serial.write(ASCII_ACK);
                reset();
                break;
              }

              purge_serial();
            }
          }
        }

        // Turn off all triggers (inverted logic)
        PORTD |= B11110000;
        // Including the master (normal logic)
        PORTB &= B11101111;

        prev_acq_time = micros();
        next_acq_time = prev_acq_time + wait_after_us;

        acq_n_frames--;

        wait_till(next_acq_time, prev_acq_time);
      }
    }
  }
}
