// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/battor_agent/battor_agent.h"

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "tools/battor_agent/battor_connection_impl.h"

using std::vector;

namespace battor {

namespace {

// The number of seconds that it takes a BattOr to reset.
const uint8_t kBattOrResetTimeSeconds = 2;

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
      command_(Command::INVALID) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

BattOrAgent::~BattOrAgent() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void BattOrAgent::StartTracing() {
  DCHECK(thread_checker_.CalledOnValidThread());

  command_ = Command::START_TRACING;
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
      PerformAction(Action::SEND_RESET);
      break;
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
      // Wait for the reset to happen before sending the init message.
      PerformDelayedAction(Action::SEND_INIT, base::TimeDelta::FromSeconds(
                                                  kBattOrResetTimeSeconds));
      break;
    case Action::SEND_INIT:
      PerformAction(Action::READ_INIT_ACK);
      break;
    case Action::SEND_SET_GAIN:
      PerformAction(Action::READ_SET_GAIN_ACK);
      break;
    case Action::SEND_START_TRACING:
      PerformAction(Action::READ_START_TRACING_ACK);
      break;
    default:
      CompleteCommand(BATTOR_ERROR_UNEXPECTED_MESSAGE);
  }
}

void BattOrAgent::OnMessageRead(bool success,
                                BattOrMessageType type,
                                scoped_ptr<vector<char>> bytes) {
  if (!success) {
    CompleteCommand(BATTOR_ERROR_RECEIVE_ERROR);
    return;
  }

  switch (last_action_) {
    case Action::READ_INIT_ACK:
      if (!IsAckOfControlCommand(type, BATTOR_CONTROL_MESSAGE_TYPE_INIT,
                                 *bytes)) {
        CompleteCommand(BATTOR_ERROR_UNEXPECTED_MESSAGE);
        return;
      }

      PerformAction(Action::SEND_SET_GAIN);
      break;

    case Action::READ_SET_GAIN_ACK:
      if (!IsAckOfControlCommand(type, BATTOR_CONTROL_MESSAGE_TYPE_SET_GAIN,
                                 *bytes)) {
        CompleteCommand(BATTOR_ERROR_UNEXPECTED_MESSAGE);
        return;
      }

      PerformAction(Action::SEND_START_TRACING);
      break;

    case Action::READ_START_TRACING_ACK:
      if (!IsAckOfControlCommand(
              type, BATTOR_CONTROL_MESSAGE_TYPE_START_SAMPLING_SD, *bytes)) {
        CompleteCommand(BATTOR_ERROR_UNEXPECTED_MESSAGE);
        return;
      }

      CompleteCommand(BATTOR_ERROR_NONE);
      return;

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
      break;

    case Action::SEND_RESET:
      // Reset the BattOr to clear any preexisting state. After sending the
      // reset signal, we need to wait for the reset to finish before issuing
      // further commands.
      SendControlMessage(BATTOR_CONTROL_MESSAGE_TYPE_RESET, 0, 0);
      break;
    case Action::SEND_INIT:
      // After resetting the BattOr, we need to make sure to flush the serial
      // stream. Strange data may have been written into it during the reset.
      connection_->Flush();

      SendControlMessage(BATTOR_CONTROL_MESSAGE_TYPE_INIT, 0, 0);
      break;
    case Action::READ_INIT_ACK:
      connection_->ReadMessage(BATTOR_MESSAGE_TYPE_CONTROL_ACK);
      break;
    case Action::SEND_SET_GAIN:
      // Set the BattOr's gain. Setting the gain tells the BattOr the range of
      // power measurements that we expect to see.
      SendControlMessage(BATTOR_CONTROL_MESSAGE_TYPE_SET_GAIN, BATTOR_GAIN_LOW,
                         0);
      break;
    case Action::READ_SET_GAIN_ACK:
      connection_->ReadMessage(BATTOR_MESSAGE_TYPE_CONTROL_ACK);
      break;
    case Action::SEND_START_TRACING:
      SendControlMessage(BATTOR_CONTROL_MESSAGE_TYPE_START_SAMPLING_SD, 0, 0);
      break;
    case Action::READ_START_TRACING_ACK:
      connection_->ReadMessage(BATTOR_MESSAGE_TYPE_CONTROL_ACK);
      break;
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
      listener_->OnStartTracingComplete(error);
      break;
    case Command::INVALID:
      NOTREACHED();
  }

  last_action_ = Action::INVALID;
  command_ = Command::INVALID;
}

}  // namespace battor
