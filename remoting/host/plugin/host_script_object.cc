// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/host_script_object.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "remoting/base/auth_token_util.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/in_memory_host_config.h"
#include "remoting/host/plugin/host_plugin_utils.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/host/support_access_verifier.h"

namespace remoting {

// Supported Javascript interface:
// readonly attribute string accessCode;
// readonly attribute int accessCodeLifetime;
// readonly attribute string client;
// readonly attribute int state;
//
// state: {
//     DISCONNECTED,
//     REQUESTED_ACCESS_CODE,
//     RECEIVED_ACCESS_CODE,
//     CONNECTED,
//     AFFIRMING_CONNECTION,
//     ERROR,
// }
//
// attribute Function void logDebugInfo(string);
// attribute Function void onStateChanged();
//
// // The |auth_service_with_token| parameter should be in the format
// // "auth_service:auth_token".  An example would be "oauth2:1/2a3912vd".
// void connect(string uid, string auth_service_with_token);
// void disconnect();

namespace {

const char* kAttrNameAccessCode = "accessCode";
const char* kAttrNameAccessCodeLifetime = "accessCodeLifetime";
const char* kAttrNameClient = "client";
const char* kAttrNameState = "state";
const char* kAttrNameLogDebugInfo = "logDebugInfo";
const char* kAttrNameOnStateChanged = "onStateChanged";
const char* kFuncNameConnect = "connect";
const char* kFuncNameDisconnect = "disconnect";

// States.
const char* kAttrNameDisconnected = "DISCONNECTED";
const char* kAttrNameRequestedAccessCode = "REQUESTED_ACCESS_CODE";
const char* kAttrNameReceivedAccessCode = "RECEIVED_ACCESS_CODE";
const char* kAttrNameConnected = "CONNECTED";
const char* kAttrNameAffirmingConnection = "AFFIRMING_CONNECTION";
const char* kAttrNameError = "ERROR";

const int kMaxLoginAttempts = 5;

}  // namespace

HostNPScriptObject::HostNPScriptObject(NPP plugin, NPObject* parent)
    : plugin_(plugin),
      parent_(parent),
      state_(kDisconnected),
      log_debug_info_func_(NULL),
      on_state_changed_func_(NULL),
      np_thread_id_(base::PlatformThread::CurrentId()),
      failed_login_attempts_(0),
      disconnected_event_(true, false) {
  logger_.reset(new HostPluginLogger(this));
  logger_->VLog(2, "HostNPScriptObject");
  host_context_.SetUITaskPostFunction(base::Bind(
      &HostNPScriptObject::PostTaskToNPThread, base::Unretained(this)));
}

HostNPScriptObject::~HostNPScriptObject() {
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);

  // Shutdown DesktopEnvironment first so that it doesn't try to post
  // tasks on the UI thread while we are stopping the host.
  desktop_environment_->Shutdown();

  // Disconnect synchronously. We cannot disconnect asynchronously
  // here because |host_context_| needs to be stopped on the plugin
  // thread, but the plugin thread may not exist after the instance
  // is destroyed.
  destructing_.Set();
  disconnected_event_.Reset();
  DisconnectInternal();
  disconnected_event_.Wait();

  // Stop all threads.
  host_context_.Stop();

  if (log_debug_info_func_) {
    g_npnetscape_funcs->releaseobject(log_debug_info_func_);
  }
  if (on_state_changed_func_) {
    g_npnetscape_funcs->releaseobject(on_state_changed_func_);
  }
}

bool HostNPScriptObject::Init() {
  logger_->VLog(2, "Init");
  // TODO(wez): This starts a bunch of threads, which might fail.
  host_context_.Start();
  return true;
}

bool HostNPScriptObject::HasMethod(const std::string& method_name) {
  logger_->VLog(2, "HasMethod %s", method_name.c_str());
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  return (method_name == kFuncNameConnect ||
          method_name == kFuncNameDisconnect);
}

bool HostNPScriptObject::InvokeDefault(const NPVariant* args,
                                       uint32_t argCount,
                                       NPVariant* result) {
  logger_->VLog(2, "InvokeDefault");
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  SetException("exception during default invocation");
  return false;
}

bool HostNPScriptObject::Invoke(const std::string& method_name,
                                const NPVariant* args,
                                uint32_t argCount,
                                NPVariant* result) {
  logger_->VLog(2, "Invoke %s", method_name.c_str());
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  if (method_name == kFuncNameConnect) {
    return Connect(args, argCount, result);
  } else if (method_name == kFuncNameDisconnect) {
    return Disconnect(args, argCount, result);
  } else {
    SetException("Invoke: unknown method " + method_name);
    return false;
  }
}

bool HostNPScriptObject::HasProperty(const std::string& property_name) {
  logger_->VLog(2, "HasProperty %s", property_name.c_str());
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  return (property_name == kAttrNameAccessCode ||
          property_name == kAttrNameAccessCodeLifetime ||
          property_name == kAttrNameClient ||
          property_name == kAttrNameState ||
          property_name == kAttrNameLogDebugInfo ||
          property_name == kAttrNameOnStateChanged ||
          property_name == kAttrNameDisconnected ||
          property_name == kAttrNameRequestedAccessCode ||
          property_name == kAttrNameReceivedAccessCode ||
          property_name == kAttrNameConnected ||
          property_name == kAttrNameAffirmingConnection ||
          property_name == kAttrNameError);
}

bool HostNPScriptObject::GetProperty(const std::string& property_name,
                                     NPVariant* result) {
  logger_->VLog(2, "GetProperty %s", property_name.c_str());
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  if (!result) {
    SetException("GetProperty: NULL result");
    return false;
  }

  if (property_name == kAttrNameOnStateChanged) {
    OBJECT_TO_NPVARIANT(on_state_changed_func_, *result);
    return true;
  } else if (property_name == kAttrNameLogDebugInfo) {
    OBJECT_TO_NPVARIANT(log_debug_info_func_, *result);
    return true;
  } else if (property_name == kAttrNameState) {
    INT32_TO_NPVARIANT(state_, *result);
    return true;
  } else if (property_name == kAttrNameAccessCode) {
    *result = NPVariantFromString(access_code_);
    return true;
  } else if (property_name == kAttrNameAccessCodeLifetime) {
    INT32_TO_NPVARIANT(access_code_lifetime_.InSeconds(), *result);
    return true;
  } else if (property_name == kAttrNameClient) {
    *result = NPVariantFromString(client_username_);
    return true;
  } else if (property_name == kAttrNameDisconnected) {
    INT32_TO_NPVARIANT(kDisconnected, *result);
    return true;
  } else if (property_name == kAttrNameRequestedAccessCode) {
    INT32_TO_NPVARIANT(kRequestedAccessCode, *result);
    return true;
  } else if (property_name == kAttrNameReceivedAccessCode) {
    INT32_TO_NPVARIANT(kReceivedAccessCode, *result);
    return true;
  } else if (property_name == kAttrNameConnected) {
    INT32_TO_NPVARIANT(kConnected, *result);
    return true;
  } else if (property_name == kAttrNameAffirmingConnection) {
    INT32_TO_NPVARIANT(kAffirmingConnection, *result);
    return true;
  } else if (property_name == kAttrNameError) {
    INT32_TO_NPVARIANT(kError, *result);
    return true;
  } else {
    SetException("GetProperty: unsupported property " + property_name);
    return false;
  }
}

bool HostNPScriptObject::SetProperty(const std::string& property_name,
                                     const NPVariant* value) {
  logger_->VLog(2, "SetProperty %s", property_name.c_str());
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);

  if (property_name == kAttrNameOnStateChanged) {
    if (NPVARIANT_IS_OBJECT(*value)) {
      if (on_state_changed_func_) {
        g_npnetscape_funcs->releaseobject(on_state_changed_func_);
      }
      on_state_changed_func_ = NPVARIANT_TO_OBJECT(*value);
      if (on_state_changed_func_) {
        g_npnetscape_funcs->retainobject(on_state_changed_func_);
      }
      return true;
    } else {
      SetException("SetProperty: unexpected type for property " +
                   property_name);
    }
    return false;
  }

  if (property_name == kAttrNameLogDebugInfo) {
    if (NPVARIANT_IS_OBJECT(*value)) {
      if (log_debug_info_func_) {
        g_npnetscape_funcs->releaseobject(log_debug_info_func_);
      }
      log_debug_info_func_ = NPVARIANT_TO_OBJECT(*value);
      if (log_debug_info_func_) {
        g_npnetscape_funcs->retainobject(log_debug_info_func_);
      }
      return true;
    } else {
      SetException("SetProperty: unexpected type for property " +
                   property_name);
    }
    return false;
  }

  return false;
}

bool HostNPScriptObject::RemoveProperty(const std::string& property_name) {
  logger_->VLog(2, "RemoveProperty %s", property_name.c_str());
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  return false;
}

bool HostNPScriptObject::Enumerate(std::vector<std::string>* values) {
  logger_->VLog(2, "Enumerate");
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  const char* entries[] = {
    kAttrNameAccessCode,
    kAttrNameState,
    kAttrNameLogDebugInfo,
    kAttrNameOnStateChanged,
    kFuncNameConnect,
    kFuncNameDisconnect,
    kAttrNameDisconnected,
    kAttrNameRequestedAccessCode,
    kAttrNameReceivedAccessCode,
    kAttrNameConnected,
    kAttrNameAffirmingConnection,
    kAttrNameError
  };
  for (size_t i = 0; i < arraysize(entries); ++i) {
    values->push_back(entries[i]);
  }
  return true;
}

void HostNPScriptObject::OnSignallingConnected(SignalStrategy* signal_strategy,
                                               const std::string& full_jid) {
}

void HostNPScriptObject::OnSignallingDisconnected() {
}

void HostNPScriptObject::OnAccessDenied() {
  DCHECK_EQ(MessageLoop::current(), host_context_.network_message_loop());

  ++failed_login_attempts_;
  if (failed_login_attempts_ == kMaxLoginAttempts)
    DisconnectInternal();
}

void HostNPScriptObject::OnClientAuthenticated(
    remoting::protocol::ConnectionToClient* client) {
  DCHECK_NE(base::PlatformThread::CurrentId(), np_thread_id_);
  client_username_ = client->session()->jid();
  size_t pos = client_username_.find('/');
  if (pos != std::string::npos)
    client_username_.replace(pos, std::string::npos, "");
  LOG(INFO) << "Client " << client_username_ << " connected.";
  OnStateChanged(kConnected);
}

void HostNPScriptObject::OnClientDisconnected(
    remoting::protocol::ConnectionToClient* client) {
  client_username_.clear();
  OnStateChanged(kDisconnected);
}

void HostNPScriptObject::OnShutdown() {
  DCHECK_EQ(MessageLoop::current(), host_context_.main_message_loop());

  OnStateChanged(kDisconnected);
}

// string uid, string auth_token
bool HostNPScriptObject::Connect(const NPVariant* args,
                                 uint32_t arg_count,
                                 NPVariant* result) {
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);

  LogDebugInfo("Connecting...");

  if (arg_count != 2) {
    SetException("connect: bad number of arguments");
    return false;
  }

  std::string uid = StringFromNPVariant(args[0]);
  if (uid.empty()) {
    SetException("connect: bad uid argument");
    return false;
  }

  std::string auth_service_with_token = StringFromNPVariant(args[1]);
  std::string auth_token;
  std::string auth_service;
  ParseAuthTokenWithService(auth_service_with_token, &auth_token,
                            &auth_service);
  if (auth_token.empty()) {
    SetException("connect: auth_service_with_token argument has empty token");
    return false;
  }

  ConnectInternal(uid, auth_token, auth_service);

  return true;
}

void HostNPScriptObject::ConnectInternal(
    const std::string& uid,
    const std::string& auth_token,
    const std::string& auth_service) {
  if (MessageLoop::current() != host_context_.main_message_loop()) {
    host_context_.main_message_loop()->PostTask(
        FROM_HERE, base::Bind(
            &HostNPScriptObject::ConnectInternal, base::Unretained(this),
            uid, auth_token, auth_service));
    return;
  }
  // Store the supplied user ID and token to the Host configuration.
  scoped_refptr<MutableHostConfig> host_config = new InMemoryHostConfig();
  host_config->SetString(kXmppLoginConfigPath, uid);
  host_config->SetString(kXmppAuthTokenConfigPath, auth_token);
  host_config->SetString(kXmppAuthServiceConfigPath, auth_service);

  // Create an access verifier and fetch the host secret.
  scoped_ptr<SupportAccessVerifier> access_verifier;
  access_verifier.reset(new SupportAccessVerifier());

  // Generate a key pair for the Host to use.
  // TODO(wez): Move this to the worker thread.
  HostKeyPair host_key_pair;
  host_key_pair.Generate();
  host_key_pair.Save(host_config);

  // Request registration of the host for support.
  scoped_ptr<RegisterSupportHostRequest> register_request(
      new RegisterSupportHostRequest());
  if (!register_request->Init(
          host_config.get(),
          base::Bind(&HostNPScriptObject::OnReceivedSupportID,
                     base::Unretained(this),
                     access_verifier.get()))) {
    OnStateChanged(kDisconnected);
    return;
  }

  // Nothing went wrong, so lets save the host, config and request.
  host_config_ = host_config;
  register_request_.reset(register_request.release());

  // Create DesktopEnvironment.
  desktop_environment_.reset(DesktopEnvironment::Create(&host_context_));

  // Create the Host.
  // TODO(sergeyu): Use firewall traversal policy settings here.
  host_ = ChromotingHost::Create(
      &host_context_, host_config_, desktop_environment_.get(),
      access_verifier.release(), logger_.get(), false);
  host_->AddStatusObserver(this);
  host_->AddStatusObserver(register_request_.get());
  host_->set_it2me(true);

  // Start the Host.
  host_->Start();

  OnStateChanged(kRequestedAccessCode);
  return;
}

bool HostNPScriptObject::Disconnect(const NPVariant* args,
                                    uint32_t arg_count,
                                    NPVariant* result) {
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  if (arg_count != 0) {
    SetException("disconnect: bad number of arguments");
    return false;
  }

  DisconnectInternal();

  return true;
}

void HostNPScriptObject::DisconnectInternal() {
  if (MessageLoop::current() != host_context_.main_message_loop()) {
    host_context_.main_message_loop()->PostTask(
        FROM_HERE, base::Bind(&HostNPScriptObject::DisconnectInternal,
                              base::Unretained(this)));
    return;
  }

  if (!host_) {
    disconnected_event_.Signal();
    return;
  }

  host_->Shutdown(
      NewRunnableMethod(this, &HostNPScriptObject::OnShutdownFinished));
}

void HostNPScriptObject::OnShutdownFinished() {
  DCHECK_EQ(MessageLoop::current(), host_context_.main_message_loop());

  host_ = NULL;
  register_request_.reset();
  host_config_ = NULL;
  disconnected_event_.Signal();
}

void HostNPScriptObject::OnReceivedSupportID(
    SupportAccessVerifier* access_verifier,
    bool success,
    const std::string& support_id,
    const base::TimeDelta& lifetime) {
  CHECK_NE(base::PlatformThread::CurrentId(), np_thread_id_);

  if (!success) {
    // TODO(wez): Replace the success/fail flag with full error reporting.
    DisconnectInternal();
    return;
  }

  // Inform the AccessVerifier of our Support-Id, for authentication.
  access_verifier->OnIT2MeHostRegistered(success, support_id);

  // Combine the Support Id with the Host Id to make the Access Code.
  // TODO(wez): Locking, anyone?
  access_code_ = support_id + access_verifier->host_secret();
  access_code_lifetime_ = lifetime;

  // Tell the ChromotingHost the access code, to use as shared-secret.
  host_->set_access_code(access_code_);

  // Let the caller know that life is good.
  OnStateChanged(kReceivedAccessCode);
}

void HostNPScriptObject::OnStateChanged(State state) {
  if (destructing_.IsSet())
    return;

  if (!host_context_.IsUIThread()) {
    host_context_.PostTaskToUIThread(
        FROM_HERE, base::Bind(&HostNPScriptObject::OnStateChanged,
                              base::Unretained(this), state));
    return;
  }
  state_ = state;
  if (on_state_changed_func_) {
    logger_->VLog(2, "Calling state changed %s", state);
    bool is_good = InvokeAndIgnoreResult(on_state_changed_func_, NULL, 0);
    LOG_IF(ERROR, !is_good) << "OnStateChanged failed";
  }
}

void HostNPScriptObject::LogDebugInfo(const std::string& message) {
  if (destructing_.IsSet())
    return;

  if (!host_context_.IsUIThread()) {
    host_context_.PostTaskToUIThread(
        FROM_HERE, base::Bind(&HostNPScriptObject::LogDebugInfo,
                              base::Unretained(this), message));
    return;
  }
  if (log_debug_info_func_) {
    NPVariant* arg = new NPVariant();
    STRINGZ_TO_NPVARIANT(message.c_str(), *arg);
    bool is_good = InvokeAndIgnoreResult(log_debug_info_func_, arg, 1);
    LOG_IF(ERROR, !is_good) << "LogDebugInfo failed";
  }
}

void HostNPScriptObject::SetException(const std::string& exception_string) {
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  g_npnetscape_funcs->setexception(parent_, exception_string.c_str());
  LogDebugInfo(exception_string);
}

bool HostNPScriptObject::InvokeAndIgnoreResult(NPObject* func,
                                               const NPVariant* args,
                                               uint32_t argCount) {
  NPVariant np_result;
  bool is_good = g_npnetscape_funcs->invokeDefault(plugin_, func, args,
                                                   argCount, &np_result);
  if (is_good)
      g_npnetscape_funcs->releasevariantvalue(&np_result);
  return is_good;
}

void HostNPScriptObject::PostTaskToNPThread(
    const tracked_objects::Location& from_here, const base::Closure& task) {
  // The NPAPI functions cannot make use of |from_here|, but this method is
  // passed as a callback to ChromotingHostContext, so it needs to have the
  // appropriate signature.

  // Copy task to the heap so that we can pass it to NPTaskSpringboard().
  base::Closure* task_in_heap = new base::Closure(task);

  // Can be called from any thread.
  g_npnetscape_funcs->pluginthreadasynccall(plugin_, &NPTaskSpringboard,
                                            task_in_heap);
}

// static
void HostNPScriptObject::NPTaskSpringboard(void* task) {
  base::Closure* real_task = reinterpret_cast<base::Closure*>(task);
  real_task->Run();
  delete real_task;
}

}  // namespace remoting
