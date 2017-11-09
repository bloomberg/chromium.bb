// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopedVirtualTimePauser_h
#define ScopedVirtualTimePauser_h

#include "platform/PlatformExport.h"

namespace blink {
namespace scheduler {
class RendererSchedulerImpl;
}  // namespace scheduler

// A move only RAII style helper which makes it easier for subsystems to pause
// virtual time while performing an asynchronous operation.
class PLATFORM_EXPORT ScopedVirtualTimePauser {
 public:
  // Note simply creating a ScopedVirtualTimePauser doesn't cause VirtualTime to
  // pause, instead you need to call PauseVirtualTime.
  explicit ScopedVirtualTimePauser(scheduler::RendererSchedulerImpl*);

  ScopedVirtualTimePauser();
  ~ScopedVirtualTimePauser();

  ScopedVirtualTimePauser(ScopedVirtualTimePauser&& other);
  ScopedVirtualTimePauser& operator=(ScopedVirtualTimePauser&& other);

  ScopedVirtualTimePauser(const ScopedVirtualTimePauser&) = delete;
  ScopedVirtualTimePauser& operator=(const ScopedVirtualTimePauser&) = delete;

  // Virtual time will be paused if any ScopedVirtualTimePauser votes to pause
  // it, and only unpaused only if all ScopedVirtualTimePausers are either
  // destroyed or vote to unpause.
  void PauseVirtualTime(bool paused);

 private:
  bool paused_ = false;
  scheduler::RendererSchedulerImpl* scheduler_;  // NOT OWNED
};

}  // namespace blink

#endif  // ScopedVirtualTimePauser_h
