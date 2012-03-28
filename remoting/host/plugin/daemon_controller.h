// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DAEMON_CONTROLLER_H_
#define REMOTING_HOST_DAEMON_CONTROLLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace remoting {

class DaemonController {
 public:
  // Note that these enumeration values are duplicated in daemon_plugin.js and
  // must be kept in sync.
  enum State {
    // Placeholder state for platforms on which the daemon process is not
    // implemented. The web-app will not show the corresponding UI. This value
    // will eventually be deprecated or removed.
    STATE_NOT_IMPLEMENTED = -1,
    // The daemon process is not installed. This is functionally equivalent
    // to STATE_STOPPED, but the start method is expected to be significantly
    // slower, and might involve user interaction. It might be appropriate to
    // indicate this in the UI.
    STATE_NOT_INSTALLED = 0,
    // The daemon process is installed but not running. Call Start to start it.
    STATE_STOPPED = 1,
    // The daemon process is running. Call Start again to change the PIN or
    // Stop to stop it.
    STATE_STARTED = 2,
    // The previous Start operation failed. This is functionally equivalent
    // to STATE_STOPPED, but the UI should report an error in this state.
    // This state should not persist across restarts of the NPAPI process.
    STATE_START_FAILED = 3,
    // The state cannot be determined. This could indicate that the plugin
    // has not been provided with sufficient information, for example, the
    // user for which to query state on a multi-user system.
    STATE_UNKNOWN = 4
  };

  // The callback for GetConfig(). |config| is set to NULL in case of
  // an error. Otherwise it is a dictionary that contains the
  // following values: host_id and xmpp_login, which may be empty if
  // the host is not initialized yet. The config must not contain any
  // security sensitive information, such as authentication tokens and
  // private keys.
  typedef base::Callback<void (scoped_ptr<base::DictionaryValue> config)>
      GetConfigCallback;

  virtual ~DaemonController() {}

  // Return the "installed/running" state of the daemon process.
  //
  // TODO(sergeyu): This method is called synchronously from the
  // webapp. In most cases it requires IO operations, so it may block
  // the user interface. Replace it with asynchronous notifications,
  // e.g. with StartStateNotifications()/StopStateNotifications() methods.
  virtual State GetState() = 0;

  // Queries current host configuration. The |callback| is called
  // after configuration is read.
  virtual void GetConfig(const GetConfigCallback& callback) = 0;

  // Start the daemon process. Since this may require that the daemon be
  // downloaded and installed, Start may return before the daemon process
  // is actually running--poll GetState until the state is STATE_STARTED
  // or STATE_START_FAILED.
  //
  // TODO(sergeyu): This method writes config and starts the host -
  // these two steps are merged for simplicity. Consider splitting it
  // into SetConfig() and Start() once we have basic host setup flow
  // working.
  virtual void SetConfigAndStart(scoped_ptr<base::DictionaryValue> config) = 0;

  // Set the PIN for accessing this host, which should be expressed as a
  // UTF8-encoded string. It is permitted to call SetPin when the daemon
  // is already running. Returns true if successful, or false if the PIN
  // does not satisfy complexity requirements.
  //
  // TODO(sergeyu): Add callback to be called after PIN is updated.
  virtual void SetPin(const std::string& pin) = 0;

  // Stop the daemon process. It is permitted to call Stop while the daemon
  // process is being installed, in which case the installation should be
  // aborted if possible; if not then it is sufficient to ensure that the
  // daemon process is not started automatically upon successful installation.
  // As with Start, Stop may return before the operation is complete--poll
  // GetState until the state is STATE_STOPPED.
  virtual void Stop() = 0;

  static DaemonController* Create();
};

}  // namespace remoting

#endif  // REMOTING_HOST_DAEMON_CONTROLLER_H_
