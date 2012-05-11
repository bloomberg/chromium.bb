// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "remoting/host/event_executor.h"

namespace remoting {

class Capturer;
class ChromotingHost;
class ChromotingHostContext;

namespace protocol {
class HostEventStub;
};

class DesktopEnvironment {
 public:
  // Creates a DesktopEnvironment used in a host plugin.
  static scoped_ptr<DesktopEnvironment> Create(
      ChromotingHostContext* context);

  // Creates a DesktopEnvironment used in a service process.
  static scoped_ptr<DesktopEnvironment> CreateForService(
      ChromotingHostContext* context);

  static scoped_ptr<DesktopEnvironment> CreateFake(
      ChromotingHostContext* context,
      scoped_ptr<Capturer> capturer,
      scoped_ptr<EventExecutor> event_executor);

  virtual ~DesktopEnvironment();

  void set_host(ChromotingHost* host) { host_ = host; }

  Capturer* capturer() const { return capturer_.get(); }
  EventExecutor* event_executor() const { return event_executor_.get(); }
  void OnSessionStarted();
  void OnSessionFinished();

 private:
  DesktopEnvironment(ChromotingHostContext* context,
                     scoped_ptr<Capturer> capturer,
                     scoped_ptr<EventExecutor> event_executor);

  // The host that owns this DesktopEnvironment.
  ChromotingHost* host_;

  // Host context used to make sure operations are run on the correct thread.
  // This is owned by the ChromotingHost.
  ChromotingHostContext* context_;

  // Capturer to be used by ScreenRecorder.
  scoped_ptr<Capturer> capturer_;

  // Executes input and clipboard events received from the client.
  scoped_ptr<EventExecutor> event_executor_;

  DISALLOW_COPY_AND_ASSIGN(DesktopEnvironment);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
