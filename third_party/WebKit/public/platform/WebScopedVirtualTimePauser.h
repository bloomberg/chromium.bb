// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScopedVirtualTimePauser_h
#define WebScopedVirtualTimePauser_h

#include "WebCommon.h"

namespace blink {
namespace scheduler {
class RendererSchedulerImpl;
}  // namespace scheduler

// A move only RAII style helper which makes it easier for subsystems to pause
// virtual time while performing an asynchronous operation.
class BLINK_PLATFORM_EXPORT WebScopedVirtualTimePauser {
 public:
  // Note simply creating a WebScopedVirtualTimePauser doesn't cause VirtualTime
  // to pause, instead you need to call PauseVirtualTime.
  explicit WebScopedVirtualTimePauser(scheduler::RendererSchedulerImpl*);

  WebScopedVirtualTimePauser();
  ~WebScopedVirtualTimePauser();

  WebScopedVirtualTimePauser(WebScopedVirtualTimePauser&& other);
  WebScopedVirtualTimePauser& operator=(WebScopedVirtualTimePauser&& other);

  WebScopedVirtualTimePauser(const WebScopedVirtualTimePauser&) = delete;
  WebScopedVirtualTimePauser& operator=(const WebScopedVirtualTimePauser&) =
      delete;

  // Virtual time will be paused if any WebScopedVirtualTimePauser votes to
  // pause it, and only unpaused only if all WebScopedVirtualTimePauser are
  // either destroyed or vote to unpause.
  void PauseVirtualTime(bool paused);

 private:
  bool paused_ = false;
  scheduler::RendererSchedulerImpl* scheduler_;  // NOT OWNED
};

}  // namespace blink

#endif  // WebScopedVirtualTimePauser_h
