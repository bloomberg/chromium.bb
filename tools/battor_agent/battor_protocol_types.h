// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BATTOR_AGENT_BATTOR_PROTOCOL_H_
#define TOOLS_BATTOR_AGENT_BATTOR_PROTOCOL_H_

namespace battor {

// Control characters in the BattOr protocol.
enum BattOrControlByte : uint8_t {
  // Indicates the start of a message in the protocol. All other instances of
  // this byte must be escaped (with BATTOR_SPECIAL_BYTE_ESCAPE).
  BATTOR_CONTROL_BYTE_START = 0x00,
  // Indicates the end of a message in the protocol. All other instances of
  // this byte must be escaped (with BATTOR_SPECIAL_BYTE_ESCAPE).
  BATTOR_CONTROL_BYTE_END = 0x01,
  // Indicates that the next byte should not be interpreted as a special
  // character, but should instead be interpreted as itself.
  BATTOR_CONTROL_BYTE_ESCAPE = 0x02,
};

// Types of BattOr messages that can be sent.
enum BattOrMessageType : uint8_t {
  // Indicates a control message sent from the client to the BattOr to tell the
  // BattOr to do something.
  BATTOR_MESSAGE_TYPE_CONTROL = 0x03,
  // Indicates a control message ack sent from the BattOr back to the client to
  // signal that the BattOr received the control message.
  BATTOR_MESSAGE_TYPE_CONTROL_ACK,
  // Indicates that the message contains Voltage and current measurements.
  BATTOR_MESSAGE_TYPE_SAMPLES,
  // TODO(charliea): Figure out what this is.
  BATTOR_MESSAGE_TYPE_PRINT,
};

// Types of BattOr control messages that can be sent.
enum BattOrControlMessageType : uint8_t {
  // Tells the BattOr to initialize itself.
  BATTOR_CONTROL_MESSAGE_TYPE_INIT = 0x00,
  // Sets the current measurement's gain.
  BATTOR_CONTROL_MESSAGE_TYPE_SET_GAIN,
  // Tells the BattOr to start taking samples and sending them over the
  // connection.
  BATTOR_CONTROL_MESSAGE_TYPE_START_SAMPLING_UART,
  // Tells the BattOr to start taking samples and storing them on its SD card.
  BATTOR_CONTROL_MESSAGE_TYPE_START_SAMPLING_SD,
  // Tells the BattOr to start streaming the samples stored on its SD card over
  // the connection.
  BATTOR_CONTROL_MESSAGE_TYPE_READ_SD_UART,
  // Tells the BattOr to send its EEPROM contents over the serial connection.
  BATTOR_CONTROL_MESSAGE_TYPE_READ_EEPROM,
  // Tells the BattOr to reset itself.
  BATTOR_CONTROL_MESSAGE_TYPE_RESET,
  // Tells the BattOr to run a self test.
  BATTOR_CONTROL_MESSAGE_TYPE_SELF_TEST,
};

// The gain level for the BattOr to use.
enum BattOrGain : uint8_t { BATTOR_GAIN_LOW = 0, BATTOR_GAIN_HIGH };

// The data types below are packed to ensure byte-compatibility with the BattOr
// firmware.
#pragma pack(push, 1)

// See: BattOrMessageType::BATTOR_MESSAGE_TYPE_CONTROL above.
struct BattOrControlMessage {
  BattOrControlMessageType type;
  uint16_t param1;
  uint16_t param2;
};

// See: BattOrMessageType::BATTOR_MESSAGE_TYPE_CONTROL_ACK above.
struct BattOrControlMessageAck {
  BattOrControlMessageType type;
  uint8_t param;
};

#pragma pack(pop)

}  // namespace battor

#endif  // TOOLS_BATTOR_AGENT_BATTOR_PROTOCOL_H_
