// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/battor_agent/battor_agent.h"

#include <iostream>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "device/serial/serial.mojom.h"

namespace {

// Callback for when the connection is opened. If an error occurs, error will be
// set to BATTOR_ERROR_CONNECTION_FAILED.
void OnConnectionOpened(battor::BattOrAgent::BattOrError* error,
                        const base::Closure& callback,
                        bool success) {
  *error = success ? battor::BattOrAgent::BATTOR_ERROR_NONE
                   : battor::BattOrAgent::BATTOR_ERROR_CONNECTION_FAILED;
  callback.Run();
}

// Callback for when a command times out. If a timeout occurs, error will be set
// to BATTOR_ERROR_TIMEOUT.
void OnTimeout(battor::BattOrAgent::BattOrError* error,
               const base::Closure& callback) {
  *error = battor::BattOrAgent::BATTOR_ERROR_TIMEOUT;
  callback.Run();
}

// Serial configuration parameters for the BattOr.
const base::TimeDelta kSerialCommandTimeout = base::TimeDelta::FromSeconds(10);
const uint32 kBattOrBitrate = 2000000;
const device::serial::DataBits kBattOrDataBits =
    device::serial::DATA_BITS_EIGHT;
const device::serial::ParityBit kBattOrParityBit =
    device::serial::PARITY_BIT_NONE;
const device::serial::StopBits kBattOrStopBit = device::serial::STOP_BITS_ONE;
const bool kBattOrCtsFlowControl = true;
const bool kBattOrHasCtsFlowControl = true;

}  // namespace

namespace battor {

BattOrAgent::BattOrAgent(const std::string& path) : path_(path) {}
BattOrAgent::~BattOrAgent() {}

BattOrAgent::BattOrError BattOrAgent::StartTracing() {
  BattOrError error = ConnectIfNeeded();
  if (error != BATTOR_ERROR_NONE)
    return error;

  // TODO(charliea): Tell the BattOr to start tracing.
  return BATTOR_ERROR_NONE;
}

BattOrAgent::BattOrError BattOrAgent::StopTracing(std::string* trace_output) {
  BattOrError error = ConnectIfNeeded();
  if (error != BATTOR_ERROR_NONE)
    return error;

  // TODO(charliea): Tell the BattOr to stop tracing.
  *trace_output = "battor trace output";

  ResetConnection();
  return BATTOR_ERROR_NONE;
}

BattOrAgent::BattOrError BattOrAgent::RecordClockSyncMarker(
    const std::string& marker) {
  BattOrError error = ConnectIfNeeded();
  if (error != BATTOR_ERROR_NONE)
    return error;

  // TODO(charliea): Tell the BattOr to record the specified clock sync marker.
  return BATTOR_ERROR_NONE;
}

BattOrAgent::BattOrError BattOrAgent::IssueClockSyncMarker() {
  BattOrError error = ConnectIfNeeded();
  if (error != BATTOR_ERROR_NONE)
    return error;

  // TODO(charliea): Tell atrace to issue a clock sync marker.
  return BATTOR_ERROR_NONE;
}

void BattOrAgent::ResetConnection() {
  io_handler_ = nullptr;
}

BattOrAgent::BattOrError BattOrAgent::ConnectIfNeeded() {
  if (io_handler_)
    return BATTOR_ERROR_NONE;

  io_handler_ = device::SerialIoHandler::Create(
      base::MessageLoop::current()->task_runner(),
      base::MessageLoop::current()->task_runner());

  device::serial::ConnectionOptions options;
  options.bitrate = kBattOrBitrate;
  options.data_bits = kBattOrDataBits;
  options.parity_bit = kBattOrParityBit;
  options.stop_bits = kBattOrStopBit;
  options.cts_flow_control = kBattOrCtsFlowControl;
  options.has_cts_flow_control = kBattOrHasCtsFlowControl;

  BattOrError error;
  base::RunLoop run_loop;
  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&OnTimeout, &error, run_loop.QuitClosure()),
      kSerialCommandTimeout);
  io_handler_->Open(path_, options, base::Bind(&OnConnectionOpened, &error,
                                               run_loop.QuitClosure()));
  run_loop.Run();

  if (error != BATTOR_ERROR_NONE) {
    error = BATTOR_ERROR_CONNECTION_FAILED;
    ResetConnection();
  }

  // TODO(charliea): Complete the inialization routine by sending the init
  // message, setting the gain, reading the EEPROM, and setting the sample rate.

  return error;
}

}  // namespace battor
