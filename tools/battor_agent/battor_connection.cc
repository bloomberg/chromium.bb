// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/battor_agent/battor_connection.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "device/serial/serial.mojom.h"

namespace {

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

// Callback for when the connection is opened. If an error occurs, error will be
// set to BATTOR_ERROR_CONNECTION_FAILED.
void OnConnectionOpened(bool* success_return,
                        const base::Closure& callback,
                        bool success) {
  *success_return = success;
  callback.Run();
}

// Callback for when a command times out. If a timeout occurs, error will be set
// to BATTOR_ERROR_TIMEOUT.
void OnTimeout(bool* success, const base::Closure& callback) {
  *success = false;
  callback.Run();
}

}  // namespace

namespace battor {

BattOrConnection::BattOrConnection() {}
BattOrConnection::~BattOrConnection() {}

scoped_ptr<BattOrConnection> BattOrConnection::Create(const std::string& path) {
  scoped_ptr<BattOrConnection> conn(new BattOrConnection());

  conn->io_handler_ = device::SerialIoHandler::Create(
      base::MessageLoop::current()->task_runner(),
      base::MessageLoop::current()->task_runner());

  device::serial::ConnectionOptions options;
  options.bitrate = kBattOrBitrate;
  options.data_bits = kBattOrDataBits;
  options.parity_bit = kBattOrParityBit;
  options.stop_bits = kBattOrStopBit;
  options.cts_flow_control = kBattOrCtsFlowControl;
  options.has_cts_flow_control = kBattOrHasCtsFlowControl;

  bool success;
  base::RunLoop run_loop;
  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&OnTimeout, &success, run_loop.QuitClosure()),
      kSerialCommandTimeout);
  conn->io_handler_->Open(
      path, options,
      base::Bind(&OnConnectionOpened, &success, run_loop.QuitClosure()));
  run_loop.Run();

  return success ? conn.Pass() : nullptr;
}

}  // namespace battor
