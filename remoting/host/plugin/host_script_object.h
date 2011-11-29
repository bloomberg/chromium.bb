// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/time.h"
#include "remoting/base/plugin_message_loop_proxy.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/log_to_server.h"
#include "remoting/host/plugin/host_plugin_utils.h"
#include "remoting/host/ui_strings.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npfunctions.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace remoting {

class ChromotingHost;
class DesktopEnvironment;
class MutableHostConfig;
class RegisterSupportHostRequest;
class SignalStrategy;
class SupportAccessVerifier;

namespace policy_hack {
class NatPolicy;
}  // namespace policy_hack

// NPAPI plugin implementation for remoting host script object.
// HostNPScriptObject creates threads that are required to run
// ChromotingHost and starts/stops the host on those threads. When
// destroyed it sychronously shuts down the host and all threads.
class HostNPScriptObject : public HostStatusObserver {
 public:
  HostNPScriptObject(NPP plugin, NPObject* parent,
                     PluginMessageLoopProxy::Delegate* plugin_thread_delegate);
  virtual ~HostNPScriptObject();

  bool Init();

  bool HasMethod(const std::string& method_name);
  bool InvokeDefault(const NPVariant* args,
                     uint32_t argCount,
                     NPVariant* result);
  bool Invoke(const std::string& method_name,
              const NPVariant* args,
              uint32_t argCount,
              NPVariant* result);
  bool HasProperty(const std::string& property_name);
  bool GetProperty(const std::string& property_name, NPVariant* result);
  bool SetProperty(const std::string& property_name, const NPVariant* value);
  bool RemoveProperty(const std::string& property_name);
  bool Enumerate(std::vector<std::string>* values);

  // remoting::HostStatusObserver implementation.
  virtual void OnSignallingConnected(remoting::SignalStrategy* signal_strategy,
                                     const std::string& full_jid) OVERRIDE;
  virtual void OnSignallingDisconnected() OVERRIDE;
  virtual void OnAccessDenied() OVERRIDE;
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

  // Post LogDebugInfo to the correct proxy (and thus, on the correct thread).
  // This should only be called by HostLogHandler. To log to the UI, use the
  // standard LOG(INFO) and it will be sent to this method.
  void PostLogDebugInfo(const std::string& message);

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

  // Start connection. args are:
  //   string uid, string auth_token
  // No result.
  bool Connect(const NPVariant* args, uint32_t argCount, NPVariant* result);

  // Disconnect. No arguments or result.
  bool Disconnect(const NPVariant* args, uint32_t argCount, NPVariant* result);

  // Localize strings. args are:
  //   localize_func - a callback function which returns a localized string for
  //   a given tag name.
  // No result.
  bool Localize(const NPVariant* args, uint32_t argCount, NPVariant* result);

  // Updates state of the host. Can be called only on the main thread.
  void SetState(State state);

  // Notifies OnStateChanged handler of a state change.
  void NotifyStateChanged(State state);

  // Call LogDebugInfo handler if there is one.
  // This must be called on the correct thread.
  void LogDebugInfo(const std::string& message);

  // Callbacks invoked during session setup.
  void OnReceivedSupportID(bool success,
                           const std::string& support_id,
                           const base::TimeDelta& lifetime);
  void NotifyAccessCode(bool success);

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

  // Called when the nat traversal policy is updated.
  void OnNatPolicyUpdate(bool nat_traversal_enabled);

  void LocalizeStrings(NPObject* localize_func);

  // Helper function for executing InvokeDefault on an NPObject that performs
  // a string->string mapping with one optional substitution parameter. Stores
  // the translation in |result| and returns true on success, or leaves it
  // unchanged and returns false on failure.
  bool LocalizeString(NPObject* localize_func, const char* tag,
                      string16* result);

  // If the web-app has registered a callback to be notified of changes to the
  // NAT traversal policy, notify it.
  void UpdateWebappNatPolicy(bool nat_traversal_enabled);

  // Helper function for executing InvokeDefault on an NPObject, and ignoring
  // the return value.
  bool InvokeAndIgnoreResult(NPObject* func,
                             const NPVariant* args,
                             uint32_t argCount);

  // Set an exception for the current call.
  void SetException(const std::string& exception_string);

  NPP plugin_;
  NPObject* parent_;
  State state_;

  base::Lock access_code_lock_;
  std::string access_code_;
  base::TimeDelta access_code_lifetime_;

  std::string client_username_;
  ScopedRefNPObject log_debug_info_func_;
  ScopedRefNPObject on_nat_traversal_policy_changed_func_;
  ScopedRefNPObject on_state_changed_func_;
  base::PlatformThreadId np_thread_id_;
  scoped_refptr<PluginMessageLoopProxy> plugin_message_loop_proxy_;

  scoped_ptr<RegisterSupportHostRequest> register_request_;
  scoped_ptr<LogToServer> log_to_server_;
  scoped_refptr<MutableHostConfig> host_config_;
  ChromotingHostContext host_context_;
  scoped_ptr<DesktopEnvironment> desktop_environment_;

  scoped_refptr<ChromotingHost> host_;
  int failed_login_attempts_;

  UiStrings ui_strings_;
  base::Lock ui_strings_lock_;

  base::WaitableEvent disconnected_event_;

  // True if we're in the middle of handling a log message.
  bool am_currently_logging_;

  base::Lock nat_policy_lock_;

  scoped_ptr<policy_hack::NatPolicy> nat_policy_;

  // Host the current nat traversal policy setting.
  bool nat_traversal_enabled_;

  // Indicates whether or not a policy has ever been read. This is to ensure
  // that on startup, we do not accidentally start a connection before we have
  // queried our policy restrictions.
  bool policy_received_;

  // Whether logging to a server is enabled.
  bool enable_log_to_server_;

  // On startup, it is possible to have Connect() called before the policy read
  // is completed.  Rather than just failing, we thunk the connection call so
  // it can be executed after at least one successful policy read. This
  // variable contains the thunk if it is necessary.
  base::Closure pending_connect_;
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::HostNPScriptObject);

#endif  // REMOTING_HOST_PLUGIN_HOST_SCRIPT_OBJECT_H_
