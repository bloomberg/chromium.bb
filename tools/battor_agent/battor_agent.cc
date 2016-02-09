// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/battor_agent/battor_agent.h"

#include <iomanip>

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "tools/battor_agent/battor_connection_impl.h"
#include "tools/battor_agent/battor_sample_converter.h"

using std::vector;

namespace battor {

namespace {

// The number of seconds that it takes a BattOr to reset.
const uint8_t kBattOrResetTimeSeconds = 5;

// The maximum number of times to retry when reading a message.
const uint8_t kMaxReadAttempts = 20;

// The number of milliseconds to wait before trying to read a message again.
const uint8_t kReadRetryDelayMilliseconds = 1;

// Returns true if the specified vector of bytes decodes to a message that is an
// ack for the specified control message type.
bool IsAckOfControlCommand(BattOrMessageType message_type,
                           BattOrControlMessageType control_message_type,
                           const vector<char>& msg) {
  if (message_type != BATTOR_MESSAGE_TYPE_CONTROL_ACK)
    return false;

  if (msg.size() != sizeof(BattOrControlMessageAck))
    return false;

  const BattOrControlMessageAck* ack =
      reinterpret_cast<const BattOrControlMessageAck*>(msg.data());

  if (ack->type != control_message_type)
    return false;

  return true;
}

// Attempts to decode the specified vector of bytes decodes to a valid EEPROM.
// Returns the new EEPROM, or nullptr if unsuccessful.
scoped_ptr<BattOrEEPROM> ParseEEPROM(BattOrMessageType message_type,
                                     const vector<char>& msg) {
  if (message_type != BATTOR_MESSAGE_TYPE_CONTROL_ACK)
    return nullptr;

  if (msg.size() != sizeof(BattOrEEPROM))
    return nullptr;

  scoped_ptr<BattOrEEPROM> eeprom(new BattOrEEPROM());
  memcpy(eeprom.get(), msg.data(), sizeof(BattOrEEPROM));
  return eeprom;
}

// Returns true if the specified vector of bytes decodes to a valid BattOr
// samples frame. The frame header and samples are returned via the frame_header
// and samples paramaters.
bool ParseSampleFrame(BattOrMessageType type,
                      const vector<char>& msg,
                      uint32_t expected_sequence_number,
                      BattOrFrameHeader* frame_header,
                      vector<RawBattOrSample>* samples) {
  if (type != BATTOR_MESSAGE_TYPE_SAMPLES)
    return false;

  // Each frame should contain a header and an integer number of BattOr samples.
  if ((msg.size() - sizeof(BattOrFrameHeader)) % sizeof(RawBattOrSample) != 0)
    return false;

  // The first bytes in the frame contain the frame header.
  const char* frame_ptr = reinterpret_cast<const char*>(msg.data());
  memcpy(frame_header, frame_ptr, sizeof(BattOrFrameHeader));
  frame_ptr += sizeof(BattOrFrameHeader);

  if (frame_header->sequence_number != expected_sequence_number) {
    LOG(WARNING) << "Unexpected sequence number: wanted "
                 << expected_sequence_number << ", but got "
                 << frame_header->sequence_number << ".";
    return false;
  }

  size_t remaining_bytes = msg.size() - sizeof(BattOrFrameHeader);
  if (remaining_bytes != frame_header->length)
    return false;

  samples->resize(remaining_bytes / sizeof(RawBattOrSample));
  memcpy(samples->data(), frame_ptr, remaining_bytes);

  return true;
}

}  // namespace

BattOrAgent::BattOrAgent(
    const std::string& path,
    Listener* listener,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
    : connection_(new BattOrConnectionImpl(path,
                                           this,
                                           file_thread_task_runner,
                                           ui_thread_task_runner)),
      listener_(listener),
      last_action_(Action::INVALID),
      command_(Command::INVALID),
      num_read_attempts_(0) {
  // We don't care what thread the constructor is called on - we only care that
  // all of the other method invocations happen on the same thread.
  thread_checker_.DetachFromThread();
}

BattOrAgent::~BattOrAgent() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void BattOrAgent::StartTracing() {
  DCHECK(thread_checker_.CalledOnValidThread());

  command_ = Command::START_TRACING;
  PerformAction(Action::REQUEST_CONNECTION);
}

void BattOrAgent::StopTracing() {
  DCHECK(thread_checker_.CalledOnValidThread());

  command_ = Command::STOP_TRACING;
  PerformAction(Action::REQUEST_CONNECTION);
}

void BattOrAgent::BeginConnect() {
  DCHECK(thread_checker_.CalledOnValidThread());

  connection_->Open();
}

void BattOrAgent::OnConnectionOpened(bool success) {
  if (!success) {
    CompleteCommand(BATTOR_ERROR_CONNECTION_FAILED);
    return;
  }

  switch (command_) {
    case Command::START_TRACING:
      // TODO(charliea): Ideally, we'd just like to send an init, and the BattOr
      // firmware can handle whether a reset is necessary or not, sending an
      // init ack regardless. This reset can be removed once this is true.
      // https://github.com/aschulm/battor/issues/30 tracks this.
      PerformAction(Action::SEND_RESET);
      return;
    case Command::STOP_TRACING:
      PerformAction(Action::SEND_EEPROM_REQUEST);
      return;
    case Command::INVALID:
      NOTREACHED();
  }
}

void BattOrAgent::OnBytesSent(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!success) {
    CompleteCommand(BATTOR_ERROR_SEND_ERROR);
    return;
  }

  switch (last_action_) {
    case Action::SEND_RESET:
      // TODO(charliea): Ideally, we'd just like to send an init, and the BattOr
      // firmware can handle whether a reset is necessary or not, sending an
      // init ack regardless. This reset can be removed once this is true.
      // https://github.com/aschulm/battor/issues/30 tracks this.

      // Wait for the reset to happen before sending the init message.
      PerformDelayedAction(Action::SEND_INIT, base::TimeDelta::FromSeconds(
                                                  kBattOrResetTimeSeconds));
      return;
    case Action::SEND_INIT:
      PerformAction(Action::READ_INIT_ACK);
      return;
    case Action::SEND_SET_GAIN:
      PerformAction(Action::READ_SET_GAIN_ACK);
      return;
    case Action::SEND_START_TRACING:
      PerformAction(Action::READ_START_TRACING_ACK);
      return;
    case Action::SEND_EEPROM_REQUEST:
      num_read_attempts_ = 1;
      PerformAction(Action::READ_EEPROM);
      return;
    case Action::SEND_SAMPLES_REQUEST:
      num_read_attempts_ = 1;
      PerformAction(Action::READ_CALIBRATION_FRAME);
      return;

    default:
      CompleteCommand(BATTOR_ERROR_UNEXPECTED_MESSAGE);
  }
}

void BattOrAgent::OnMessageRead(bool success,
                                BattOrMessageType type,
                                scoped_ptr<vector<char>> bytes) {
  if (!success) {
    switch (last_action_) {
      case Action::READ_EEPROM:
      case Action::READ_CALIBRATION_FRAME:
      case Action::READ_DATA_FRAME:
        if (++num_read_attempts_ > kMaxReadAttempts) {
          CompleteCommand(BATTOR_ERROR_RECEIVE_ERROR);
          return;
        }

        PerformDelayedAction(last_action_, base::TimeDelta::FromMilliseconds(
                                               kReadRetryDelayMilliseconds));
        return;

      default:
        CompleteCommand(BATTOR_ERROR_RECEIVE_ERROR);
        return;
    }
  }

  switch (last_action_) {
    case Action::READ_INIT_ACK:
      if (!IsAckOfControlCommand(type, BATTOR_CONTROL_MESSAGE_TYPE_INIT,
                                 *bytes)) {
        CompleteCommand(BATTOR_ERROR_UNEXPECTED_MESSAGE);
        return;
      }

      PerformAction(Action::SEND_SET_GAIN);
      return;

    case Action::READ_SET_GAIN_ACK:
      if (!IsAckOfControlCommand(type, BATTOR_CONTROL_MESSAGE_TYPE_SET_GAIN,
                                 *bytes)) {
        CompleteCommand(BATTOR_ERROR_UNEXPECTED_MESSAGE);
        return;
      }

      PerformAction(Action::SEND_START_TRACING);
      return;

    case Action::READ_START_TRACING_ACK:
      if (!IsAckOfControlCommand(
              type, BATTOR_CONTROL_MESSAGE_TYPE_START_SAMPLING_SD, *bytes)) {
        CompleteCommand(BATTOR_ERROR_UNEXPECTED_MESSAGE);
        return;
      }

      CompleteCommand(BATTOR_ERROR_NONE);
      return;

    case Action::READ_EEPROM:
      battor_eeprom_ = ParseEEPROM(type, *bytes);
      if (!battor_eeprom_) {
        CompleteCommand(BATTOR_ERROR_UNEXPECTED_MESSAGE);
        return;
      }

      PerformAction(Action::SEND_SAMPLES_REQUEST);
      return;

    case Action::READ_CALIBRATION_FRAME: {
      BattOrFrameHeader frame_header;
      if (!ParseSampleFrame(type, *bytes, next_sequence_number_++,
                            &frame_header, &calibration_frame_)) {
        CompleteCommand(BATTOR_ERROR_UNEXPECTED_MESSAGE);
        return;
      }

      // Make sure that the calibration frame has actual samples in it.
      if (calibration_frame_.empty()) {
        CompleteCommand(BATTOR_ERROR_UNEXPECTED_MESSAGE);
        return;
      }

      num_read_attempts_ = 1;
      PerformAction(Action::READ_DATA_FRAME);
      return;
    }

    case Action::READ_DATA_FRAME: {
      BattOrFrameHeader frame_header;
      vector<RawBattOrSample> frame;
      if (!ParseSampleFrame(type, *bytes, next_sequence_number_++,
                            &frame_header, &frame)) {
        CompleteCommand(BATTOR_ERROR_UNEXPECTED_MESSAGE);
        return;
      }

      // Check for the empty frame the BattOr uses to indicate it's done
      // streaming samples.
      if (frame.empty()) {
        CompleteCommand(BATTOR_ERROR_NONE);
        return;
      }

      samples_.insert(samples_.end(), frame.begin(), frame.end());

      num_read_attempts_ = 1;
      PerformAction(Action::READ_DATA_FRAME);
      return;
    }

    default:
      CompleteCommand(BATTOR_ERROR_UNEXPECTED_MESSAGE);
  }
}

void BattOrAgent::PerformAction(Action action) {
  DCHECK(thread_checker_.CalledOnValidThread());

  last_action_ = action;

  switch (action) {
    case Action::REQUEST_CONNECTION:
      BeginConnect();
      return;

    // The following actions are required for StartTracing:
    case Action::SEND_RESET:
      // Reset the BattOr to clear any preexisting state. After sending the
      // reset signal, we need to wait for the reset to finish before issuing
      // further commands.
      SendControlMessage(BATTOR_CONTROL_MESSAGE_TYPE_RESET, 0, 0);
      return;
    case Action::SEND_INIT:
      // After resetting the BattOr, we need to make sure to flush the serial
      // stream. Strange data may have been written into it during the reset.
      connection_->Flush();

      SendControlMessage(BATTOR_CONTROL_MESSAGE_TYPE_INIT, 0, 0);
      return;
    case Action::READ_INIT_ACK:
      connection_->ReadMessage(BATTOR_MESSAGE_TYPE_CONTROL_ACK);
      return;
    case Action::SEND_SET_GAIN:
      // Set the BattOr's gain. Setting the gain tells the BattOr the range of
      // power measurements that we expect to see.
      SendControlMessage(BATTOR_CONTROL_MESSAGE_TYPE_SET_GAIN, BATTOR_GAIN_LOW,
                         0);
      return;
    case Action::READ_SET_GAIN_ACK:
      connection_->ReadMessage(BATTOR_MESSAGE_TYPE_CONTROL_ACK);
      return;
    case Action::SEND_START_TRACING:
      SendControlMessage(BATTOR_CONTROL_MESSAGE_TYPE_START_SAMPLING_SD, 0, 0);
      return;
    case Action::READ_START_TRACING_ACK:
      connection_->ReadMessage(BATTOR_MESSAGE_TYPE_CONTROL_ACK);
      return;

    // The following actions are required for StopTracing:
    case Action::SEND_EEPROM_REQUEST:
      // Read the BattOr's EEPROM to get calibration information that's required
      // to convert the raw samples to accurate ones.
      SendControlMessage(BATTOR_CONTROL_MESSAGE_TYPE_READ_EEPROM,
                         sizeof(BattOrEEPROM), 0);
      return;
    case Action::READ_EEPROM:
      connection_->ReadMessage(BATTOR_MESSAGE_TYPE_CONTROL_ACK);
      return;
    case Action::SEND_SAMPLES_REQUEST:
      // Send a request to the BattOr to tell it to start streaming the samples
      // that it's stored on its SD card over the serial connection.
      SendControlMessage(BATTOR_CONTROL_MESSAGE_TYPE_READ_SD_UART, 0, 0);
      return;
    case Action::READ_CALIBRATION_FRAME:
      // Data frames are numbered starting at zero and counting up by one each
      // data frame. We keep track of the next frame sequence number we expect
      // to see to ensure we don't miss any data.
      next_sequence_number_ = 0;
    case Action::READ_DATA_FRAME:
      // The first frame sent back from the BattOr contains voltage and current
      // data that excludes whatever device is being measured from the
      // circuit. We use this first frame to establish a baseline voltage and
      // current.
      //
      // All further frames contain real (but uncalibrated) voltage and current
      // data.
      connection_->ReadMessage(BATTOR_MESSAGE_TYPE_SAMPLES);
      return;

    case Action::INVALID:
      NOTREACHED();
  }
}

void BattOrAgent::PerformDelayedAction(Action action, base::TimeDelta delay) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&BattOrAgent::PerformAction, AsWeakPtr(), action),
      delay);
}

void BattOrAgent::SendControlMessage(BattOrControlMessageType type,
                                     uint16_t param1,
                                     uint16_t param2) {
  DCHECK(thread_checker_.CalledOnValidThread());

  BattOrControlMessage msg{type, param1, param2};
  connection_->SendBytes(BATTOR_MESSAGE_TYPE_CONTROL, &msg, sizeof(msg));
}

void BattOrAgent::CompleteCommand(BattOrError error) {
  switch (command_) {
    case Command::START_TRACING:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&Listener::OnStartTracingComplete,
                                base::Unretained(listener_), error));
      break;
    case Command::STOP_TRACING: {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&Listener::OnStopTracingComplete,
                     base::Unretained(listener_), SamplesToString(), error));
      break;
    }
    case Command::INVALID:
      NOTREACHED();
  }

  last_action_ = Action::INVALID;
  command_ = Command::INVALID;
  battor_eeprom_.reset();
  calibration_frame_.clear();
  samples_.clear();
  next_sequence_number_ = 0;
}

std::string BattOrAgent::SamplesToString() {
  if (calibration_frame_.empty() || samples_.empty() || !battor_eeprom_)
    return "";

  BattOrSampleConverter converter(*battor_eeprom_, calibration_frame_);

  std::stringstream trace_stream;
  trace_stream << std::fixed;

  // Create a header that indicates the BattOr's parameters for these samples.
  BattOrSample min_sample = converter.MinSample();
  BattOrSample max_sample = converter.MaxSample();
  trace_stream << "# BattOr" << std::endl
               << std::setprecision(1)
               << "# voltage_range [" <<  min_sample.voltage_mV << ", "
               << max_sample.voltage_mV << "] mV" << std::endl
               << "# current_range [" << min_sample.current_mA << ", "
               << max_sample.current_mA << "] mA" << std::endl
               << "# sample_rate " << battor_eeprom_->sd_sample_rate << " Hz"
               << ", gain " << battor_eeprom_->low_gain << "x" << std::endl;

  // Create a string representation of the BattOr samples.
  for (size_t i = 0; i < samples_.size(); i++) {
    BattOrSample sample = converter.ToSample(samples_[i], i);
    trace_stream << std::setprecision(2) << sample.time_ms << " "
                 << std::setprecision(1) << sample.current_mA << " "
                 << sample.voltage_mV << std::endl;
  }

  return trace_stream.str();
}

}  // namespace battor
