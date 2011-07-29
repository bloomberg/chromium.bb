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
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/plugin/host_plugin_logger.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npfunctions.h"
#include "third_party/npapi/bindings/npruntime.h"

class Task;

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace remoting {

class ChromotingHost;
class DesktopEnvironment;
class MutableHostConfig;
class RegisterSupportHostRequest;
class SignalStrategy;
class SupportAccessVerifier;

// NPAPI plugin implementation for remoting host script object.
// HostNPScriptObject creates threads that are required to run
// ChromotingHost and starts/stops the host on those threads. When
// destroyed it sychronously shuts down the host and all threads.
class HostNPScriptObject : public HostStatusObserver {
 public:
  HostNPScriptObject(NPP plugin, NPObject* parent);
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

  // Call LogDebugInfo handler if there is one.
  void LogDebugInfo(const std::string& message);

  // remoting::HostStatusObserver implementation.
  virtual void OnSignallingConnected(remoting::SignalStrategy* signal_strategy,
                                     const std::string& full_jid) OVERRIDE;
  virtual void OnSignallingDisconnected() OVERRIDE;
  virtual void OnAccessDenied() OVERRIDE;
  virtual void OnAuthenticatedClientsChanged(int clients_connected) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

 private:
  enum State {
    kDisconnected,
    kRequestedAccessCode,
    kReceivedAccessCode,
    kConnected,
    kAffirmingConnection,
    kError
  };

  // Start connection. args are:
  //   string uid, string auth_token
  // No result.
  bool Connect(const NPVariant* args, uint32_t argCount, NPVariant* result);

  // Disconnect. No arguments or result.
  bool Disconnect(const NPVariant* args, uint32_t argCount, NPVariant* result);

  // Call OnStateChanged handler if there is one.
  void OnStateChanged(State state);

  // Callbacks invoked during session setup.
  void OnReceivedSupportID(remoting::SupportAccessVerifier* access_verifier,
                           bool success,
                           const std::string& support_id,
                           const base::TimeDelta& lifetime);

  // Helper functions that run on main thread. Can be called on any
  // other thread.
  void ConnectInternal(const std::string& uid,
                       const std::string& auth_token,
                       const std::string& auth_service);
  void DisconnectInternal();

  // Callback for ChromotingHost::Shutdown().
  void OnShutdownFinished();

  // Helper function for executing InvokeDefault on an NPObject, and ignoring
  // the return value.
  bool InvokeAndIgnoreResult(NPObject* func,
                             const NPVariant* args,
                             uint32_t argCount);

  // Posts a task on the main NP thread.
  void PostTaskToNPThread(const tracked_objects::Location& from_here,
                          Task* task);

  // Utility function for PostTaskToNPThread.
  static void NPTaskSpringboard(void* task);

  // Set an exception for the current call.
  void SetException(const std::string& exception_string);

  NPP plugin_;
  NPObject* parent_;
  int state_;
  std::string access_code_;
  base::TimeDelta access_code_lifetime_;
  NPObject* log_debug_info_func_;
  NPObject* on_state_changed_func_;
  base::PlatformThreadId np_thread_id_;

  scoped_ptr<HostPluginLogger> logger_;

  scoped_ptr<RegisterSupportHostRequest> register_request_;
  scoped_refptr<MutableHostConfig> host_config_;
  ChromotingHostContext host_context_;
  scoped_ptr<DesktopEnvironment> desktop_environment_;

  scoped_refptr<ChromotingHost> host_;
  int failed_login_attempts_;

  base::WaitableEvent disconnected_event_;
  base::CancellationFlag destructing_;
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::HostNPScriptObject);

#endif  // REMOTING_HOST_HOST_SCRIPT_OBJECT_H_
