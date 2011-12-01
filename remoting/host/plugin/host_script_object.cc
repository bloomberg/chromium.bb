// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/host_script_object.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/sys_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "remoting/base/auth_token_util.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/host_secret.h"
#include "remoting/host/in_memory_host_config.h"
#include "remoting/host/it2me_host_user_interface.h"
#include "remoting/host/plugin/host_log_handler.h"
#include "remoting/host/plugin/policy_hack/nat_policy.h"
#include "remoting/host/register_support_host_request.h"

namespace remoting {

// Supported Javascript interface:
// readonly attribute string accessCode;
// readonly attribute int accessCodeLifetime;
// readonly attribute string client;
// readonly attribute int state;
//
// state: {
//     DISCONNECTED,
//     STARTING,
//     REQUESTED_ACCESS_CODE,
//     RECEIVED_ACCESS_CODE,
//     CONNECTED,
//     DISCONNECTING,
//     ERROR,
// }
//
// attribute Function void logDebugInfo(string);
// attribute Function void onNatTraversalPolicyChanged(boolean);
// attribute Function void onStateChanged(state);
//
// // The |auth_service_with_token| parameter should be in the format
// // "auth_service:auth_token".  An example would be "oauth2:1/2a3912vd".
// void connect(string uid, string auth_service_with_token);
// void disconnect();
// void localize(string (*localize_func)(string,...));

namespace {

const char* kAttrNameAccessCode = "accessCode";
const char* kAttrNameAccessCodeLifetime = "accessCodeLifetime";
const char* kAttrNameClient = "client";
const char* kAttrNameState = "state";
const char* kAttrNameLogDebugInfo = "logDebugInfo";
const char* kAttrNameOnNatTraversalPolicyChanged =
    "onNatTraversalPolicyChanged";
const char* kAttrNameOnStateChanged = "onStateChanged";
const char* kFuncNameConnect = "connect";
const char* kFuncNameDisconnect = "disconnect";
const char* kFuncNameLocalize = "localize";

// States.
const char* kAttrNameDisconnected = "DISCONNECTED";
const char* kAttrNameStarting = "STARTING";
const char* kAttrNameRequestedAccessCode = "REQUESTED_ACCESS_CODE";
const char* kAttrNameReceivedAccessCode = "RECEIVED_ACCESS_CODE";
const char* kAttrNameConnected = "CONNECTED";
const char* kAttrNameDisconnecting = "DISCONNECTING";
const char* kAttrNameError = "ERROR";

const int kMaxLoginAttempts = 5;

}  // namespace

HostNPScriptObject::HostNPScriptObject(
    NPP plugin,
    NPObject* parent,
    PluginMessageLoopProxy::Delegate* plugin_thread_delegate)
    : plugin_(plugin),
      parent_(parent),
      state_(kDisconnected),
      np_thread_id_(base::PlatformThread::CurrentId()),
      plugin_message_loop_proxy_(
          new PluginMessageLoopProxy(plugin_thread_delegate)),
      host_context_(plugin_message_loop_proxy_),
      failed_login_attempts_(0),
      disconnected_event_(true, false),
      am_currently_logging_(false),
      nat_traversal_enabled_(false),
      policy_received_(false),
      enable_log_to_server_(false) {
}

HostNPScriptObject::~HostNPScriptObject() {
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);

  // Shutdown It2MeHostUserInterface first so that it doesn't try to post
  // tasks on the UI thread while we are stopping the host.
  if (it2me_host_user_interface_.get()) {
    it2me_host_user_interface_->Shutdown();
  }

  HostLogHandler::UnregisterLoggingScriptObject(this);

  plugin_message_loop_proxy_->Detach();

  // Stop listening for policy updates.
  if (nat_policy_.get()) {
    base::WaitableEvent nat_policy_stopped_(true, false);
    nat_policy_->StopWatching(&nat_policy_stopped_);
    nat_policy_stopped_.Wait();
    nat_policy_.reset();
  }

  // Disconnect synchronously. We cannot disconnect asynchronously
  // here because |host_context_| needs to be stopped on the plugin
  // thread, but the plugin thread may not exist after the instance
  // is destroyed.
  disconnected_event_.Reset();
  DisconnectInternal();
  disconnected_event_.Wait();

  // Stop all threads.
  host_context_.Stop();
}

bool HostNPScriptObject::Init() {
  VLOG(2) << "Init";
  // TODO(wez): This starts a bunch of threads, which might fail.
  host_context_.Start();
  nat_policy_.reset(
      policy_hack::NatPolicy::Create(host_context_.network_message_loop()));
  nat_policy_->StartWatching(
      base::Bind(&HostNPScriptObject::OnNatPolicyUpdate,
                 base::Unretained(this)));
  return true;
}

bool HostNPScriptObject::HasMethod(const std::string& method_name) {
  VLOG(2) << "HasMethod " << method_name;
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  return (method_name == kFuncNameConnect ||
          method_name == kFuncNameDisconnect ||
          method_name == kFuncNameLocalize);
}

bool HostNPScriptObject::InvokeDefault(const NPVariant* args,
                                       uint32_t argCount,
                                       NPVariant* result) {
  VLOG(2) << "InvokeDefault";
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  SetException("exception during default invocation");
  return false;
}

bool HostNPScriptObject::Invoke(const std::string& method_name,
                                const NPVariant* args,
                                uint32_t argCount,
                                NPVariant* result) {
  VLOG(2) << "Invoke " << method_name;
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  if (method_name == kFuncNameConnect) {
    return Connect(args, argCount, result);
  } else if (method_name == kFuncNameDisconnect) {
    return Disconnect(args, argCount, result);
  } else if (method_name == kFuncNameLocalize) {
    return Localize(args, argCount, result);
  } else {
    SetException("Invoke: unknown method " + method_name);
    return false;
  }
}

bool HostNPScriptObject::HasProperty(const std::string& property_name) {
  VLOG(2) << "HasProperty " << property_name;
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  return (property_name == kAttrNameAccessCode ||
          property_name == kAttrNameAccessCodeLifetime ||
          property_name == kAttrNameClient ||
          property_name == kAttrNameState ||
          property_name == kAttrNameLogDebugInfo ||
          property_name == kAttrNameOnNatTraversalPolicyChanged ||
          property_name == kAttrNameOnStateChanged ||
          property_name == kAttrNameDisconnected ||
          property_name == kAttrNameStarting ||
          property_name == kAttrNameRequestedAccessCode ||
          property_name == kAttrNameReceivedAccessCode ||
          property_name == kAttrNameConnected ||
          property_name == kAttrNameDisconnecting ||
          property_name == kAttrNameError);
}

bool HostNPScriptObject::GetProperty(const std::string& property_name,
                                     NPVariant* result) {
  VLOG(2) << "GetProperty " << property_name;
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  if (!result) {
    SetException("GetProperty: NULL result");
    return false;
  }

  if (property_name == kAttrNameOnNatTraversalPolicyChanged) {
    OBJECT_TO_NPVARIANT(on_nat_traversal_policy_changed_func_.get(), *result);
    return true;
  } else if (property_name == kAttrNameOnStateChanged) {
    OBJECT_TO_NPVARIANT(on_state_changed_func_.get(), *result);
    return true;
  } else if (property_name == kAttrNameLogDebugInfo) {
    OBJECT_TO_NPVARIANT(log_debug_info_func_.get(), *result);
    return true;
  } else if (property_name == kAttrNameState) {
    INT32_TO_NPVARIANT(state_, *result);
    return true;
  } else if (property_name == kAttrNameAccessCode) {
    base::AutoLock auto_lock(access_code_lock_);
    *result = NPVariantFromString(access_code_);
    return true;
  } else if (property_name == kAttrNameAccessCodeLifetime) {
    base::AutoLock auto_lock(access_code_lock_);
    INT32_TO_NPVARIANT(access_code_lifetime_.InSeconds(), *result);
    return true;
  } else if (property_name == kAttrNameClient) {
    *result = NPVariantFromString(client_username_);
    return true;
  } else if (property_name == kAttrNameDisconnected) {
    INT32_TO_NPVARIANT(kDisconnected, *result);
    return true;
  } else if (property_name == kAttrNameStarting) {
    INT32_TO_NPVARIANT(kStarting, *result);
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
  } else if (property_name == kAttrNameDisconnecting) {
    INT32_TO_NPVARIANT(kDisconnecting, *result);
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
  VLOG(2) <<  "SetProperty " << property_name;
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);

  if (property_name == kAttrNameOnNatTraversalPolicyChanged) {
    if (NPVARIANT_IS_OBJECT(*value)) {
      on_nat_traversal_policy_changed_func_ = NPVARIANT_TO_OBJECT(*value);
      bool policy_received, nat_traversal_enabled;
      {
        base::AutoLock lock(nat_policy_lock_);
        policy_received = policy_received_;
        nat_traversal_enabled = nat_traversal_enabled_;
      }
      if (policy_received) {
        UpdateWebappNatPolicy(nat_traversal_enabled);
      }
      return true;
    } else {
      SetException("SetProperty: unexpected type for property " +
                   property_name);
    }
    return false;
  }

  if (property_name == kAttrNameOnStateChanged) {
    if (NPVARIANT_IS_OBJECT(*value)) {
      on_state_changed_func_ = NPVARIANT_TO_OBJECT(*value);
      return true;
    } else {
      SetException("SetProperty: unexpected type for property " +
                   property_name);
    }
    return false;
  }

  if (property_name == kAttrNameLogDebugInfo) {
    if (NPVARIANT_IS_OBJECT(*value)) {
      log_debug_info_func_ = NPVARIANT_TO_OBJECT(*value);
      HostLogHandler::RegisterLoggingScriptObject(this);
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
  VLOG(2) << "RemoveProperty " << property_name;
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  return false;
}

bool HostNPScriptObject::Enumerate(std::vector<std::string>* values) {
  VLOG(2) << "Enumerate";
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  const char* entries[] = {
    kAttrNameAccessCode,
    kAttrNameState,
    kAttrNameLogDebugInfo,
    kAttrNameOnStateChanged,
    kFuncNameConnect,
    kFuncNameDisconnect,
    kFuncNameLocalize,
    kAttrNameDisconnected,
    kAttrNameStarting,
    kAttrNameRequestedAccessCode,
    kAttrNameReceivedAccessCode,
    kAttrNameConnected,
    kAttrNameDisconnecting,
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
  DCHECK(host_context_.network_message_loop()->BelongsToCurrentThread());

  ++failed_login_attempts_;
  if (failed_login_attempts_ == kMaxLoginAttempts)
    DisconnectInternal();
}

void HostNPScriptObject::OnClientAuthenticated(const std::string& jid) {
  if (MessageLoop::current() != host_context_.main_message_loop()) {
    host_context_.main_message_loop()->PostTask(FROM_HERE, base::Bind(
        &HostNPScriptObject::OnClientAuthenticated,
        base::Unretained(this), jid));
    return;
  }

  if (state_ == kDisconnecting) {
    // Ignore the new connection if we are disconnecting.
    return;
  }

  client_username_ = jid;
  size_t pos = client_username_.find('/');
  if (pos != std::string::npos)
    client_username_.replace(pos, std::string::npos, "");
  LOG(INFO) << "Client " << client_username_ << " connected.";
  SetState(kConnected);
}

void HostNPScriptObject::OnClientDisconnected(const std::string& jid) {
  if (MessageLoop::current() != host_context_.main_message_loop()) {
    host_context_.main_message_loop()->PostTask(FROM_HERE, base::Bind(
        &HostNPScriptObject::OnClientDisconnected,
        base::Unretained(this), jid));
    return;
  }

  client_username_.clear();

  // Disconnect the host when a client disconnects.
  DisconnectInternal();
}

void HostNPScriptObject::OnShutdown() {
  if (MessageLoop::current() != host_context_.main_message_loop()) {
    host_context_.main_message_loop()->PostTask(FROM_HERE, base::Bind(
        &HostNPScriptObject::OnShutdown, base::Unretained(this)));
    return;
  }

  host_ = NULL;
  if (state_ != kDisconnected) {
    SetState(kDisconnected);
  }
}

// string uid, string auth_token
bool HostNPScriptObject::Connect(const NPVariant* args,
                                 uint32_t arg_count,
                                 NPVariant* result) {
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);

  LOG(INFO) << "Connecting...";

  if (arg_count != 2) {
    SetException("connect: bad number of arguments");
    return false;
  }

  if (state_ != kDisconnected) {
    SetException("connect: can be called only when disconnected");
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

  ReadPolicyAndConnect(uid, auth_token, auth_service);

  return true;
}

void HostNPScriptObject::ReadPolicyAndConnect(const std::string& uid,
                                              const std::string& auth_token,
                                              const std::string& auth_service) {
  if (MessageLoop::current() != host_context_.main_message_loop()) {
    host_context_.main_message_loop()->PostTask(
        FROM_HERE, base::Bind(
            &HostNPScriptObject::ReadPolicyAndConnect, base::Unretained(this),
            uid, auth_token, auth_service));
    return;
  }

  SetState(kStarting);

  // Only proceed to FinishConnect() if at least one policy update has been
  // received.
  if (policy_received_) {
    FinishConnect(uid, auth_token, auth_service);
  } else {
    // Otherwise, create the policy watcher, and thunk the connect.
    pending_connect_ =
        base::Bind(&HostNPScriptObject::FinishConnect,
                   base::Unretained(this), uid, auth_token, auth_service);
  }
}

void HostNPScriptObject::FinishConnect(
    const std::string& uid,
    const std::string& auth_token,
    const std::string& auth_service) {
  DCHECK_EQ(MessageLoop::current(), host_context_.main_message_loop());

  if (state_ != kStarting) {
    // Host has been stopped while we were fetching policy.
    return;
  }

  // Store the supplied user ID and token to the Host configuration.
  scoped_refptr<MutableHostConfig> host_config = new InMemoryHostConfig();
  host_config->SetString(kXmppLoginConfigPath, uid);
  host_config->SetString(kXmppAuthTokenConfigPath, auth_token);
  host_config->SetString(kXmppAuthServiceConfigPath, auth_service);

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
                     base::Unretained(this)))) {
    SetState(kError);
    return;
  }

  // Create DesktopEnvironment.
  desktop_environment_.reset(DesktopEnvironment::Create(&host_context_));
  if (desktop_environment_.get() == NULL) {
    SetState(kError);
    return;
  }

  // Beyond this point nothing can fail, so save the config and request.
  host_config_ = host_config;
  register_request_.reset(register_request.release());

  // Create the Host.
  LOG(INFO) << "NAT state: " << nat_traversal_enabled_;
  host_ = ChromotingHost::Create(
      &host_context_, host_config_, desktop_environment_.get(),
      nat_traversal_enabled_);
  host_->AddStatusObserver(this);
  host_->AddStatusObserver(register_request_.get());
  if (enable_log_to_server_) {
    log_to_server_.reset(new LogToServer(host_context_.network_message_loop()));
    host_->AddStatusObserver(log_to_server_.get());
  }
  host_->set_it2me(true);
  it2me_host_user_interface_.reset(new It2MeHostUserInterface(host_.get(),
                                                              &host_context_));
  it2me_host_user_interface_->Init();
  host_->AddStatusObserver(it2me_host_user_interface_.get());

  {
    base::AutoLock auto_lock(ui_strings_lock_);
    host_->SetUiStrings(ui_strings_);
  }

  // Start the Host.
  host_->Start();

  SetState(kRequestedAccessCode);
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

bool HostNPScriptObject::Localize(const NPVariant* args,
                                  uint32_t arg_count,
                                  NPVariant* result) {
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  if (arg_count != 1) {
    SetException("localize: bad number of arguments");
    return false;
  }

  if (NPVARIANT_IS_OBJECT(args[0])) {
    ScopedRefNPObject localize_func(NPVARIANT_TO_OBJECT(args[0]));
    LocalizeStrings(localize_func.get());
    return true;
  } else {
    SetException("localize: unexpected type for argument 1");
    return false;
  }
}

void HostNPScriptObject::DisconnectInternal() {
  if (MessageLoop::current() != host_context_.main_message_loop()) {
    host_context_.main_message_loop()->PostTask(
        FROM_HERE, base::Bind(&HostNPScriptObject::DisconnectInternal,
                              base::Unretained(this)));
    return;
  }

  switch (state_) {
    case kDisconnected:
      disconnected_event_.Signal();
      return;

    case kStarting:
      SetState(kDisconnecting);
      SetState(kDisconnected);
      disconnected_event_.Signal();
      return;

    case kDisconnecting:
      return;

    default:
      DCHECK(host_);
      SetState(kDisconnecting);
      host_->Shutdown(
          base::Bind(&HostNPScriptObject::OnShutdownFinished,
                     base::Unretained(this)));
  }
}

void HostNPScriptObject::OnShutdownFinished() {
  if (MessageLoop::current() != host_context_.main_message_loop()) {
    host_context_.main_message_loop()->PostTask(FROM_HERE, base::Bind(
        &HostNPScriptObject::OnShutdownFinished, base::Unretained(this)));
    return;
  }

  disconnected_event_.Signal();
}

void HostNPScriptObject::OnNatPolicyUpdate(bool nat_traversal_enabled) {
  if (MessageLoop::current() != host_context_.main_message_loop()) {
    host_context_.main_message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&HostNPScriptObject::OnNatPolicyUpdate,
                   base::Unretained(this), nat_traversal_enabled));
    return;
  }

  VLOG(2) << "OnNatPolicyUpdate: " << nat_traversal_enabled;

  // When transitioning from enabled to disabled, force disconnect any
  // existing session.
  if (nat_traversal_enabled_ && !nat_traversal_enabled) {
    DisconnectInternal();
  }

  {
    base::AutoLock lock(nat_policy_lock_);
    policy_received_ = true;
    nat_traversal_enabled_ = nat_traversal_enabled;
  }

  UpdateWebappNatPolicy(nat_traversal_enabled_);

  if (!pending_connect_.is_null()) {
    pending_connect_.Run();
    pending_connect_.Reset();
  }
}

void HostNPScriptObject::OnReceivedSupportID(
    bool success,
    const std::string& support_id,
    const base::TimeDelta& lifetime) {
  DCHECK(host_context_.network_message_loop()->BelongsToCurrentThread());

  if (!success) {
    host_context_.main_message_loop()->PostTask(FROM_HERE, base::Bind(
        &HostNPScriptObject::NotifyAccessCode, base::Unretained(this), false));
    DisconnectInternal();
    return;
  }

  std::string host_secret = GenerateSupportHostSecret();
  std::string access_code = support_id + host_secret;
  host_->SetSharedSecret(access_code);

  {
    base::AutoLock lock(access_code_lock_);
    access_code_ = access_code;
    access_code_lifetime_ = lifetime;
  }

  host_context_.main_message_loop()->PostTask(FROM_HERE, base::Bind(
      &HostNPScriptObject::NotifyAccessCode, base::Unretained(this), true));
}

void HostNPScriptObject::NotifyAccessCode(bool success) {
  DCHECK_EQ(MessageLoop::current(), host_context_.main_message_loop());
  if (state_ == kRequestedAccessCode) {
    SetState(success ? kReceivedAccessCode : kError);
  }
}

void HostNPScriptObject::SetState(State state) {
  DCHECK_EQ(MessageLoop::current(), host_context_.main_message_loop());
  switch (state_) {
    case kDisconnected:
      DCHECK(state == kStarting ||
             state == kError) << state;
      break;
    case kStarting:
      DCHECK(state == kRequestedAccessCode ||
             state == kDisconnecting ||
             state == kError) << state;
      break;
    case kRequestedAccessCode:
      DCHECK(state == kReceivedAccessCode ||
             state == kDisconnecting ||
             state == kError) << state;
      break;
    case kReceivedAccessCode:
      DCHECK(state == kConnected ||
             state == kDisconnecting ||
             state == kError) << state;
      break;
    case kConnected:
      DCHECK(state == kDisconnecting ||
             state == kDisconnected ||
             state == kError) << state;
      break;
    case kDisconnecting:
      DCHECK(state == kDisconnected) << state;
      break;
    case kError:
      DCHECK(state == kDisconnecting) << state;
      break;
  };
  state_ = state;
  NotifyStateChanged(state);
}

void HostNPScriptObject::NotifyStateChanged(State state) {
  if (!plugin_message_loop_proxy_->BelongsToCurrentThread()) {
    plugin_message_loop_proxy_->PostTask(
        FROM_HERE, base::Bind(&HostNPScriptObject::NotifyStateChanged,
                              base::Unretained(this), state));
    return;
  }
  if (on_state_changed_func_.get()) {
    VLOG(2) << "Calling state changed " << state;
    NPVariant state_var;
    INT32_TO_NPVARIANT(state, state_var);
    bool is_good = InvokeAndIgnoreResult(on_state_changed_func_.get(),
                                         &state_var, 1);
    LOG_IF(ERROR, !is_good) << "OnStateChanged failed";
  }
}

void HostNPScriptObject::PostLogDebugInfo(const std::string& message) {
  if (plugin_message_loop_proxy_->BelongsToCurrentThread()) {
    // Make sure we're not currently processing a log message.
    // We only need to check this if we're on the plugin thread.
    if (am_currently_logging_)
      return;
  }

  // Always post (even if we're already on the correct thread) so that debug
  // log messages are shown in the correct order.
  plugin_message_loop_proxy_->PostTask(
      FROM_HERE, base::Bind(&HostNPScriptObject::LogDebugInfo,
                            base::Unretained(this), message));
}

void HostNPScriptObject::LogDebugInfo(const std::string& message) {
  if (log_debug_info_func_.get()) {
    am_currently_logging_ = true;
    NPVariant log_message;
    STRINGZ_TO_NPVARIANT(message.c_str(), log_message);
    bool is_good = InvokeAndIgnoreResult(log_debug_info_func_.get(),
                                         &log_message, 1);
    if (!is_good) {
      LOG(ERROR) << "ERROR - LogDebugInfo failed\n";
    }
    am_currently_logging_ = false;
  }
}

void HostNPScriptObject::SetException(const std::string& exception_string) {
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  g_npnetscape_funcs->setexception(parent_, exception_string.c_str());
  LOG(INFO) << exception_string;
}

void HostNPScriptObject::LocalizeStrings(NPObject* localize_func) {
  DCHECK(plugin_message_loop_proxy_->BelongsToCurrentThread());

  UiStrings ui_strings;
  string16 direction;
  LocalizeString(localize_func, "@@bidi_dir", &direction);
  ui_strings.direction = UTF16ToUTF8(direction) == "rtl" ?
      remoting::UiStrings::RTL : remoting::UiStrings::LTR;
  LocalizeString(localize_func, /*i18n-content*/"PRODUCT_NAME",
                 &ui_strings.product_name);
  LocalizeString(localize_func, /*i18n-content*/"DISCONNECT_OTHER_BUTTON",
                 &ui_strings.disconnect_button_text);
  LocalizeString(localize_func,
#if defined(OS_WIN)
      /*i18n-content*/"DISCONNECT_BUTTON_PLUS_SHORTCUT_WINDOWS",
#elif defined(OS_MACOSX)
      /*i18n-content*/"DISCONNECT_BUTTON_PLUS_SHORTCUT_MAC_OS_X",
#else
      /*i18n-content*/"DISCONNECT_BUTTON_PLUS_SHORTCUT_LINUX",
#endif
      &ui_strings.disconnect_button_text_plus_shortcut);
  LocalizeString(localize_func, /*i18n-content*/"CONTINUE_PROMPT",
                 &ui_strings.continue_prompt);
  LocalizeString(localize_func, /*i18n-content*/"CONTINUE_BUTTON",
                 &ui_strings.continue_button_text);
  LocalizeString(localize_func, /*i18n-content*/"STOP_SHARING_BUTTON",
                 &ui_strings.stop_sharing_button_text);
  LocalizeString(localize_func, /*i18n-content*/"MESSAGE_SHARED",
                 &ui_strings.disconnect_message);

  base::AutoLock auto_lock(ui_strings_lock_);
  ui_strings_ = ui_strings;
}

bool HostNPScriptObject::LocalizeString(NPObject* localize_func,
                                        const char* tag, string16* result) {
  NPVariant args[2];
  STRINGZ_TO_NPVARIANT(tag, args[0]);
  NPVariant np_result;
  bool is_good = g_npnetscape_funcs->invokeDefault(
      plugin_, localize_func, &args[0], 1, &np_result);
  if (!is_good) {
    LOG(ERROR) << "Localization failed for " << tag;
    return false;
  }
  std::string translation = StringFromNPVariant(np_result);
  g_npnetscape_funcs->releasevariantvalue(&np_result);
  if (translation.empty()) {
    LOG(ERROR) << "Missing translation for " << tag;
    return false;
  }
  *result = UTF8ToUTF16(translation);
  return true;
}

void HostNPScriptObject::UpdateWebappNatPolicy(bool nat_traversal_enabled) {
  if (!plugin_message_loop_proxy_->BelongsToCurrentThread()) {
    plugin_message_loop_proxy_->PostTask(
        FROM_HERE, base::Bind(&HostNPScriptObject::UpdateWebappNatPolicy,
                              base::Unretained(this), nat_traversal_enabled));
    return;
  }
  if (on_nat_traversal_policy_changed_func_.get()) {
    NPVariant policy;
    BOOLEAN_TO_NPVARIANT(nat_traversal_enabled, policy);
    InvokeAndIgnoreResult(on_nat_traversal_policy_changed_func_.get(),
                          &policy, 1);
  }
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

}  // namespace remoting
