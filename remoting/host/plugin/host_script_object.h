// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PLUGIN_HOST_SCRIPT_OBJECT_H_
#define REMOTING_HOST_PLUGIN_HOST_SCRIPT_OBJECT_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/log_to_server.h"
#include "remoting/host/plugin/host_plugin_utils.h"
#include "remoting/host/setup/daemon_controller.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "remoting/protocol/pairing_registry.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npfunctions.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace remoting {

class RegisterSupportHostRequest;
class HostNPScriptObject;
class DesktopEnvironmentFactory;
class HostEventLogger;
class ChromotingHost;

namespace policy_hack {

class PolicyWatcher;

}  // namespace policy_hack

// These state values are duplicated in host_session.js. Remember to update
// both copies when making changes.
enum It2MeHostState {
  kDisconnected,
  kStarting,
  kRequestedAccessCode,
  kReceivedAccessCode,
  kConnected,
  kDisconnecting,
  kError,
  kInvalidDomainError
};

// Internal implementation of the plugin's It2Me host function.
class It2MeImpl
    : public base::RefCountedThreadSafe<It2MeImpl>,
      public HostStatusObserver {
 public:

  class Observer {
   public:
    virtual void OnClientAuthenticated(const std::string& client_username) = 0;
    virtual void OnStoreAccessCode(const std::string& access_code,
                                   base::TimeDelta access_code_lifetime) = 0;
    virtual void OnNatPolicyChanged(bool nat_traversal_enabled) = 0;
    virtual void OnStateChanged(It2MeHostState state) = 0;
  };

  It2MeImpl(
      scoped_ptr<ChromotingHostContext> context,
      scoped_refptr<base::SingleThreadTaskRunner> plugin_task_runner,
      base::WeakPtr<It2MeImpl::Observer> observer,
      const XmppSignalStrategy::XmppServerConfig& xmpp_server_config,
      const std::string& directory_bot_jid);

  // Methods called by the script object, from the plugin thread.

  // Creates It2Me host structures and starts the host.
  void Connect();

  // Disconnects the host, ready for tear-down.
  // Also called internally, from the network thread.
  void Disconnect();

  // Request a NAT policy notification.
  void RequestNatPolicy();

  // remoting::HostStatusObserver implementation.
  virtual void OnAccessDenied(const std::string& jid) OVERRIDE;
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<It2MeImpl>;

  virtual ~It2MeImpl();

  // Updates state of the host. Can be called only on the network thread.
  void SetState(It2MeHostState state);

  // Returns true if the host is connected.
  bool IsConnected() const;

  // Called by Connect() to check for policies and start connection process.
  void ReadPolicyAndConnect();

  // Called by ReadPolicyAndConnect once policies have been read.
  void FinishConnect();

  // Called when the support host registration completes.
  void OnReceivedSupportID(bool success,
                           const std::string& support_id,
                           const base::TimeDelta& lifetime);

  // Shuts down |host_| on the network thread and posts ShutdownOnUiThread()
  // to shut down UI thread resources.
  void ShutdownOnNetworkThread();

  // Shuts down |desktop_environment_factory_| and |policy_watcher_| on
  // the UI thread.
  void ShutdownOnUiThread();

  // Called when initial policies are read, and when they change.
  void OnPolicyUpdate(scoped_ptr<base::DictionaryValue> policies);

  // Handlers for NAT traversal and host domain policies.
  void UpdateNatPolicy(bool nat_traversal_enabled);
  void UpdateHostDomainPolicy(const std::string& host_domain);

  // Caller supplied fields.
  scoped_ptr<ChromotingHostContext> host_context_;
  scoped_refptr<base::SingleThreadTaskRunner> plugin_task_runner_;
  base::WeakPtr<It2MeImpl::Observer> observer_;
  XmppSignalStrategy::XmppServerConfig xmpp_server_config_;
  std::string directory_bot_jid_;

  It2MeHostState state_;

  scoped_refptr<RsaKeyPair> host_key_pair_;
  scoped_ptr<SignalStrategy> signal_strategy_;
  scoped_ptr<RegisterSupportHostRequest> register_request_;
  scoped_ptr<LogToServer> log_to_server_;
  scoped_ptr<DesktopEnvironmentFactory> desktop_environment_factory_;
  scoped_ptr<HostEventLogger> host_event_logger_;

  scoped_ptr<ChromotingHost> host_;
  int failed_login_attempts_;

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

  DISALLOW_COPY_AND_ASSIGN(It2MeImpl);
};

// NPAPI plugin implementation for remoting host script object.
// HostNPScriptObject creates threads that are required to run
// ChromotingHost and starts/stops the host on those threads. When
// destroyed it synchronously shuts down the host and all threads.
class HostNPScriptObject : public It2MeImpl::Observer {
 public:
  HostNPScriptObject(NPP plugin,
                     NPObject* parent,
                     scoped_refptr<AutoThreadTaskRunner> plugin_task_runner);
  virtual ~HostNPScriptObject();

  // Implementations used to implement the NPObject interface.
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

  // Post LogDebugInfo to the correct proxy (and thus, on the correct thread).
  // This should only be called by HostLogHandler. To log to the UI, use the
  // standard LOG(INFO) and it will be sent to this method.
  void PostLogDebugInfo(const std::string& message);

  void SetWindow(NPWindow* np_window);

 private:
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

  // Deletes all paired clients from the registry.
  bool ClearPairedClients(const NPVariant* args,
                          uint32_t arg_count,
                          NPVariant* result);

  // Deletes a paired client referenced by client id.
  bool DeletePairedClient(const NPVariant* args,
                          uint32_t arg_count,
                          NPVariant* result);

  // Fetches the host name, passing it to the supplied callback. Args are:
  //   function(string) callback
  // Returns false if the parameters are invalid.
  bool GetHostName(const NPVariant* args,
                   uint32_t arg_count,
                   NPVariant* result);

  // Calculates PIN hash value to be stored in the config. Args are:
  //   string hostId Host ID.
  //   string pin The PIN.
  //   function(string) callback
  // Passes the resulting hash value base64-encoded to the callback.
  // Returns false if the parameters are invalid.
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

  // Retrieves the list of paired clients as a JSON-encoded string.
  bool GetPairedClients(const NPVariant* args,
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
  // Implementation of It2MeImpl::Observer methods.

  // Notifies OnStateChanged handler of a state change.
  virtual void OnStateChanged(It2MeHostState state) OVERRIDE;

  // If the web-app has registered a callback to be notified of changes to the
  // NAT traversal policy, notify it.
  virtual void OnNatPolicyChanged(bool nat_traversal_enabled) OVERRIDE;

  // Stores the Access Code for the web-app to query.
  virtual void OnStoreAccessCode(const std::string& access_code,
                                 base::TimeDelta access_code_lifetime) OVERRIDE;

  // Stores the client user's name for the web-app to query.
  virtual void OnClientAuthenticated(
      const std::string& client_username) OVERRIDE;

  // Used to generate localized strings to pass to the It2Me host core.
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

  //////////////////////////////////////////////////////////
  // Helper methods for Me2Me host.

  // Helpers for GenerateKeyPair().
  static void DoGenerateKeyPair(
      const scoped_refptr<AutoThreadTaskRunner>& plugin_task_runner,
      const base::Callback<void (const std::string&,
                                 const std::string&)>& callback);
  void InvokeGenerateKeyPairCallback(scoped_ptr<ScopedRefNPObject> callback,
                                     const std::string& private_key,
                                     const std::string& public_key);

  // Callback handler for SetConfigAndStart(), Stop(), SetPin() and
  // SetUsageStatsConsent() in DaemonController.
  void InvokeAsyncResultCallback(scoped_ptr<ScopedRefNPObject> callback,
                                 DaemonController::AsyncResult result);

  // Callback handler for PairingRegistry methods that return a boolean
  // success status.
  void InvokeBooleanCallback(scoped_ptr<ScopedRefNPObject> callback,
                             bool result);

  // Callback handler for DaemonController::GetConfig().
  void InvokeGetDaemonConfigCallback(scoped_ptr<ScopedRefNPObject> callback,
                                     scoped_ptr<base::DictionaryValue> config);

  // Callback handler for DaemonController::GetVersion().
  void InvokeGetDaemonVersionCallback(scoped_ptr<ScopedRefNPObject> callback,
                                      const std::string& version);

  // Callback handler for GetPairedClients().
  void InvokeGetPairedClientsCallback(
      scoped_ptr<ScopedRefNPObject> callback,
      scoped_ptr<base::ListValue> paired_clients);

  // Callback handler for DaemonController::GetUsageStatsConsent().
  void InvokeGetUsageStatsConsentCallback(
      scoped_ptr<ScopedRefNPObject> callback,
      const DaemonController::UsageStatsConsent& consent);

  //////////////////////////////////////////////////////////
  // Basic helper methods used for both It2Me and Me2me.

  // Call LogDebugInfo handler if there is one.
  // This must be called on the correct thread.
  void LogDebugInfo(const std::string& message);

  // Helper function for executing InvokeDefault on an NPObject, and ignoring
  // the return value.
  bool InvokeAndIgnoreResult(const ScopedRefNPObject& func,
                             const NPVariant* args,
                             uint32_t arg_count);

  // Set an exception for the current call.
  void SetException(const std::string& exception_string);

  //////////////////////////////////////////////////////////
  // Plugin state variables shared between It2Me and Me2Me.

  NPP plugin_;
  NPObject* parent_;
  scoped_refptr<AutoThreadTaskRunner> plugin_task_runner_;
  scoped_ptr<base::ThreadTaskRunnerHandle> plugin_task_runner_handle_;

  // True if we're in the middle of handling a log message.
  bool am_currently_logging_;

  ScopedRefNPObject log_debug_info_func_;

  //////////////////////////////////////////////////////////
  // It2Me host state.

  // Internal implementation of the It2Me host function.
  scoped_refptr<It2MeImpl> it2me_impl_;

  // Cached, read-only copies of |it2me_impl_| session state.
  It2MeHostState state_;
  std::string access_code_;
  base::TimeDelta access_code_lifetime_;
  std::string client_username_;

  // IT2Me Talk server configuration used by |it2me_impl_| to connect.
  XmppSignalStrategy::XmppServerConfig xmpp_server_config_;

  // Chromoting Bot JID used by |it2me_impl_| to register the host.
  std::string directory_bot_jid_;

  // Callbacks to notify in response to |it2me_impl_| events.
  ScopedRefNPObject on_nat_traversal_policy_changed_func_;
  ScopedRefNPObject on_state_changed_func_;

  //////////////////////////////////////////////////////////
  // Me2Me host state.

  // Platform-specific installation & configuration implementation.
  scoped_refptr<DaemonController> daemon_controller_;

  // TODO(sergeyu): Replace this thread with
  // SequencedWorkerPool. Problem is that SequencedWorkerPool relies
  // on MessageLoopProxy::current().
  scoped_refptr<AutoThreadTaskRunner> worker_thread_;

  // Used to load and update the paired clients for this host.
  scoped_refptr<protocol::PairingRegistry> pairing_registry_;

  //////////////////////////////////////////////////////////
  // Plugin state used for both Ir2Me and Me2Me.

  // Used to cancel pending tasks for this object when it is destroyed.
  base::WeakPtr<HostNPScriptObject> weak_ptr_;
  base::WeakPtrFactory<HostNPScriptObject> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostNPScriptObject);
};

}  // namespace remoting

#endif  // REMOTING_HOST_PLUGIN_HOST_SCRIPT_OBJECT_H_
