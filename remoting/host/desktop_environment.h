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

namespace remoting {

class Capturer;
class ChromotingHost;
class ChromotingHostContext;
class ContinueWindow;
class Curtain;
class DisconnectWindow;
class EventExecutor;
class LocalInputMonitor;
class UIThreadProxy;

class DesktopEnvironment {
 public:
  static DesktopEnvironment* Create(ChromotingHostContext* context);

  // DesktopEnvironment takes ownership of all the objects passed in.
  DesktopEnvironment(ChromotingHostContext* context,
                     Capturer* capturer,
                     EventExecutor* event_executor,
                     Curtain* curtain,
                     DisconnectWindow* disconnect_window,
                     ContinueWindow* continue_window,
                     LocalInputMonitor* monitor);
  virtual ~DesktopEnvironment();

  // Shuts down the object and all its resources synchronously. Must
  // be called on the UI thread.
  void Shutdown();

  void set_host(ChromotingHost* host) { host_ = host; }

  Capturer* capturer() const { return capturer_.get(); }
  EventExecutor* event_executor() const { return event_executor_.get(); }
  Curtain* curtain() const { return curtain_.get(); }

  // Called whenever a new client has connected.
  void OnConnect(const std::string& username);

  // Called when the last client has disconnected.
  void OnLastDisconnect();

  // Called when the remote connection has been paused/unpaused.
  void OnPause(bool pause);

 private:
  enum ContinueTimerState {
    INACTIVE,      // The timer is not running or has been cancelled.
    SHOW_DIALOG,   // Show the continue dialog when the timer expires.
    SHUTDOWN_HOST  // Shutdown the Chromoting host when the timer expires.
  };

  void ProcessOnConnect(const std::string& username);
  void ProcessOnLastDisconnect();
  void ProcessOnPause(bool pause);

  void MonitorLocalInputs(bool enable);

  // Show or hide the Disconnect window on the UI thread.  If |show| is false,
  // hide the window, ignoring the |username| parameter.
  void ShowDisconnectWindow(bool show, const std::string& username);

  // Show or hide the Continue Sharing window on the UI thread.
  void ShowContinueWindow(bool show);

  void StartContinueWindowTimer(bool start);

  void ContinueWindowTimerFunc();

  // The host that owns this DesktopEnvironment.
  ChromotingHost* host_;

  // Host context used to make sure operations are run on the correct thread.
  // This is owned by the ChromotingHost.
  ChromotingHostContext* context_;

  // Capturer to be used by ScreenRecorder.
  scoped_ptr<Capturer> capturer_;

  // Executes input events received from the client.
  scoped_ptr<EventExecutor> event_executor_;

  // Curtain ensures privacy for the remote user.
  scoped_ptr<Curtain> curtain_;

  // Provide a user interface allowing the host user to close the connection.
  scoped_ptr<DisconnectWindow> disconnect_window_;

  // Provide a user interface requiring the user to periodically re-confirm
  // the connection.
  scoped_ptr<ContinueWindow> continue_window_;

  // Monitor local inputs to allow remote inputs to be blocked while the local
  // user is trying to do something.
  scoped_ptr<LocalInputMonitor> local_input_monitor_;

  bool is_monitoring_local_inputs_;

  // Timer controlling the "continue session" dialog.
  ContinueTimerState continue_timer_state_;
  base::Time continue_timer_target_time_;

  scoped_refptr<UIThreadProxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(DesktopEnvironment);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
