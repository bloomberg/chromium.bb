// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/embedder/default_mach_broker.h"

#include "base/logging.h"

namespace mojo {
namespace core {

namespace {
const char kBootstrapPortName[] = "mojo_default_mach_broker";
}

// static
void DefaultMachBroker::SendTaskPortToParent() {
  bool result =
      base::MachPortBroker::ChildSendTaskPortToParent(kBootstrapPortName);
  DCHECK(result);
}

// static
DefaultMachBroker* DefaultMachBroker::Get() {
  static DefaultMachBroker* broker = new DefaultMachBroker;
  return broker;
}

DefaultMachBroker::DefaultMachBroker() : broker_(kBootstrapPortName) {
  bool result = broker_.Init();
  DCHECK(result);
}

DefaultMachBroker::~DefaultMachBroker() {}

void DefaultMachBroker::ExpectPid(base::ProcessHandle pid) {
  broker_.AddPlaceholderForPid(pid);
}

void DefaultMachBroker::RemovePid(base::ProcessHandle pid) {
  broker_.InvalidatePid(pid);
}

}  // namespace core
}  // namespace mojo
