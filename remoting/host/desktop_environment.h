// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_DESKTOP_ENVIRONMENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "remoting/base/scoped_thread_proxy.h"

namespace remoting {

class Capturer;
class ChromotingHost;
class ChromotingHostContext;
class EventExecutor;

class DesktopEnvironment {
 public:
  static DesktopEnvironment* Create(ChromotingHostContext* context);

  // DesktopEnvironment takes ownership of all the objects passed in.
  DesktopEnvironment(ChromotingHostContext* context,
                     Capturer* capturer,
                     EventExecutor* event_executor);
  virtual ~DesktopEnvironment();

  void set_host(ChromotingHost* host) { host_ = host; }

  Capturer* capturer() const { return capturer_.get(); }
  EventExecutor* event_executor() const { return event_executor_.get(); }

 private:
  // The host that owns this DesktopEnvironment.
  ChromotingHost* host_;

  // Host context used to make sure operations are run on the correct thread.
  // This is owned by the ChromotingHost.
  ChromotingHostContext* context_;

  // Capturer to be used by ScreenRecorder.
  scoped_ptr<Capturer> capturer_;

  // Executes input events received from the client.
  scoped_ptr<EventExecutor> event_executor_;

  DISALLOW_COPY_AND_ASSIGN(DesktopEnvironment);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
