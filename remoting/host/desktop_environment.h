// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_DESKTOP_ENVIRONMENT_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace remoting {

class Capturer;
class Curtain;
class EventExecutor;
class DisconnectWindow;

class DesktopEnvironment {
 public:
  // DesktopEnvironment takes ownership of all the objects passed the ctor.
  DesktopEnvironment(Capturer* capturer, EventExecutor* event_executor,
                     Curtain* curtain, DisconnectWindow* disconnect_window);
  virtual ~DesktopEnvironment();

  Capturer* capturer() const { return capturer_.get(); }
  EventExecutor* event_executor() const { return event_executor_.get(); }
  Curtain* curtain() const { return curtain_.get(); }
  DisconnectWindow* disconnect_window() { return disconnect_window_.get(); }

 private:
  // Capturer to be used by ScreenRecorder.
  scoped_ptr<Capturer> capturer_;

  // Executes input events received from the client.
  scoped_ptr<EventExecutor> event_executor_;

  // Curtain ensures privacy for the remote user.
  scoped_ptr<Curtain> curtain_;

  // Provide a user interface allowing the host user to close the connection.
  scoped_ptr<DisconnectWindow> disconnect_window_;

  DISALLOW_COPY_AND_ASSIGN(DesktopEnvironment);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
