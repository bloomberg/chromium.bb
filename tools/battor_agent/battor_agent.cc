// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/battor_agent/battor_agent.h"

namespace battor {

BattOrAgent::BattOrAgent(
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner,
    const std::string& path,
    Listener* listener)
    : listener_(listener),
      connection_(new BattOrConnection(path,
                                       this,
                                       file_thread_task_runner,
                                       ui_thread_task_runner)) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

BattOrAgent::~BattOrAgent() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void BattOrAgent::StartTracing() {
  DCHECK(thread_checker_.CalledOnValidThread());

  ConnectIfNeeded();
}

void BattOrAgent::DoStartTracing() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(charliea): Tell the BattOr to start tracing.
  listener_->OnStartTracingComplete(BATTOR_ERROR_NONE);
}

void BattOrAgent::OnConnectionOpened(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(charliea): Rewrite this in a way that allows for multiple tracing
  // commands.
  if (success) {
    DoStartTracing();
  } else {
    connection_.reset();
    listener_->OnStartTracingComplete(BATTOR_ERROR_CONNECTION_FAILED);
  }
}

void BattOrAgent::OnBytesSent(bool success) {}

void BattOrAgent::OnBytesRead(bool success,
                              BattOrMessageType type,
                              scoped_ptr<std::vector<char>> bytes) {}

void BattOrAgent::ConnectIfNeeded() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (connection_->IsOpen()) {
    // TODO(charliea): Rewrite this in a way that allows for multiple tracing
    // commands.
    DoStartTracing();
    return;
  }

  connection_->Open();
}

}  // namespace battor
