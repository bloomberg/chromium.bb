// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/battor_agent/battor_agent.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "device/serial/serial.mojom.h"
#include "device/serial/serial_io_handler.h"

namespace {

// Serial configuration parameters for the BattOr.
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

BattOrAgent::BattOrAgent(
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner,
    const std::string& path,
    Listener* listener)
    : file_thread_task_runner_(file_thread_task_runner),
      ui_thread_task_runner_(ui_thread_task_runner),
      path_(path),
      listener_(listener) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

BattOrAgent::~BattOrAgent() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void BattOrAgent::StartTracing() {
  DCHECK(thread_checker_.CalledOnValidThread());

  ConnectIfNeeded(
      base::Bind(&BattOrAgent::DoStartTracing, AsWeakPtr()),
      base::Bind(&Listener::OnStartTracingComplete, base::Unretained(listener_),
                 BATTOR_ERROR_CONNECTION_FAILED));
}

void BattOrAgent::DoStartTracing() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(charliea): Tell the BattOr to start tracing.
  listener_->OnStartTracingComplete(BATTOR_ERROR_NONE);
}

void BattOrAgent::ConnectIfNeeded(const base::Closure& success_callback,
                                  const base::Closure& failure_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (io_handler_) {
    success_callback.Run();
    return;
  }

  io_handler_ = device::SerialIoHandler::Create(file_thread_task_runner_,
                                                ui_thread_task_runner_);

  device::serial::ConnectionOptions options;
  options.bitrate = kBattOrBitrate;
  options.data_bits = kBattOrDataBits;
  options.parity_bit = kBattOrParityBit;
  options.stop_bits = kBattOrStopBit;
  options.cts_flow_control = kBattOrCtsFlowControl;
  options.has_cts_flow_control = kBattOrHasCtsFlowControl;

  io_handler_->Open(path_, options,
                    base::Bind(&BattOrAgent::OnConnectComplete, AsWeakPtr(),
                               success_callback, failure_callback));
}

void BattOrAgent::OnConnectComplete(const base::Closure& success_callback,
                                    const base::Closure& failure_callback,
                                    bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (success) {
    success_callback.Run();
  } else {
    io_handler_ = nullptr;
    failure_callback.Run();
  }
}

}  // namespace battor
