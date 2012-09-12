// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/host_script_object.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/net_util.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/auth_token_util.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/desktop_environment_factory.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/host_secret.h"
#include "remoting/host/it2me_host_user_interface.h"
#include "remoting/host/network_settings.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/plugin/daemon_controller.h"
#include "remoting/host/plugin/host_log_handler.h"
#include "remoting/host/policy_hack/policy_watcher.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/host/session_manager_factory.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "remoting/protocol/it2me_host_authenticator_factory.h"

namespace remoting {

namespace {

// This is used for tagging system event logs.
const char kApplicationName[] = "chromoting";

const char* kAttrNameAccessCode = "accessCode";
const char* kAttrNameAccessCodeLifetime = "accessCodeLifetime";
const char* kAttrNameClient = "client";
const char* kAttrNameDaemonState = "daemonState";
const char* kAttrNameState = "state";
const char* kAttrNameLogDebugInfo = "logDebugInfo";
const char* kAttrNameOnNatTraversalPolicyChanged =
    "onNatTraversalPolicyChanged";
const char* kAttrNameOnStateChanged = "onStateChanged";
const char* kFuncNameConnect = "connect";
const char* kFuncNameDisconnect = "disconnect";
const char* kFuncNameLocalize = "localize";
const char* kFuncNameGetHostName = "getHostName";
const char* kFuncNameGetPinHash = "getPinHash";
const char* kFuncNameGenerateKeyPair = "generateKeyPair";
const char* kFuncNameUpdateDaemonConfig = "updateDaemonConfig";
const char* kFuncNameGetDaemonConfig = "getDaemonConfig";
const char* kFuncNameGetDaemonVersion = "getDaemonVersion";
const char* kFuncNameGetUsageStatsConsent = "getUsageStatsConsent";
const char* kFuncNameStartDaemon = "startDaemon";
const char* kFuncNameStopDaemon = "stopDaemon";

// States.
const char* kAttrNameDisconnected = "DISCONNECTED";
const char* kAttrNameStarting = "STARTING";
const char* kAttrNameRequestedAccessCode = "REQUESTED_ACCESS_CODE";
const char* kAttrNameReceivedAccessCode = "RECEIVED_ACCESS_CODE";
const char* kAttrNameConnected = "CONNECTED";
const char* kAttrNameDisconnecting = "DISCONNECTING";
const char* kAttrNameError = "ERROR";

const int kMaxLoginAttempts = 5;

// We may need to have more than one task running at the same time
// (e.g. key generation and status update), yet unlikely to ever need
// more than 2 threads.
const int kMaxWorkerPoolThreads = 2;

}  // namespace

HostNPScriptObject::HostNPScriptObject(
    NPP plugin,
    NPObject* parent,
    PluginThreadTaskRunner::Delegate* plugin_thread_delegate)
    : plugin_(plugin),
      parent_(parent),
      am_currently_logging_(false),
      state_(kDisconnected),
      np_thread_id_(base::PlatformThread::CurrentId()),
      plugin_task_runner_(
          new PluginThreadTaskRunner(plugin_thread_delegate)),
      desktop_environment_factory_(new DesktopEnvironmentFactory()),
      failed_login_attempts_(0),
      disconnected_event_(true, false),
      stopped_event_(true, false),
      nat_traversal_enabled_(false),
      policy_received_(false),
      daemon_controller_(DaemonController::Create()),
      worker_thread_("RemotingHostPlugin") {
  worker_thread_.Start();
}

HostNPScriptObject::~HostNPScriptObject() {
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);

  HostLogHandler::UnregisterLoggingScriptObject(this);

  // Stop the message loop. Any attempt to post a task to
  // |context_.ui_task_runner()| will result in a CHECK() after this point.
  // TODO(alexeypa): Enable posting messages to |plugin_task_runner_| during
  // shutdown to avoid this hack.
  plugin_task_runner_->Detach();

  // Stop listening for policy updates.
  if (policy_watcher_.get()) {
    base::WaitableEvent policy_watcher_stopped_(true, false);
    policy_watcher_->StopWatching(&policy_watcher_stopped_);
    policy_watcher_stopped_.Wait();
    policy_watcher_.reset();
  }

  if (host_context_.get()) {
    // Disconnect synchronously. We cannot disconnect asynchronously
    // here because |host_context_| needs to be stopped on the plugin
    // thread, but the plugin thread may not exist after the instance
    // is destroyed.
    disconnected_event_.Reset();
    DisconnectInternal();

    // |disconnected_event_| is signalled when the host is completely stopped.
    disconnected_event_.Wait();

    // UI needs to be shut down on the UI thread before we destroy the
    // host context (because it depends on the context object), but
    // only after the host has been shut down (becase the UI object is
    // registered as status observer for the host, and we can't
    // unregister it from this thread).
    it2me_host_user_interface_.reset();

    // Release the context's TaskRunner references for the threads, so they can
    // exit when no objects need them.
    host_context_->ReleaseTaskRunners();

    // |stopped_event_| is signalled when the last reference to the plugin
    // thread is dropped.
    stopped_event_.Wait();

    // Stop all threads.
    host_context_.reset();
  }

  worker_thread_.Stop();
}

bool HostNPScriptObject::Init() {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
  VLOG(2) << "Init";

  host_context_.reset(new ChromotingHostContext(new AutoThreadTaskRunner(
      plugin_task_runner_,
      base::Bind(&base::WaitableEvent::Signal,
                 base::Unretained(&stopped_event_)))));
  if (!host_context_->Start()) {
    host_context_.reset();
    return false;
  }

  policy_watcher_.reset(
      policy_hack::PolicyWatcher::Create(host_context_->network_task_runner()));
  policy_watcher_->StartWatching(
      base::Bind(&HostNPScriptObject::OnPolicyUpdate,
                 base::Unretained(this)));
  return true;
}

bool HostNPScriptObject::HasMethod(const std::string& method_name) {
  VLOG(2) << "HasMethod " << method_name;
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  return (method_name == kFuncNameConnect ||
          method_name == kFuncNameDisconnect ||
          method_name == kFuncNameLocalize ||
          method_name == kFuncNameGetHostName ||
          method_name == kFuncNameGetPinHash ||
          method_name == kFuncNameGenerateKeyPair ||
          method_name == kFuncNameUpdateDaemonConfig ||
          method_name == kFuncNameGetDaemonConfig ||
          method_name == kFuncNameGetDaemonVersion ||
          method_name == kFuncNameGetUsageStatsConsent ||
          method_name == kFuncNameStartDaemon ||
          method_name == kFuncNameStopDaemon);
}

bool HostNPScriptObject::InvokeDefault(const NPVariant* args,
                                       uint32_t arg_count,
                                       NPVariant* result) {
  VLOG(2) << "InvokeDefault";
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  SetException("exception during default invocation");
  return false;
}

bool HostNPScriptObject::Invoke(const std::string& method_name,
                                const NPVariant* args,
                                uint32_t arg_count,
                                NPVariant* result) {
  VLOG(2) << "Invoke " << method_name;
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  if (method_name == kFuncNameConnect) {
    return Connect(args, arg_count, result);
  } else if (method_name == kFuncNameDisconnect) {
    return Disconnect(args, arg_count, result);
  } else if (method_name == kFuncNameLocalize) {
    return Localize(args, arg_count, result);
  } else if (method_name == kFuncNameGetHostName) {
    return GetHostName(args, arg_count, result);
  } else if (method_name == kFuncNameGetPinHash) {
    return GetPinHash(args, arg_count, result);
  } else if (method_name == kFuncNameGenerateKeyPair) {
    return GenerateKeyPair(args, arg_count, result);
  } else if (method_name == kFuncNameUpdateDaemonConfig) {
    return UpdateDaemonConfig(args, arg_count, result);
  } else if (method_name == kFuncNameGetDaemonConfig) {
    return GetDaemonConfig(args, arg_count, result);
  } else if (method_name == kFuncNameGetDaemonVersion) {
    return GetDaemonVersion(args, arg_count, result);
  } else if (method_name == kFuncNameGetUsageStatsConsent) {
    return GetUsageStatsConsent(args, arg_count, result);
  } else if (method_name == kFuncNameStartDaemon) {
    return StartDaemon(args, arg_count, result);
  } else if (method_name == kFuncNameStopDaemon) {
    return StopDaemon(args, arg_count, result);
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
          property_name == kAttrNameDaemonState ||
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
  } else if (property_name == kAttrNameDaemonState) {
    INT32_TO_NPVARIANT(daemon_controller_->GetState(), *result);
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
    kAttrNameDisconnected,
    kAttrNameStarting,
    kAttrNameRequestedAccessCode,
    kAttrNameReceivedAccessCode,
    kAttrNameConnected,
    kAttrNameDisconnecting,
    kAttrNameError,
    kFuncNameConnect,
    kFuncNameDisconnect,
    kFuncNameLocalize,
    kFuncNameGetHostName,
    kFuncNameGetPinHash,
    kFuncNameGenerateKeyPair,
    kFuncNameUpdateDaemonConfig,
    kFuncNameGetDaemonConfig,
    kFuncNameGetDaemonVersion,
    kFuncNameGetUsageStatsConsent,
    kFuncNameStartDaemon,
    kFuncNameStopDaemon
  };
  for (size_t i = 0; i < arraysize(entries); ++i) {
    values->push_back(entries[i]);
  }
  return true;
}

void HostNPScriptObject::OnAccessDenied(const std::string& jid) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  ++failed_login_attempts_;
  if (failed_login_attempts_ == kMaxLoginAttempts) {
    DisconnectInternal();
  }
}

void HostNPScriptObject::OnClientAuthenticated(const std::string& jid) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

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
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());
  client_username_.clear();
  DisconnectInternal();
}

void HostNPScriptObject::OnShutdown() {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  register_request_.reset();
  log_to_server_.reset();
  signal_strategy_.reset();
  host_event_logger_.reset();
  host_->RemoveStatusObserver(this);
  host_ = NULL;

  if (state_ != kDisconnected) {
    SetState(kDisconnected);
  }
}

// string uid, string auth_token
bool HostNPScriptObject::Connect(const NPVariant* args,
                                 uint32_t arg_count,
                                 NPVariant* result) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

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

  // The UserInterface object needs to be created on the UI thread.
  it2me_host_user_interface_.reset(
      new It2MeHostUserInterface(host_context_.get()));

  ReadPolicyAndConnect(uid, auth_token, auth_service);

  return true;
}

void HostNPScriptObject::ReadPolicyAndConnect(const std::string& uid,
                                              const std::string& auth_token,
                                              const std::string& auth_service) {
  if (!host_context_->network_task_runner()->BelongsToCurrentThread()) {
    host_context_->network_task_runner()->PostTask(
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
  if (!host_context_->network_task_runner()->BelongsToCurrentThread()) {
    host_context_->network_task_runner()->PostTask(FROM_HERE, base::Bind(
        &HostNPScriptObject::FinishConnect, base::Unretained(this),
        uid, auth_token, auth_service));
    return;
  }

  if (state_ != kStarting) {
    // Host has been stopped while we were fetching policy.
    return;
  }

  // Check the host domain policy.
  if (!required_host_domain_.empty() &&
      !EndsWith(uid, std::string("@") + required_host_domain_, false)) {
    SetState(kError);
    return;
  }

  // Generate a key pair for the Host to use.
  // TODO(wez): Move this to the worker thread.
  host_key_pair_.Generate();

  // Create XMPP connection.
  scoped_ptr<SignalStrategy> signal_strategy(
      new XmppSignalStrategy(host_context_->url_request_context_getter(),
                             uid, auth_token, auth_service));

  // Request registration of the host for support.
  scoped_ptr<RegisterSupportHostRequest> register_request(
      new RegisterSupportHostRequest(
          signal_strategy.get(), &host_key_pair_,
          base::Bind(&HostNPScriptObject::OnReceivedSupportID,
                     base::Unretained(this))));

  // Beyond this point nothing can fail, so save the config and request.
  signal_strategy_.reset(signal_strategy.release());
  register_request_.reset(register_request.release());

  // If NAT traversal is off then limit port range to allow firewall pin-holing.
  LOG(INFO) << "NAT state: " << nat_traversal_enabled_;
  NetworkSettings network_settings(
     nat_traversal_enabled_ ?
     NetworkSettings::NAT_TRAVERSAL_ENABLED :
     NetworkSettings::NAT_TRAVERSAL_DISABLED);
  if (!nat_traversal_enabled_) {
    network_settings.min_port = NetworkSettings::kDefaultMinPort;
    network_settings.max_port = NetworkSettings::kDefaultMaxPort;
  }

  // Create the Host.
  host_ = new ChromotingHost(
      host_context_.get(), signal_strategy_.get(),
      desktop_environment_factory_.get(),
      CreateHostSessionManager(network_settings,
                               host_context_->url_request_context_getter()));
  host_->AddStatusObserver(this);
  log_to_server_.reset(
      new LogToServer(host_, ServerLogEntry::IT2ME, signal_strategy_.get()));
  base::Closure disconnect_callback = base::Bind(
      &ChromotingHost::Shutdown, base::Unretained(host_.get()),
      base::Closure());
  it2me_host_user_interface_->Start(host_.get(), disconnect_callback);
  host_event_logger_ = HostEventLogger::Create(host_, kApplicationName);

  {
    base::AutoLock auto_lock(ui_strings_lock_);
    host_->SetUiStrings(ui_strings_);
  }

  signal_strategy_->Connect();
  host_->Start(uid);

  SetState(kRequestedAccessCode);
  return;
}

bool HostNPScriptObject::Disconnect(const NPVariant* args,
                                    uint32_t arg_count,
                                    NPVariant* result) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
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
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
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

bool HostNPScriptObject::GetHostName(const NPVariant* args,
                                     uint32_t arg_count,
                                     NPVariant* result) {
  if (arg_count != 0) {
    SetException("getHostName: bad number of arguments");
    return false;
  }
  DCHECK(result);
  *result = NPVariantFromString(net::GetHostName());
  return true;
}

bool HostNPScriptObject::GetPinHash(const NPVariant* args,
                                    uint32_t arg_count,
                                    NPVariant* result) {
  if (arg_count != 2) {
    SetException("getPinHash: bad number of arguments");
    return false;
  }

  std::string host_id = StringFromNPVariant(args[0]);
  if (host_id.empty()) {
    SetException("getPinHash: bad hostId parameter");
    return false;
  }

  if (!NPVARIANT_IS_STRING(args[1])) {
    SetException("getPinHash: bad pin parameter");
    return false;
  }
  std::string pin = StringFromNPVariant(args[1]);

  *result = NPVariantFromString(remoting::MakeHostPinHash(host_id, pin));

  return true;
}

bool HostNPScriptObject::GenerateKeyPair(const NPVariant* args,
                                         uint32_t arg_count,
                                         NPVariant* result) {
  if (arg_count != 1) {
    SetException("generateKeyPair: bad number of arguments");
    return false;
  }

  ScopedRefNPObject callback_obj(ObjectFromNPVariant(args[0]));
  if (!callback_obj.get()) {
    SetException("generateKeyPair: invalid callback parameter");
    return false;
  }

  worker_thread_.message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(&HostNPScriptObject::DoGenerateKeyPair,
                            base::Unretained(this), callback_obj));
  return true;
}

bool HostNPScriptObject::UpdateDaemonConfig(const NPVariant* args,
                                            uint32_t arg_count,
                                            NPVariant* result) {
  if (arg_count != 2) {
    SetException("updateDaemonConfig: bad number of arguments");
    return false;
  }

  std::string config_str = StringFromNPVariant(args[0]);
  scoped_ptr<base::Value> config(
      base::JSONReader::Read(config_str, base::JSON_ALLOW_TRAILING_COMMAS));
  if (config_str.empty() || !config.get() ||
      !config->IsType(base::Value::TYPE_DICTIONARY)) {
    SetException("updateDaemonConfig: bad config parameter");
    return false;
  }
  scoped_ptr<base::DictionaryValue> config_dict(
      reinterpret_cast<base::DictionaryValue*>(config.release()));

  ScopedRefNPObject callback_obj(ObjectFromNPVariant(args[1]));
  if (!callback_obj.get()) {
    SetException("updateDaemonConfig: invalid callback parameter");
    return false;
  }

  if (config_dict->HasKey(kHostIdConfigPath) ||
      config_dict->HasKey(kXmppLoginConfigPath)) {
    SetException("updateDaemonConfig: trying to update immutable config "
                 "parameters");
    return false;
  }

  daemon_controller_->UpdateConfig(
      config_dict.Pass(),
      base::Bind(&HostNPScriptObject::InvokeAsyncResultCallback,
                 base::Unretained(this), callback_obj));
  return true;
}

bool HostNPScriptObject::GetDaemonConfig(const NPVariant* args,
                                         uint32_t arg_count,
                                         NPVariant* result) {
  if (arg_count != 1) {
    SetException("getDaemonConfig: bad number of arguments");
    return false;
  }

  ScopedRefNPObject callback_obj(ObjectFromNPVariant(args[0]));
  if (!callback_obj.get()) {
    SetException("getDaemonConfig: invalid callback parameter");
    return false;
  }

  // We control lifetime of the |daemon_controller_| so it's safe to
  // use base::Unretained() here.
  daemon_controller_->GetConfig(
      base::Bind(&HostNPScriptObject::InvokeGetDaemonConfigCallback,
                 base::Unretained(this), callback_obj));

  return true;
}

bool HostNPScriptObject::GetDaemonVersion(const NPVariant* args,
                                          uint32_t arg_count,
                                          NPVariant* result) {
  if (arg_count != 1) {
    SetException("getDaemonVersion: bad number of arguments");
    return false;
  }

  ScopedRefNPObject callback_obj(ObjectFromNPVariant(args[0]));
  if (!callback_obj.get()) {
    SetException("getDaemonVersion: invalid callback parameter");
    return false;
  }

  // We control lifetime of the |daemon_controller_| so it's safe to
  // use base::Unretained() here.
  daemon_controller_->GetVersion(
      base::Bind(&HostNPScriptObject::InvokeGetDaemonVersionCallback,
                 base::Unretained(this), callback_obj));

  return true;
}

bool HostNPScriptObject::GetUsageStatsConsent(const NPVariant* args,
                                              uint32_t arg_count,
                                              NPVariant* result) {
  if (arg_count != 1) {
    SetException("getUsageStatsConsent: bad number of arguments");
    return false;
  }

  ScopedRefNPObject callback_obj(ObjectFromNPVariant(args[0]));
  if (!callback_obj.get()) {
    SetException("getUsageStatsConsent: invalid callback parameter");
    return false;
  }

  // We control lifetime of the |daemon_controller_| so it's safe to
  // use base::Unretained() here.
  daemon_controller_->GetUsageStatsConsent(
      base::Bind(&HostNPScriptObject::InvokeGetUsageStatsConsentCallback,
                 base::Unretained(this), callback_obj));
  return true;
}

bool HostNPScriptObject::StartDaemon(const NPVariant* args,
                                     uint32_t arg_count,
                                     NPVariant* result) {
  if (arg_count != 3) {
    SetException("startDaemon: bad number of arguments");
    return false;
  }

  std::string config_str = StringFromNPVariant(args[0]);
  scoped_ptr<base::Value> config(
      base::JSONReader::Read(config_str, base::JSON_ALLOW_TRAILING_COMMAS));
  if (config_str.empty() || !config.get() ||
      !config->IsType(base::Value::TYPE_DICTIONARY)) {
    SetException("startDaemon: bad config parameter");
    return false;
  }
  scoped_ptr<base::DictionaryValue> config_dict(
      reinterpret_cast<base::DictionaryValue*>(config.release()));

  if (!NPVARIANT_IS_BOOLEAN(args[1])) {
    SetException("startDaemon: invalid consent parameter");
    return false;
  }

  ScopedRefNPObject callback_obj(ObjectFromNPVariant(args[2]));
  if (!callback_obj.get()) {
    SetException("startDaemon: invalid callback parameter");
    return false;
  }

  daemon_controller_->SetConfigAndStart(
      config_dict.Pass(),
      NPVARIANT_TO_BOOLEAN(args[1]),
      base::Bind(&HostNPScriptObject::InvokeAsyncResultCallback,
                 base::Unretained(this), callback_obj));
  return true;
}

bool HostNPScriptObject::StopDaemon(const NPVariant* args,
                                    uint32_t arg_count,
                                    NPVariant* result) {
  if (arg_count != 1) {
    SetException("stopDaemon: bad number of arguments");
    return false;
  }

  ScopedRefNPObject callback_obj(ObjectFromNPVariant(args[0]));
  if (!callback_obj.get()) {
    SetException("stopDaemon: invalid callback parameter");
    return false;
  }

  daemon_controller_->Stop(
      base::Bind(&HostNPScriptObject::InvokeAsyncResultCallback,
                 base::Unretained(this), callback_obj));
  return true;
}

void HostNPScriptObject::DisconnectInternal() {
  if (!host_context_->network_task_runner()->BelongsToCurrentThread()) {
    host_context_->network_task_runner()->PostTask(
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
      SetState(kDisconnecting);

      if (!host_) {
        OnShutdownFinished();
        return;
      }
      // ChromotingHost::Shutdown() may destroy SignalStrategy
      // synchronously, but SignalStrategy::Listener handlers are not
      // allowed to destroy SignalStrategy, so post task to call
      // Shutdown() later.
      host_context_->network_task_runner()->PostTask(
          FROM_HERE, base::Bind(
              &ChromotingHost::Shutdown, host_,
              base::Bind(&HostNPScriptObject::OnShutdownFinished,
                         base::Unretained(this))));
      return;
  }
}

void HostNPScriptObject::OnShutdownFinished() {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  disconnected_event_.Signal();
}

void HostNPScriptObject::OnPolicyUpdate(
    scoped_ptr<base::DictionaryValue> policies) {
  if (!host_context_->network_task_runner()->BelongsToCurrentThread()) {
    host_context_->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&HostNPScriptObject::OnPolicyUpdate,
                   base::Unretained(this), base::Passed(&policies)));
    return;
  }

  bool nat_policy;
  if (policies->GetBoolean(policy_hack::PolicyWatcher::kNatPolicyName,
                           &nat_policy)) {
    UpdateNatPolicy(nat_policy);
  }
  std::string host_domain;
  if (policies->GetString(policy_hack::PolicyWatcher::kHostDomainPolicyName,
                          &host_domain)) {
    UpdateHostDomainPolicy(host_domain);
  }

  {
    base::AutoLock lock(nat_policy_lock_);
    policy_received_ = true;
  }

  if (!pending_connect_.is_null()) {
    pending_connect_.Run();
    pending_connect_.Reset();
  }
}

void HostNPScriptObject::UpdateNatPolicy(bool nat_traversal_enabled) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateNatPolicy: " << nat_traversal_enabled;

  // When transitioning from enabled to disabled, force disconnect any
  // existing session.
  if (nat_traversal_enabled_ && !nat_traversal_enabled) {
    DisconnectInternal();
  }

  {
    base::AutoLock lock(nat_policy_lock_);
    nat_traversal_enabled_ = nat_traversal_enabled;
  }

  UpdateWebappNatPolicy(nat_traversal_enabled_);
}

void HostNPScriptObject::UpdateHostDomainPolicy(
    const std::string& host_domain) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateHostDomainPolicy: " << host_domain;

  // When setting a host domain policy, force disconnect any existing session.
  if (!host_domain.empty() && state_ != kStarting) {
    DisconnectInternal();
  }

  required_host_domain_ = host_domain;
}

void HostNPScriptObject::OnReceivedSupportID(
    bool success,
    const std::string& support_id,
    const base::TimeDelta& lifetime) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  if (!success) {
    SetState(kError);
    DisconnectInternal();
    return;
  }

  std::string host_secret = GenerateSupportHostSecret();
  std::string access_code = support_id + host_secret;
  scoped_ptr<protocol::AuthenticatorFactory> factory(
      new protocol::It2MeHostAuthenticatorFactory(
          host_key_pair_.GenerateCertificate(), *host_key_pair_.private_key(),
          access_code));
  host_->SetAuthenticatorFactory(factory.Pass());

  {
    base::AutoLock lock(access_code_lock_);
    access_code_ = access_code;
    access_code_lifetime_ = lifetime;
  }

  SetState(kReceivedAccessCode);
}

void HostNPScriptObject::SetState(State state) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());
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
  if (!plugin_task_runner_->BelongsToCurrentThread()) {
    plugin_task_runner_->PostTask(
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
  if (plugin_task_runner_->BelongsToCurrentThread()) {
    // Make sure we're not currently processing a log message.
    // We only need to check this if we're on the plugin thread.
    if (am_currently_logging_)
      return;
  }

  // Always post (even if we're already on the correct thread) so that debug
  // log messages are shown in the correct order.
  plugin_task_runner_->PostTask(
      FROM_HERE, base::Bind(&HostNPScriptObject::LogDebugInfo,
                            base::Unretained(this), message));
}

void HostNPScriptObject::SetWindow(NPWindow* np_window) {
  daemon_controller_->SetWindow(np_window->window);
}

void HostNPScriptObject::LocalizeStrings(NPObject* localize_func) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

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
  LocalizeStringWithSubstitution(localize_func,
                                 /*i18n-content*/"MESSAGE_SHARED", "$1",
                                 &ui_strings.disconnect_message);

  base::AutoLock auto_lock(ui_strings_lock_);
  ui_strings_ = ui_strings;
}

bool HostNPScriptObject::LocalizeString(NPObject* localize_func,
                                        const char* tag, string16* result) {
  return LocalizeStringWithSubstitution(localize_func, tag, NULL, result);
}

bool HostNPScriptObject::LocalizeStringWithSubstitution(
    NPObject* localize_func,
    const char* tag,
    const char* substitution,
    string16* result) {
  int argc = substitution ? 2 : 1;
  scoped_array<NPVariant> args(new NPVariant[argc]);
  STRINGZ_TO_NPVARIANT(tag, args[0]);
  if (substitution) {
    STRINGZ_TO_NPVARIANT(substitution, args[1]);
  }
  NPVariant np_result;
  bool is_good = g_npnetscape_funcs->invokeDefault(
      plugin_, localize_func, args.get(), argc, &np_result);
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
  if (!plugin_task_runner_->BelongsToCurrentThread()) {
    plugin_task_runner_->PostTask(
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

void HostNPScriptObject::DoGenerateKeyPair(const ScopedRefNPObject& callback) {
  HostKeyPair key_pair;
  key_pair.Generate();
  InvokeGenerateKeyPairCallback(callback, key_pair.GetAsString(),
                                key_pair.GetPublicKey());
}

void HostNPScriptObject::InvokeGenerateKeyPairCallback(
    const ScopedRefNPObject& callback,
    const std::string& private_key,
    const std::string& public_key) {
  if (!plugin_task_runner_->BelongsToCurrentThread()) {
    plugin_task_runner_->PostTask(
        FROM_HERE, base::Bind(
            &HostNPScriptObject::InvokeGenerateKeyPairCallback,
            base::Unretained(this), callback, private_key, public_key));
    return;
  }

  NPVariant params[2];
  params[0] = NPVariantFromString(private_key);
  params[1] = NPVariantFromString(public_key);
  InvokeAndIgnoreResult(callback.get(), params, arraysize(params));
  g_npnetscape_funcs->releasevariantvalue(&(params[0]));
  g_npnetscape_funcs->releasevariantvalue(&(params[1]));
}

void HostNPScriptObject::InvokeAsyncResultCallback(
    const ScopedRefNPObject& callback,
    DaemonController::AsyncResult result) {
  if (!plugin_task_runner_->BelongsToCurrentThread()) {
    plugin_task_runner_->PostTask(
        FROM_HERE, base::Bind(
            &HostNPScriptObject::InvokeAsyncResultCallback,
            base::Unretained(this), callback, result));
    return;
  }

  NPVariant result_var;
  INT32_TO_NPVARIANT(static_cast<int32>(result), result_var);
  InvokeAndIgnoreResult(callback.get(), &result_var, 1);
  g_npnetscape_funcs->releasevariantvalue(&result_var);
}

void HostNPScriptObject::InvokeGetDaemonConfigCallback(
    const ScopedRefNPObject& callback,
    scoped_ptr<base::DictionaryValue> config) {
  if (!plugin_task_runner_->BelongsToCurrentThread()) {
    plugin_task_runner_->PostTask(
        FROM_HERE, base::Bind(
            &HostNPScriptObject::InvokeGetDaemonConfigCallback,
            base::Unretained(this), callback, base::Passed(&config)));
    return;
  }

  // There is no easy way to create a dictionary from an NPAPI plugin
  // so we have to serialize the dictionary to pass it to JavaScript.
  std::string config_str;
  if (config.get())
    base::JSONWriter::Write(config.get(), &config_str);

  NPVariant config_val = NPVariantFromString(config_str);
  InvokeAndIgnoreResult(callback.get(), &config_val, 1);
  g_npnetscape_funcs->releasevariantvalue(&config_val);
}

void HostNPScriptObject::InvokeGetDaemonVersionCallback(
    const ScopedRefNPObject& callback, const std::string& version) {
  if (!plugin_task_runner_->BelongsToCurrentThread()) {
    plugin_task_runner_->PostTask(
        FROM_HERE, base::Bind(
            &HostNPScriptObject::InvokeGetDaemonVersionCallback,
            base::Unretained(this), callback, version));
    return;
  }

  NPVariant version_val = NPVariantFromString(version);
  InvokeAndIgnoreResult(callback.get(), &version_val, 1);
  g_npnetscape_funcs->releasevariantvalue(&version_val);
}

void HostNPScriptObject::InvokeGetUsageStatsConsentCallback(
    const ScopedRefNPObject& callback,
    bool supported,
    bool allowed,
    bool set_by_policy) {
  if (!plugin_task_runner_->BelongsToCurrentThread()) {
    plugin_task_runner_->PostTask(
        FROM_HERE, base::Bind(
            &HostNPScriptObject::InvokeGetUsageStatsConsentCallback,
            base::Unretained(this), callback, supported, allowed,
            set_by_policy));
    return;
  }

  NPVariant params[3];
  BOOLEAN_TO_NPVARIANT(supported, params[0]);
  BOOLEAN_TO_NPVARIANT(allowed, params[1]);
  BOOLEAN_TO_NPVARIANT(set_by_policy, params[2]);
  InvokeAndIgnoreResult(callback.get(), params, arraysize(params));
  g_npnetscape_funcs->releasevariantvalue(&(params[0]));
  g_npnetscape_funcs->releasevariantvalue(&(params[1]));
  g_npnetscape_funcs->releasevariantvalue(&(params[2]));
}

void HostNPScriptObject::LogDebugInfo(const std::string& message) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
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

bool HostNPScriptObject::InvokeAndIgnoreResult(NPObject* func,
                                               const NPVariant* args,
                                               uint32_t arg_count) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
  NPVariant np_result;
  bool is_good = g_npnetscape_funcs->invokeDefault(plugin_, func, args,
                                                   arg_count, &np_result);
  if (is_good)
      g_npnetscape_funcs->releasevariantvalue(&np_result);
  return is_good;
}

void HostNPScriptObject::SetException(const std::string& exception_string) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
  g_npnetscape_funcs->setexception(parent_, exception_string.c_str());
  LOG(INFO) << exception_string;
}

}  // namespace remoting
