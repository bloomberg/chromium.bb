// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PLUGIN_HOST_SCRIPT_OBJECT_H_
#define REMOTING_HOST_PLUGIN_HOST_SCRIPT_OBJECT_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/string16.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "remoting/base/plugin_thread_task_runner.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/log_to_server.h"
#include "remoting/host/plugin/daemon_controller.h"
#include "remoting/host/plugin/host_plugin_utils.h"
#include "remoting/host/ui_strings.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npfunctions.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace remoting {

class ChromotingHost;
class DesktopEnvironmentFactory;
class HostEventLogger;
class It2MeHostUserInterface;
class MutableHostConfig;
class RegisterSupportHostRequest;
class SignalStrategy;
class SupportAccessVerifier;

namespace policy_hack {
class PolicyWatcher;
}  // namespace policy_hack

// NPAPI plugin implementation for remoting host script object.
// HostNPScriptObject creates threads that are required to run
// ChromotingHost and starts/stops the host on those threads. When
// destroyed it synchronously shuts down the host and all threads.
class HostNPScriptObject : public HostStatusObserver {
 public:
  HostNPScriptObject(NPP plugin, NPObject* parent,
                     PluginThreadTaskRunner::Delegate* plugin_thread_delegate);
  virtual ~HostNPScriptObject();

  bool Init();

  bool HasMethod(const std::string& method_name);
  bool InvokeDefault(const NPVariant* args,
                     uint32_t arg_count,
                     NPVariant* result);
  bool Invoke(const std::string& method_name,
              const NPVariant* args,
              uint32_t arg_count,
              NPVariant* result);
  bool HasProperty(const std::string& property_name);
  bool GetProperty(const std::string& property_name, NPVariant* result);
  bool SetProperty(const std::string& property_name, const NPVariant* value);
  bool RemoveProperty(const std::string& property_name);
  bool Enumerate(std::vector<std::string>* values);

  // remoting::HostStatusObserver implementation.
  virtual void OnAccessDenied(const std::string& jid) OVERRIDE;
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

  // Post LogDebugInfo to the correct proxy (and thus, on the correct thread).
  // This should only be called by HostLogHandler. To log to the UI, use the
  // standard LOG(INFO) and it will be sent to this method.
  void PostLogDebugInfo(const std::string& message);

  void SetWindow(NPWindow* np_window);

 private:
  // These state values are duplicated in the JS code. Remember to update both
  // copies when making changes.
  enum State {
    kDisconnected,
    kStarting,
    kRequestedAccessCode,
    kReceivedAccessCode,
    kConnected,
    kDisconnecting,
    kError
  };

  //////////////////////////////////////////////////////////
  // Plugin methods for It2Me host.

  // Start connection. args are:
  //   string uid, string auth_token
  // No result.
  bool Connect(const NPVariant* args, uint32_t arg_count, NPVariant* result);

  // Disconnect. No arguments or result.
  bool Disconnect(const NPVariant* args, uint32_t arg_count, NPVariant* result);

  // Localize strings. args are:
  //   localize_func - a callback function which returns a localized string for
  //   a given tag name.
  // No result.
  bool Localize(const NPVariant* args, uint32_t arg_count, NPVariant* result);

  //////////////////////////////////////////////////////////
  // Plugin methods for Me2Me host.

  // Returns host name. No arguments.
  bool GetHostName(const NPVariant* args,
                   uint32_t arg_count,
                   NPVariant* result);

  // Calculates PIN hash value to be stored in the config. Args are:
  //   string hostId Host ID.
  //   string pin The PIN.
  // Returns the resulting hash value encoded with Base64.
  bool GetPinHash(const NPVariant* args,
                  uint32_t arg_count,
                  NPVariant* result);

  // Generates new key pair to use for the host. The specified
  // callback is called when when the key is generated. The key is
  // returned in format understood by the host (PublicKeyInfo
  // structure encoded with ASN.1 DER, and then BASE64). Args are:
  //   function(string) callback The callback to be called when done.
  bool GenerateKeyPair(const NPVariant* args,
                       uint32_t arg_count,
                       NPVariant* result);

  // Update host config for Me2Me. Args are:
  //   string config
  //   function(number) done_callback
  bool UpdateDaemonConfig(const NPVariant* args,
                          uint32_t arg_count,
                          NPVariant* result);

  // Loads daemon config. The first argument specifies the callback to be
  // called once the config has been loaded. The config is passed as a JSON
  // formatted string. Args are:
  //   function(string) callback
  bool GetDaemonConfig(const NPVariant* args,
                       uint32_t arg_count,
                       NPVariant* result);

  // Retrieves daemon version. The first argument specifies the callback to be
  // called with the obtained version. The version is passed as a dotted
  // version string, described in daemon_controller.h.
  bool GetDaemonVersion(const NPVariant* args,
                        uint32_t arg_count,
                        NPVariant* result);

  // Retrieves the user's consent to report crash dumps. The first argument
  // specifies the callback to be called with the recorder consent. Possible
  // consent codes are defined in remoting/host/breakpad.h.
  bool GetUsageStatsConsent(const NPVariant* args,
                            uint32_t arg_count,
                            NPVariant* result);

  // Start the daemon process with the specified config. Args are:
  //   string config
  //   function(number) done_callback
  bool StartDaemon(const NPVariant* args,
                   uint32_t arg_count,
                   NPVariant* result);

  // Stop the daemon process. Args are:
  //   function(number) done_callback
  bool StopDaemon(const NPVariant* args, uint32_t arg_count, NPVariant* result);

  //////////////////////////////////////////////////////////
  // Helper methods for It2Me host.

  // Updates state of the host. Can be called only on the main thread.
  void SetState(State state);

  // Notifies OnStateChanged handler of a state change.
  void NotifyStateChanged(State state);

  // Callbacks invoked during session setup.
  void OnReceivedSupportID(bool success,
                           const std::string& support_id,
                           const base::TimeDelta& lifetime);

  // Helper functions that run on main thread. Can be called on any
  // other thread.
  void ReadPolicyAndConnect(const std::string& uid,
                            const std::string& auth_token,
                            const std::string& auth_service);
  void FinishConnect(const std::string& uid,
                     const std::string& auth_token,
                     const std::string& auth_service);

  void DisconnectInternal();

  // Callback for ChromotingHost::Shutdown().
  void OnShutdownFinished();

  // Called when a policy is updated.
  void OnPolicyUpdate(scoped_ptr<base::DictionaryValue> policies);

  // Called when the nat traversal policy is updated.
  void UpdateNatPolicy(bool nat_traversal_enabled);

  // Called when the host domain policy is updated.
  void UpdateHostDomainPolicy(const std::string& host_domain);

  void LocalizeStrings(NPObject* localize_func);

  // Helper function for executing InvokeDefault on an NPObject that performs
  // a string->string mapping without substitution. Stores the translation in
  // |result| and returns true on success, or leaves it unchanged and returns
  // false on failure.
  bool LocalizeString(NPObject* localize_func, const char* tag,
                      string16* result);

  // Helper function for executing InvokeDefault on an NPObject that performs
  // a string->string mapping with one substitution. Stores the translation in
  // |result| and returns true on success, or leaves it unchanged and returns
  // false on failure.
  bool LocalizeStringWithSubstitution(NPObject* localize_func,
                                      const char* tag,
                                      const char* substitution,
                                      string16* result);

  // If the web-app has registered a callback to be notified of changes to the
  // NAT traversal policy, notify it.
  void UpdateWebappNatPolicy(bool nat_traversal_enabled);

  //////////////////////////////////////////////////////////
  // Helper methods for Me2Me host.

  // Helpers for GenerateKeyPair().
  void DoGenerateKeyPair(const ScopedRefNPObject& callback);
  void InvokeGenerateKeyPairCallback(const ScopedRefNPObject& callback,
                                     const std::string& private_key,
                                     const std::string& public_key);


  // Callback handler for SetConfigAndStart(), Stop(), SetPin() and
  // SetUsageStatsConsent() in DaemonController.
  void InvokeAsyncResultCallback(const ScopedRefNPObject& callback,
                                 DaemonController::AsyncResult result);

  // Callback handler for DaemonController::GetConfig().
  void InvokeGetDaemonConfigCallback(const ScopedRefNPObject& callback,
                                     scoped_ptr<base::DictionaryValue> config);

  // Callback handler for DaemonController::GetVersion().
  void InvokeGetDaemonVersionCallback(const ScopedRefNPObject& callback,
                                      const std::string& version);

  // Callback handler for DaemonController::GetUsageStatsConsent().
  void InvokeGetUsageStatsConsentCallback(const ScopedRefNPObject& callback,
                                          bool supported,
                                          bool allowed,
                                          bool set_by_policy);

  //////////////////////////////////////////////////////////
  // Basic helper methods used for both It2Me and Me2me.

  // Call LogDebugInfo handler if there is one.
  // This must be called on the correct thread.
  void LogDebugInfo(const std::string& message);

  // Helper function for executing InvokeDefault on an NPObject, and ignoring
  // the return value.
  bool InvokeAndIgnoreResult(NPObject* func,
                             const NPVariant* args,
                             uint32_t arg_count);

  // Set an exception for the current call.
  void SetException(const std::string& exception_string);

  //////////////////////////////////////////////////////////
  // Plugin state variables shared between It2Me and Me2Me.

  // True if we're in the middle of handling a log message.
  NPP plugin_;
  NPObject* parent_;
  bool am_currently_logging_;

  //////////////////////////////////////////////////////////
  // It2Me host state.
  State state_;

  base::Lock access_code_lock_;
  std::string access_code_;
  base::TimeDelta access_code_lifetime_;

  std::string client_username_;
  ScopedRefNPObject log_debug_info_func_;
  ScopedRefNPObject on_nat_traversal_policy_changed_func_;
  ScopedRefNPObject on_state_changed_func_;
  base::PlatformThreadId np_thread_id_;
  scoped_refptr<PluginThreadTaskRunner> plugin_task_runner_;

  scoped_ptr<ChromotingHostContext> host_context_;
  HostKeyPair host_key_pair_;
  scoped_ptr<SignalStrategy> signal_strategy_;
  scoped_ptr<RegisterSupportHostRequest> register_request_;
  scoped_ptr<LogToServer> log_to_server_;
  scoped_ptr<DesktopEnvironmentFactory> desktop_environment_factory_;
  scoped_ptr<It2MeHostUserInterface> it2me_host_user_interface_;
  scoped_ptr<HostEventLogger> host_event_logger_;

  scoped_refptr<ChromotingHost> host_;
  int failed_login_attempts_;

  UiStrings ui_strings_;
  base::Lock ui_strings_lock_;

  base::WaitableEvent disconnected_event_;
  base::WaitableEvent stopped_event_;

  base::Lock nat_policy_lock_;

  scoped_ptr<policy_hack::PolicyWatcher> policy_watcher_;

  // Host the current nat traversal policy setting.
  bool nat_traversal_enabled_;

  // The host domain policy setting.
  std::string required_host_domain_;

  // Indicates whether or not a policy has ever been read. This is to ensure
  // that on startup, we do not accidentally start a connection before we have
  // queried our policy restrictions.
  bool policy_received_;

  // On startup, it is possible to have Connect() called before the policy read
  // is completed.  Rather than just failing, we thunk the connection call so
  // it can be executed after at least one successful policy read. This
  // variable contains the thunk if it is necessary.
  base::Closure pending_connect_;

  //////////////////////////////////////////////////////////
  // Me2Me host state.
  scoped_ptr<DaemonController> daemon_controller_;

  // TODO(sergeyu): Replace this thread with
  // SequencedWorkerPool. Problem is that SequencedWorkerPool relies
  // on MessageLoopProxy::current().
  base::Thread worker_thread_;

  DISALLOW_COPY_AND_ASSIGN(HostNPScriptObject);
};

}  // namespace remoting

#endif  // REMOTING_HOST_PLUGIN_HOST_SCRIPT_OBJECT_H_
