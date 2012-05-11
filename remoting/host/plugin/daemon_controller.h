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
    // The daemon is not installed. This is functionally equivalent to
    // STATE_STOPPED, but the start method is expected to be significantly
    // slower, and might involve user interaction. It might be appropriate to
    // indicate this in the UI.
    STATE_NOT_INSTALLED = 0,
    // The daemon is being installed.
    STATE_INSTALLING = 1,
    // The daemon is installed but not running. Call Start to start it.
    STATE_STOPPED = 2,
    // The daemon process is starting.
    STATE_STARTING = 3,
    // The daemon process is running. Call Start again to change the PIN or
    // Stop to stop it.
    STATE_STARTED = 4,
    // The daemon process is stopping.
    STATE_STOPPING = 5,
    // The state cannot be determined. This could indicate that the plugin
    // has not been provided with sufficient information, for example, the
    // user for which to query state on a multi-user system.
    STATE_UNKNOWN = 6
  };

  // Enum used for completion callback.
  enum AsyncResult {
    RESULT_OK = 0,

    // The operation has FAILED.
    RESULT_FAILED = 1,

    // User has cancelled the action (e.g. rejected UAC prompt).
    // TODO(sergeyu): Current implementations don't return this value.
    RESULT_CANCELLED = 2,

    // Failed to access host directory.
    RESULT_FAILED_DIRECTORY = 3

    // TODO(sergeyu): Add more error codes when we know how to handle
    // them in the webapp.
  };

  // Callback type for GetConfig(). If the host is configured then a dictionary
  // is returned containing host_id and xmpp_login, with security-sensitive
  // fields filtered out. An empty dictionary is returned if the host is not
  // configured, and NULL if the configuration is corrupt or cannot be read.
  typedef base::Callback<void (scoped_ptr<base::DictionaryValue> config)>
      GetConfigCallback;

  // Callback used for asynchronous operations, e.g. when
  // starting/stopping the service.
  typedef base::Callback<void (AsyncResult result)> CompletionCallback;

  // Callback type for GetVersion().
  typedef base::Callback<void (const std::string&)> GetVersionCallback;

  virtual ~DaemonController() {}

  // Return the "installed/running" state of the daemon process.
  //
  // TODO(sergeyu): This method is called synchronously from the
  // webapp. In most cases it requires IO operations, so it may block
  // the user interface. Replace it with asynchronous notifications,
  // e.g. with StartStateNotifications()/StopStateNotifications() methods.
  virtual State GetState() = 0;

  // Queries current host configuration. The |callback| is called
  // after the configuration is read, and any values that might be security
  // sensitive have been filtered out.
  virtual void GetConfig(const GetConfigCallback& callback) = 0;

  // Start the daemon process. This may require that the daemon be
  // downloaded and installed. |done_callback| is called when the
  // operation is finished or fails.
  //
  // TODO(sergeyu): This method writes config and starts the host -
  // these two steps are merged for simplicity. Consider splitting it
  // into SetConfig() and Start() once we have basic host setup flow
  // working.
  virtual void SetConfigAndStart(scoped_ptr<base::DictionaryValue> config,
                                 const CompletionCallback& done_callback) = 0;

  // Updates current host configuration with the values specified in
  // |config|. Changes must take effect before the call completes.
  // Any value in the existing configuration that isn't specified in |config|
  // is preserved. |config| must not contain host_id or xmpp_login values,
  // because implementations of this method cannot change them.
  virtual void UpdateConfig(scoped_ptr<base::DictionaryValue> config,
                            const CompletionCallback& done_callback) = 0;

  // Stop the daemon process. It is permitted to call Stop while the daemon
  // process is being installed, in which case the installation should be
  // aborted if possible; if not then it is sufficient to ensure that the
  // daemon process is not started automatically upon successful installation.
  // As with Start, Stop may return before the operation is complete--poll
  // GetState until the state is STATE_STOPPED.
  virtual void Stop(const CompletionCallback& done_callback) = 0;

  // Caches the native handle of the plugin window so it can be used to focus
  // elevation prompts properly.
  virtual void SetWindow(void* window_handle) = 0;

  // Get the version of the daemon as a dotted decimal string of the form
  // major.minor.build.patch, if it is installed, or "" otherwise.
  virtual void GetVersion(const GetVersionCallback& done_callback) = 0;

  static scoped_ptr<DaemonController> Create();
};

}  // namespace remoting

#endif  // REMOTING_HOST_DAEMON_CONTROLLER_H_
