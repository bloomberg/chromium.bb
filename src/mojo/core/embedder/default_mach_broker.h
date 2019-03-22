// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_EMBEDDER_DEFAULT_MACH_BROKER_H_
#define MOJO_CORE_EMBEDDER_DEFAULT_MACH_BROKER_H_

#include "base/component_export.h"
#include "base/mac/mach_port_broker.h"
#include "base/macros.h"

namespace mojo {
namespace core {

// A singleton Mojo embedders can use to manage task port brokering among
// connected processes.
class COMPONENT_EXPORT(MOJO_CORE_EMBEDDER) DefaultMachBroker {
 public:
  // Sends the task port of the current process to the parent over Mach IPC.
  // For use in child processes.
  static void SendTaskPortToParent();

  // Returns the global |DefaultMachBroker|.
  static DefaultMachBroker* Get();

  // Registers |pid| with a MACH_PORT_NULL task port in the port provider. A
  // child's pid must be registered before the broker will accept a task port
  // from that child.
  //
  // Callers MUST have the lock acquired (see |GetLock()) while calling this.
  void ExpectPid(base::ProcessHandle pid);

  // Removes |pid| from the port provider.
  //
  // Callers MUST have the lock acquired (see |GetLock()) while calling this.
  void RemovePid(base::ProcessHandle pid);

  base::Lock& GetLock() { return broker_.GetLock(); }
  base::PortProvider* port_provider() { return &broker_; }

 private:
  DefaultMachBroker();
  ~DefaultMachBroker();

  base::MachPortBroker broker_;

  DISALLOW_COPY_AND_ASSIGN(DefaultMachBroker);
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_EMBEDDER_DEFAULT_MACH_BROKER_H_
