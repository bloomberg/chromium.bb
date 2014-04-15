// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_DAEMON_CONTROLLER_H_
#define REMOTING_HOST_SETUP_DAEMON_CONTROLLER_H_

#include <queue>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class AutoThread;
class AutoThreadTaskRunner;

class DaemonController : public base::RefCountedThreadSafe<DaemonController> {
 public:
  // Note that these enumeration values are duplicated in host_controller.js and
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

  struct UsageStatsConsent {
    // Indicates whether crash dump reporting is supported by the host.
    bool supported;

    // Indicates if crash dump reporting is allowed by the user.
    bool allowed;

    // Carries information whether the crash dump reporting is controlled by
    // policy.
    bool set_by_policy;
  };

  // Callback type for GetUsageStatsConsent().
  typedef base::Callback<void (const UsageStatsConsent&)>
      GetUsageStatsConsentCallback;

  // Interface representing the platform-spacific back-end. Most of its methods
  // are blocking and should be called on a background thread. There are two
  // exceptions:
  //   - GetState() is synchronous and called on the UI thread. It should avoid
  //         accessing any data members of the implementation.
  //   - SetConfigAndStart(), UpdateConfig() and Stop() indicate completion via
  //         a callback. There methods can be long running and should be caled
  //         on a background thread.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Return the "installed/running" state of the daemon process. This method
    // should avoid accessing any data members of the implementation.
    virtual State GetState() = 0;

    // Queries current host configuration. Any values that might be security
    // sensitive have been filtered out.
    virtual scoped_ptr<base::DictionaryValue> GetConfig() = 0;

    // Download and install the host component. |done| is invoked on the
    // calling thread when the operation is completed.
    virtual void InstallHost(const CompletionCallback& done) = 0;

    // Starts the daemon process. This may require that the daemon be
    // downloaded and installed. |done| is invoked on the calling thread when
    // the operation is completed.
    virtual void SetConfigAndStart(
        scoped_ptr<base::DictionaryValue> config,
        bool consent,
        const CompletionCallback& done) = 0;

    // Updates current host configuration with the values specified in
    // |config|. Any value in the existing configuration that isn't specified in
    // |config| is preserved. |config| must not contain host_id or xmpp_login
    // values, because implementations of this method cannot change them. |done|
    // is invoked on the calling thread when the operation is completed.
    virtual void UpdateConfig(
        scoped_ptr<base::DictionaryValue> config,
        const CompletionCallback& done) = 0;

    // Stops the daemon process. |done| is invoked on the calling thread when
    // the operation is completed.
    virtual void Stop(const CompletionCallback& done) = 0;

    // Caches the native handle of the plugin window so it can be used to focus
    // elevation prompts properly.
    virtual void SetWindow(void* window_handle) = 0;

    // Get the version of the daemon as a dotted decimal string of the form
    // major.minor.build.patch, if it is installed, or "" otherwise.
    virtual std::string GetVersion() = 0;

    // Get the user's consent to crash reporting.
    virtual UsageStatsConsent GetUsageStatsConsent() = 0;
  };

  static scoped_refptr<DaemonController> Create();

  explicit DaemonController(scoped_ptr<Delegate> delegate);

  // Return the "installed/running" state of the daemon process.
  //
  // TODO(sergeyu): This method is called synchronously from the
  // webapp. In most cases it requires IO operations, so it may block
  // the user interface. Replace it with asynchronous notifications,
  // e.g. with StartStateNotifications()/StopStateNotifications() methods.
  State GetState();

  // Queries current host configuration. The |done| is called
  // after the configuration is read, and any values that might be security
  // sensitive have been filtered out.
  void GetConfig(const GetConfigCallback& done);

  // Download and install the host component. |done| is called when the
  // operation is finished or fails.
  void InstallHost(const CompletionCallback& done);

  // Start the daemon process. This may require that the daemon be
  // downloaded and installed. |done| is called when the
  // operation is finished or fails.
  //
  // TODO(sergeyu): This method writes config and starts the host -
  // these two steps are merged for simplicity. Consider splitting it
  // into SetConfig() and Start() once we have basic host setup flow
  // working.
  void SetConfigAndStart(scoped_ptr<base::DictionaryValue> config,
                         bool consent,
                         const CompletionCallback& done);

  // Updates current host configuration with the values specified in
  // |config|. Changes must take effect before the call completes.
  // Any value in the existing configuration that isn't specified in |config|
  // is preserved. |config| must not contain host_id or xmpp_login values,
  // because implementations of this method cannot change them.
  void UpdateConfig(scoped_ptr<base::DictionaryValue> config,
                    const CompletionCallback& done);

  // Stop the daemon process. It is permitted to call Stop while the daemon
  // process is being installed, in which case the installation should be
  // aborted if possible; if not then it is sufficient to ensure that the
  // daemon process is not started automatically upon successful installation.
  // As with Start, Stop may return before the operation is complete--poll
  // GetState until the state is STATE_STOPPED.
  void Stop(const CompletionCallback& done);

  // Caches the native handle of the plugin window so it can be used to focus
  // elevation prompts properly.
  void SetWindow(void* window_handle);

  // Get the version of the daemon as a dotted decimal string of the form
  // major.minor.build.patch, if it is installed, or "" otherwise.
  void GetVersion(const GetVersionCallback& done);

  // Get the user's consent to crash reporting.
  void GetUsageStatsConsent(const GetUsageStatsConsentCallback& done);

 private:
  friend class base::RefCountedThreadSafe<DaemonController>;
  virtual ~DaemonController();

  // Blocking helper methods used to call the delegate.
  void DoGetConfig(const GetConfigCallback& done);
  void DoInstallHost(const CompletionCallback& done);
  void DoSetConfigAndStart(scoped_ptr<base::DictionaryValue> config,
                           bool consent,
                           const CompletionCallback& done);
  void DoUpdateConfig(scoped_ptr<base::DictionaryValue> config,
                      const CompletionCallback& done);
  void DoStop(const CompletionCallback& done);
  void DoSetWindow(void* window_handle, const base::Closure& done);
  void DoGetVersion(const GetVersionCallback& done);
  void DoGetUsageStatsConsent(const GetUsageStatsConsentCallback& done);

  // "Trampoline" callbacks that schedule the next pending request and then
  // invoke the original caller-supplied callback.
  void InvokeCompletionCallbackAndScheduleNext(
      const CompletionCallback& done,
      AsyncResult result);
  void InvokeConfigCallbackAndScheduleNext(
      const GetConfigCallback& done,
      scoped_ptr<base::DictionaryValue> config);
  void InvokeConsentCallbackAndScheduleNext(
      const GetUsageStatsConsentCallback& done,
      const UsageStatsConsent& consent);
  void InvokeVersionCallbackAndScheduleNext(
      const GetVersionCallback& done,
      const std::string& version);

  // Queue management methods.
  void ScheduleNext();
  void ServiceOrQueueRequest(const base::Closure& request);
  void ServiceNextRequest();

  // Task runner on which all public methods of this class should be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner used to run blocking calls to the delegate. A single thread
  // task runner is used to guarantee that one method of the delegate is
  // called at a time.
  scoped_refptr<AutoThreadTaskRunner> delegate_task_runner_;

  scoped_ptr<AutoThread> delegate_thread_;

  scoped_ptr<Delegate> delegate_;

  std::queue<base::Closure> pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(DaemonController);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_DAEMON_CONTROLLER_H_
