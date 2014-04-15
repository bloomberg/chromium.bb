// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/host_script_object.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "remoting/base/auth_token_util.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/logging.h"
#include "remoting/base/resources.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_config.h"
#include "remoting/host/pairing_registry_delegate.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/plugin/host_log_handler.h"
#include "remoting/host/policy_hack/policy_watcher.h"
#include "remoting/host/service_urls.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npfunctions.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace remoting {

namespace {

const char* kAttrNameAccessCode = "accessCode";
const char* kAttrNameAccessCodeLifetime = "accessCodeLifetime";
const char* kAttrNameClient = "client";
const char* kAttrNameDaemonState = "daemonState";
const char* kAttrNameState = "state";
const char* kAttrNameLogDebugInfo = "logDebugInfo";
const char* kAttrNameOnNatTraversalPolicyChanged =
    "onNatTraversalPolicyChanged";
const char* kAttrNameOnStateChanged = "onStateChanged";
const char* kAttrNameXmppServerAddress = "xmppServerAddress";
const char* kAttrNameXmppServerUseTls = "xmppServerUseTls";
const char* kAttrNameDirectoryBotJid = "directoryBotJid";
const char* kAttrNameSupportedFeatures = "supportedFeatures";
const char* kFuncNameConnect = "connect";
const char* kFuncNameDisconnect = "disconnect";
const char* kFuncNameLocalize = "localize";
const char* kFuncNameClearPairedClients = "clearPairedClients";
const char* kFuncNameDeletePairedClient = "deletePairedClient";
const char* kFuncNameGetHostName = "getHostName";
const char* kFuncNameGetPinHash = "getPinHash";
const char* kFuncNameGenerateKeyPair = "generateKeyPair";
const char* kFuncNameUpdateDaemonConfig = "updateDaemonConfig";
const char* kFuncNameGetDaemonConfig = "getDaemonConfig";
const char* kFuncNameGetDaemonVersion = "getDaemonVersion";
const char* kFuncNameGetPairedClients = "getPairedClients";
const char* kFuncNameGetUsageStatsConsent = "getUsageStatsConsent";
const char* kFuncNameInstallHost = "installHost";
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
const char* kAttrNameInvalidDomainError = "INVALID_DOMAIN_ERROR";

// Space separated list of features supported in addition to the base protocol.
const char* kSupportedFeatures = "pairingRegistry";

}  // namespace

HostNPScriptObject::HostNPScriptObject(
    NPP plugin,
    NPObject* parent,
    scoped_refptr<AutoThreadTaskRunner> plugin_task_runner)
    : plugin_(plugin),
      parent_(parent),
      plugin_task_runner_(plugin_task_runner),
      am_currently_logging_(false),
      state_(kDisconnected),
      weak_factory_(this) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  weak_ptr_ = weak_factory_.GetWeakPtr();

  // Set the thread task runner for the plugin thread so that timers and other
  // code using |base::ThreadTaskRunnerHandle| could be used on the plugin
  // thread.
  //
  // If component build is used, Chrome and the plugin may end up sharing base
  // binary. This means that the instance of |base::ThreadTaskRunnerHandle|
  // created by Chrome for the current thread is shared as well. This routinely
  // happens in the development setting so the below check for
  // |!base::ThreadTaskRunnerHandle::IsSet()| is a hack/workaround allowing this
  // configuration to work. It lets the plugin to access Chrome's message loop
  // directly via |base::ThreadTaskRunnerHandle|. This is safe as long as both
  // Chrome and the plugin are built from the same version of the sources.
  if (!base::ThreadTaskRunnerHandle::IsSet()) {
    plugin_task_runner_handle_.reset(
        new base::ThreadTaskRunnerHandle(plugin_task_runner_));
  }

  daemon_controller_ = DaemonController::Create();

  ServiceUrls* service_urls = ServiceUrls::GetInstance();
  bool xmpp_server_valid = net::ParseHostAndPort(
      service_urls->xmpp_server_address(),
      &xmpp_server_config_.host, &xmpp_server_config_.port);
  // For the plugin, this is always the default address, which must be valid.
  DCHECK(xmpp_server_valid);
  xmpp_server_config_.use_tls = service_urls->xmpp_server_use_tls();
  directory_bot_jid_ = service_urls->directory_bot_jid();

  // Create worker thread for encryption key generation and loading the paired
  // clients.
  worker_thread_ = AutoThread::Create("ChromotingWorkerThread",
                                      plugin_task_runner_);

  pairing_registry_ = CreatePairingRegistry(worker_thread_);
}

HostNPScriptObject::~HostNPScriptObject() {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  HostLogHandler::UnregisterLoggingScriptObject(this);

  // Stop the It2Me host if the caller forgot to.
  if (it2me_host_.get()) {
    it2me_host_->Disconnect();
    it2me_host_ = NULL;
  }
}

bool HostNPScriptObject::HasMethod(const std::string& method_name) {
  VLOG(2) << "HasMethod " << method_name;
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
  return (method_name == kFuncNameConnect ||
          method_name == kFuncNameDisconnect ||
          method_name == kFuncNameLocalize ||
          method_name == kFuncNameClearPairedClients ||
          method_name == kFuncNameDeletePairedClient ||
          method_name == kFuncNameGetHostName ||
          method_name == kFuncNameGetPinHash ||
          method_name == kFuncNameGenerateKeyPair ||
          method_name == kFuncNameUpdateDaemonConfig ||
          method_name == kFuncNameGetDaemonConfig ||
          method_name == kFuncNameGetDaemonVersion ||
          method_name == kFuncNameGetPairedClients ||
          method_name == kFuncNameGetUsageStatsConsent ||
          method_name == kFuncNameInstallHost ||
          method_name == kFuncNameStartDaemon ||
          method_name == kFuncNameStopDaemon);
}

bool HostNPScriptObject::InvokeDefault(const NPVariant* args,
                                       uint32_t arg_count,
                                       NPVariant* result) {
  VLOG(2) << "InvokeDefault";
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
  SetException("exception during default invocation");
  return false;
}

bool HostNPScriptObject::Invoke(const std::string& method_name,
                                const NPVariant* args,
                                uint32_t arg_count,
                                NPVariant* result) {
  VLOG(2) << "Invoke " << method_name;
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
  if (method_name == kFuncNameConnect) {
    return Connect(args, arg_count, result);
  } else if (method_name == kFuncNameDisconnect) {
    return Disconnect(args, arg_count, result);
  } else if (method_name == kFuncNameLocalize) {
    return Localize(args, arg_count, result);
  } else if (method_name == kFuncNameClearPairedClients) {
    return ClearPairedClients(args, arg_count, result);
  } else if (method_name == kFuncNameDeletePairedClient) {
    return DeletePairedClient(args, arg_count, result);
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
  } else if (method_name == kFuncNameGetPairedClients) {
    return GetPairedClients(args, arg_count, result);
  } else if (method_name == kFuncNameGetUsageStatsConsent) {
    return GetUsageStatsConsent(args, arg_count, result);
  } else if (method_name == kFuncNameInstallHost) {
    return InstallHost(args, arg_count, result);
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
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
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
          property_name == kAttrNameError ||
          property_name == kAttrNameXmppServerAddress ||
          property_name == kAttrNameXmppServerUseTls ||
          property_name == kAttrNameDirectoryBotJid ||
          property_name == kAttrNameSupportedFeatures);
}

bool HostNPScriptObject::GetProperty(const std::string& property_name,
                                     NPVariant* result) {
  VLOG(2) << "GetProperty " << property_name;
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
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
    *result = NPVariantFromString(access_code_);
    return true;
  } else if (property_name == kAttrNameAccessCodeLifetime) {
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
  } else if (property_name == kAttrNameInvalidDomainError) {
    INT32_TO_NPVARIANT(kInvalidDomainError, *result);
    return true;
  } else if (property_name == kAttrNameXmppServerAddress) {
    *result = NPVariantFromString(base::StringPrintf(
        "%s:%u", xmpp_server_config_.host.c_str(), xmpp_server_config_.port));
    return true;
  } else if (property_name == kAttrNameXmppServerUseTls) {
    BOOLEAN_TO_NPVARIANT(xmpp_server_config_.use_tls, *result);
    return true;
  } else if (property_name == kAttrNameDirectoryBotJid) {
    *result = NPVariantFromString(directory_bot_jid_);
    return true;
  } else if (property_name == kAttrNameSupportedFeatures) {
    *result = NPVariantFromString(kSupportedFeatures);
    return true;
  } else {
    SetException("GetProperty: unsupported property " + property_name);
    return false;
  }
}

bool HostNPScriptObject::SetProperty(const std::string& property_name,
                                     const NPVariant* value) {
  VLOG(2) <<  "SetProperty " << property_name;
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  if (property_name == kAttrNameOnNatTraversalPolicyChanged) {
    if (NPVARIANT_IS_OBJECT(*value)) {
      on_nat_traversal_policy_changed_func_ = NPVARIANT_TO_OBJECT(*value);
      if (it2me_host_.get()) {
        // Ask the It2Me host to notify the web-app of the policy.
        it2me_host_->RequestNatPolicy();
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

#if !defined(NDEBUG)
  if (property_name == kAttrNameXmppServerAddress) {
    if (NPVARIANT_IS_STRING(*value)) {
      std::string address = StringFromNPVariant(*value);
      bool xmpp_server_valid = net::ParseHostAndPort(
          address, &xmpp_server_config_.host, &xmpp_server_config_.port);
      if (xmpp_server_valid) {
        return true;
      } else {
        SetException("SetProperty: invalid value for property " +
                     property_name);
      }
    } else {
      SetException("SetProperty: unexpected type for property " +
                   property_name);
    }
    return false;
  }

  if (property_name == kAttrNameXmppServerUseTls) {
    if (NPVARIANT_IS_BOOLEAN(*value)) {
      xmpp_server_config_.use_tls = NPVARIANT_TO_BOOLEAN(*value);
      return true;
    } else {
      SetException("SetProperty: unexpected type for property " +
                   property_name);
    }
    return false;
  }

  if (property_name == kAttrNameDirectoryBotJid) {
    if (NPVARIANT_IS_STRING(*value)) {
      directory_bot_jid_ = StringFromNPVariant(*value);
      return true;
    } else {
      SetException("SetProperty: unexpected type for property " +
                   property_name);
    }
    return false;
  }
#endif  // !defined(NDEBUG)

  return false;
}

bool HostNPScriptObject::RemoveProperty(const std::string& property_name) {
  VLOG(2) << "RemoveProperty " << property_name;
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
  return false;
}

bool HostNPScriptObject::Enumerate(std::vector<std::string>* values) {
  VLOG(2) << "Enumerate";
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
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
    kAttrNameXmppServerAddress,
    kAttrNameXmppServerUseTls,
    kAttrNameDirectoryBotJid,
    kFuncNameConnect,
    kFuncNameDisconnect,
    kFuncNameLocalize,
    kFuncNameClearPairedClients,
    kFuncNameDeletePairedClient,
    kFuncNameGetHostName,
    kFuncNameGetPinHash,
    kFuncNameGenerateKeyPair,
    kFuncNameUpdateDaemonConfig,
    kFuncNameGetDaemonConfig,
    kFuncNameGetDaemonVersion,
    kFuncNameGetPairedClients,
    kFuncNameGetUsageStatsConsent,
    kFuncNameInstallHost,
    kFuncNameStartDaemon,
    kFuncNameStopDaemon
  };
  for (size_t i = 0; i < arraysize(entries); ++i) {
    values->push_back(entries[i]);
  }
  return true;
}

// string username, string auth_token
bool HostNPScriptObject::Connect(const NPVariant* args,
                                 uint32_t arg_count,
                                 NPVariant* result) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  HOST_LOG << "Connecting...";

  if (arg_count != 2) {
    SetException("connect: bad number of arguments");
    return false;
  }

  if (it2me_host_.get()) {
    SetException("connect: can be called only when disconnected");
    return false;
  }

  XmppSignalStrategy::XmppServerConfig xmpp_config = xmpp_server_config_;

  xmpp_config.username = StringFromNPVariant(args[0]);
  if (xmpp_config.username.empty()) {
    SetException("connect: bad username argument");
    return false;
  }

  std::string auth_service_with_token = StringFromNPVariant(args[1]);
  ParseAuthTokenWithService(auth_service_with_token, &xmpp_config.auth_token,
                            &xmpp_config.auth_service);
  if (xmpp_config.auth_token.empty()) {
    SetException("connect: auth_service_with_token argument has empty token");
    return false;
  }

  // Create a host context to manage the threads for the it2me host.
  // The plugin, rather than the It2MeHost object, owns and maintains the
  // lifetime of the host context.
  host_context_.reset(
      ChromotingHostContext::Create(plugin_task_runner_).release());
  if (!host_context_) {
    SetException("connect: failed to start threads");
    return false;
  }

  // Create the It2Me host and start connecting.
  scoped_ptr<It2MeHostFactory> factory(new It2MeHostFactory());
  it2me_host_ = factory->CreateIt2MeHost(
      host_context_.get(), plugin_task_runner_, weak_ptr_,
      xmpp_config, directory_bot_jid_);
  it2me_host_->Connect();

  return true;
}

bool HostNPScriptObject::Disconnect(const NPVariant* args,
                                    uint32_t arg_count,
                                    NPVariant* result) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
  if (arg_count != 0) {
    SetException("disconnect: bad number of arguments");
    return false;
  }

  if (it2me_host_.get()) {
    it2me_host_->Disconnect();
    it2me_host_ = NULL;
  }

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

bool HostNPScriptObject::ClearPairedClients(const NPVariant* args,
                                            uint32_t arg_count,
                                            NPVariant* result) {
  if (arg_count != 1) {
    SetException("clearPairedClients: bad number of arguments");
    return false;
  }

  if (!NPVARIANT_IS_OBJECT(args[0])) {
    SetException("clearPairedClients: invalid callback parameter");
    return false;
  }

  scoped_ptr<ScopedRefNPObject> callback_obj(
      new ScopedRefNPObject(ObjectFromNPVariant(args[0])));
  if (pairing_registry_) {
    pairing_registry_->ClearAllPairings(
        base::Bind(&HostNPScriptObject::InvokeBooleanCallback, weak_ptr_,
                   base::Passed(&callback_obj)));
  } else {
    InvokeBooleanCallback(callback_obj.Pass(), false);
  }

  return true;
}

bool HostNPScriptObject::DeletePairedClient(const NPVariant* args,
                                            uint32_t arg_count,
                                            NPVariant* result) {
  if (arg_count != 2) {
    SetException("deletePairedClient: bad number of arguments");
    return false;
  }

  if (!NPVARIANT_IS_STRING(args[0])) {
    SetException("deletePairedClient: bad clientId parameter");
    return false;
  }

  if (!NPVARIANT_IS_OBJECT(args[1])) {
    SetException("deletePairedClient: invalid callback parameter");
    return false;
  }

  std::string client_id = StringFromNPVariant(args[0]);
  scoped_ptr<ScopedRefNPObject> callback_obj(
      new ScopedRefNPObject(ObjectFromNPVariant(args[1])));
  if (pairing_registry_) {
    pairing_registry_->DeletePairing(
        client_id,
        base::Bind(&HostNPScriptObject::InvokeBooleanCallback,
                   weak_ptr_, base::Passed(&callback_obj)));
  } else {
    InvokeBooleanCallback(callback_obj.Pass(), false);
  }

  return true;
}

bool HostNPScriptObject::GetHostName(const NPVariant* args,
                                     uint32_t arg_count,
                                     NPVariant* result) {
  if (arg_count != 1) {
    SetException("getHostName: bad number of arguments");
    return false;
  }

  ScopedRefNPObject callback_obj(ObjectFromNPVariant(args[0]));
  if (!callback_obj.get()) {
    SetException("getHostName: invalid callback parameter");
    return false;
  }

  NPVariant host_name_val = NPVariantFromString(net::GetHostName());
  InvokeAndIgnoreResult(callback_obj, &host_name_val, 1);
  g_npnetscape_funcs->releasevariantvalue(&host_name_val);

  return true;
}

bool HostNPScriptObject::GetPinHash(const NPVariant* args,
                                    uint32_t arg_count,
                                    NPVariant* result) {
  if (arg_count != 3) {
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

  ScopedRefNPObject callback_obj(ObjectFromNPVariant(args[2]));
  if (!callback_obj.get()) {
    SetException("getPinHash: invalid callback parameter");
    return false;
  }

  NPVariant pin_hash_val = NPVariantFromString(
      remoting::MakeHostPinHash(host_id, pin));
  InvokeAndIgnoreResult(callback_obj, &pin_hash_val, 1);
  g_npnetscape_funcs->releasevariantvalue(&pin_hash_val);

  return true;
}

bool HostNPScriptObject::GenerateKeyPair(const NPVariant* args,
                                         uint32_t arg_count,
                                         NPVariant* result) {
  if (arg_count != 1) {
    SetException("generateKeyPair: bad number of arguments");
    return false;
  }

  scoped_ptr<ScopedRefNPObject> callback_obj(
      new ScopedRefNPObject(ObjectFromNPVariant(args[0])));
  if (!callback_obj->get()) {
    SetException("generateKeyPair: invalid callback parameter");
    return false;
  }

  base::Callback<void (const std::string&,
                       const std::string&)> wrapped_callback =
      base::Bind(&HostNPScriptObject::InvokeGenerateKeyPairCallback, weak_ptr_,
                 base::Passed(&callback_obj));
  worker_thread_->PostTask(
      FROM_HERE, base::Bind(&HostNPScriptObject::DoGenerateKeyPair,
                            plugin_task_runner_, wrapped_callback));
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

  scoped_ptr<ScopedRefNPObject> callback_obj(
      new ScopedRefNPObject(ObjectFromNPVariant(args[1])));
  if (!callback_obj->get()) {
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
      base::Bind(&HostNPScriptObject::InvokeAsyncResultCallback, weak_ptr_,
                 base::Passed(&callback_obj)));
  return true;
}

bool HostNPScriptObject::GetDaemonConfig(const NPVariant* args,
                                         uint32_t arg_count,
                                         NPVariant* result) {
  if (arg_count != 1) {
    SetException("getDaemonConfig: bad number of arguments");
    return false;
  }

  scoped_ptr<ScopedRefNPObject> callback_obj(
      new ScopedRefNPObject(ObjectFromNPVariant(args[0])));
  if (!callback_obj->get()) {
    SetException("getDaemonConfig: invalid callback parameter");
    return false;
  }

  daemon_controller_->GetConfig(
      base::Bind(&HostNPScriptObject::InvokeGetDaemonConfigCallback, weak_ptr_,
                 base::Passed(&callback_obj)));
  return true;
}

bool HostNPScriptObject::GetDaemonVersion(const NPVariant* args,
                                          uint32_t arg_count,
                                          NPVariant* result) {
  if (arg_count != 1) {
    SetException("getDaemonVersion: bad number of arguments");
    return false;
  }

  scoped_ptr<ScopedRefNPObject> callback_obj(
      new ScopedRefNPObject(ObjectFromNPVariant(args[0])));
  if (!callback_obj->get()) {
    SetException("getDaemonVersion: invalid callback parameter");
    return false;
  }

  daemon_controller_->GetVersion(
      base::Bind(&HostNPScriptObject::InvokeGetDaemonVersionCallback, weak_ptr_,
                 base::Passed(&callback_obj)));

  return true;
}

bool HostNPScriptObject::GetPairedClients(const NPVariant* args,
                                          uint32_t arg_count,
                                          NPVariant* result) {
  if (arg_count != 1) {
    SetException("getPairedClients: bad number of arguments");
    return false;
  }

  scoped_ptr<ScopedRefNPObject> callback_obj(
      new ScopedRefNPObject(ObjectFromNPVariant(args[0])));
  if (!callback_obj->get()) {
    SetException("getPairedClients: invalid callback parameter");
    return false;
  }

  if (pairing_registry_) {
    pairing_registry_->GetAllPairings(
        base::Bind(&HostNPScriptObject::InvokeGetPairedClientsCallback,
                   weak_ptr_, base::Passed(&callback_obj)));
  } else {
    scoped_ptr<base::ListValue> no_paired_clients(new base::ListValue);
    InvokeGetPairedClientsCallback(callback_obj.Pass(),
                                   no_paired_clients.Pass());
  }
  return true;
}

bool HostNPScriptObject::GetUsageStatsConsent(const NPVariant* args,
                                              uint32_t arg_count,
                                              NPVariant* result) {
  if (arg_count != 1) {
    SetException("getUsageStatsConsent: bad number of arguments");
    return false;
  }

  scoped_ptr<ScopedRefNPObject> callback_obj(
      new ScopedRefNPObject(ObjectFromNPVariant(args[0])));
  if (!callback_obj->get()) {
    SetException("getUsageStatsConsent: invalid callback parameter");
    return false;
  }

  daemon_controller_->GetUsageStatsConsent(
      base::Bind(&HostNPScriptObject::InvokeGetUsageStatsConsentCallback,
                 weak_ptr_, base::Passed(&callback_obj)));
  return true;
}

bool HostNPScriptObject::InstallHost(const NPVariant* args,
                                     uint32_t arg_count,
                                     NPVariant* result) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  if (arg_count != 1) {
    SetException("installHost: bad number of arguments");
    return false;
  }

  scoped_ptr<ScopedRefNPObject> callback_obj(
      new ScopedRefNPObject(ObjectFromNPVariant(args[0])));
  if (!callback_obj->get()) {
    SetException("installHost: invalid callback parameter");
    return false;
  }

  daemon_controller_->InstallHost(
      base::Bind(&HostNPScriptObject::InvokeAsyncResultCallback, weak_ptr_,
                 base::Passed(&callback_obj)));
  return true;
}

bool HostNPScriptObject::StartDaemon(const NPVariant* args,
                                     uint32_t arg_count,
                                     NPVariant* result) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

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

  scoped_ptr<ScopedRefNPObject> callback_obj(
      new ScopedRefNPObject(ObjectFromNPVariant(args[2])));
  if (!callback_obj->get()) {
    SetException("startDaemon: invalid callback parameter");
    return false;
  }

  daemon_controller_->SetConfigAndStart(
      config_dict.Pass(),
      NPVARIANT_TO_BOOLEAN(args[1]),
      base::Bind(&HostNPScriptObject::InvokeAsyncResultCallback, weak_ptr_,
                 base::Passed(&callback_obj)));
  return true;
}

bool HostNPScriptObject::StopDaemon(const NPVariant* args,
                                    uint32_t arg_count,
                                    NPVariant* result) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  if (arg_count != 1) {
    SetException("stopDaemon: bad number of arguments");
    return false;
  }

  scoped_ptr<ScopedRefNPObject> callback_obj(
      new ScopedRefNPObject(ObjectFromNPVariant(args[0])));
  if (!callback_obj->get()) {
    SetException("stopDaemon: invalid callback parameter");
    return false;
  }

  daemon_controller_->Stop(
      base::Bind(&HostNPScriptObject::InvokeAsyncResultCallback, weak_ptr_,
                 base::Passed(&callback_obj)));
  return true;
}

void HostNPScriptObject::OnStateChanged(It2MeHostState state) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  state_ = state;

  if (state_ == kDisconnected)
    client_username_.clear();

  if (on_state_changed_func_.get()) {
    NPVariant state_var;
    INT32_TO_NPVARIANT(state, state_var);
    InvokeAndIgnoreResult(on_state_changed_func_, &state_var, 1);
  }
}

void HostNPScriptObject::OnNatPolicyChanged(bool nat_traversal_enabled) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  if (on_nat_traversal_policy_changed_func_.get()) {
    NPVariant policy;
    BOOLEAN_TO_NPVARIANT(nat_traversal_enabled, policy);
    InvokeAndIgnoreResult(on_nat_traversal_policy_changed_func_,
                          &policy, 1);
  }
}

// Stores the Access Code for the web-app to query.
void HostNPScriptObject::OnStoreAccessCode(
    const std::string& access_code, base::TimeDelta access_code_lifetime) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  access_code_ = access_code;
  access_code_lifetime_ = access_code_lifetime;
}

// Stores the client user's name for the web-app to query.
void HostNPScriptObject::OnClientAuthenticated(
    const std::string& client_username) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  client_username_ = client_username;
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
                            weak_ptr_, message));
}

void HostNPScriptObject::SetWindow(NPWindow* np_window) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  daemon_controller_->SetWindow(np_window->window);
}

void HostNPScriptObject::LocalizeStrings(NPObject* localize_func) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  // Reload resources for the current locale. The default UI locale is used on
  // Windows.
#if !defined(OS_WIN)
  base::string16 ui_locale;
  LocalizeString(localize_func, "@@ui_locale", &ui_locale);
  remoting::LoadResources(base::UTF16ToUTF8(ui_locale));
#endif  // !defined(OS_WIN)
}

bool HostNPScriptObject::LocalizeString(NPObject* localize_func,
                                        const char* tag,
                                        base::string16* result) {
  return LocalizeStringWithSubstitution(localize_func, tag, NULL, result);
}

bool HostNPScriptObject::LocalizeStringWithSubstitution(
    NPObject* localize_func,
    const char* tag,
    const char* substitution,
    base::string16* result) {
  int argc = substitution ? 2 : 1;
  scoped_ptr<NPVariant[]> args(new NPVariant[argc]);
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
  *result = base::UTF8ToUTF16(translation);
  return true;
}

// static
void HostNPScriptObject::DoGenerateKeyPair(
    const scoped_refptr<AutoThreadTaskRunner>& plugin_task_runner,
    const base::Callback<void (const std::string&,
                               const std::string&)>& callback) {
  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::Generate();
  plugin_task_runner->PostTask(FROM_HERE,
                               base::Bind(callback, key_pair->ToString(),
                                          key_pair->GetPublicKey()));
}

void HostNPScriptObject::InvokeGenerateKeyPairCallback(
    scoped_ptr<ScopedRefNPObject> callback,
    const std::string& private_key,
    const std::string& public_key) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  NPVariant params[2];
  params[0] = NPVariantFromString(private_key);
  params[1] = NPVariantFromString(public_key);
  InvokeAndIgnoreResult(*callback, params, arraysize(params));
  g_npnetscape_funcs->releasevariantvalue(&(params[0]));
  g_npnetscape_funcs->releasevariantvalue(&(params[1]));
}

void HostNPScriptObject::InvokeAsyncResultCallback(
    scoped_ptr<ScopedRefNPObject> callback,
    DaemonController::AsyncResult result) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  NPVariant result_var;
  INT32_TO_NPVARIANT(static_cast<int32>(result), result_var);
  InvokeAndIgnoreResult(*callback, &result_var, 1);
  g_npnetscape_funcs->releasevariantvalue(&result_var);
}

void HostNPScriptObject::InvokeBooleanCallback(
    scoped_ptr<ScopedRefNPObject> callback, bool result) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  NPVariant result_var;
  BOOLEAN_TO_NPVARIANT(result, result_var);
  InvokeAndIgnoreResult(*callback, &result_var, 1);
  g_npnetscape_funcs->releasevariantvalue(&result_var);
}

void HostNPScriptObject::InvokeGetDaemonConfigCallback(
    scoped_ptr<ScopedRefNPObject> callback,
    scoped_ptr<base::DictionaryValue> config) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  // There is no easy way to create a dictionary from an NPAPI plugin
  // so we have to serialize the dictionary to pass it to JavaScript.
  std::string config_str;
  if (config.get())
    base::JSONWriter::Write(config.get(), &config_str);

  NPVariant config_val = NPVariantFromString(config_str);
  InvokeAndIgnoreResult(*callback, &config_val, 1);
  g_npnetscape_funcs->releasevariantvalue(&config_val);
}

void HostNPScriptObject::InvokeGetDaemonVersionCallback(
    scoped_ptr<ScopedRefNPObject> callback, const std::string& version) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  NPVariant version_val = NPVariantFromString(version);
  InvokeAndIgnoreResult(*callback, &version_val, 1);
  g_npnetscape_funcs->releasevariantvalue(&version_val);
}

void HostNPScriptObject::InvokeGetPairedClientsCallback(
    scoped_ptr<ScopedRefNPObject> callback,
    scoped_ptr<base::ListValue> paired_clients) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  std::string paired_clients_json;
  base::JSONWriter::Write(paired_clients.get(), &paired_clients_json);

  NPVariant paired_clients_val = NPVariantFromString(paired_clients_json);
  InvokeAndIgnoreResult(*callback, &paired_clients_val, 1);
  g_npnetscape_funcs->releasevariantvalue(&paired_clients_val);
}

void HostNPScriptObject::InvokeGetUsageStatsConsentCallback(
    scoped_ptr<ScopedRefNPObject> callback,
    const DaemonController::UsageStatsConsent& consent) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  NPVariant params[3];
  BOOLEAN_TO_NPVARIANT(consent.supported, params[0]);
  BOOLEAN_TO_NPVARIANT(consent.allowed, params[1]);
  BOOLEAN_TO_NPVARIANT(consent.set_by_policy, params[2]);
  InvokeAndIgnoreResult(*callback, params, arraysize(params));
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
    bool is_good = InvokeAndIgnoreResult(log_debug_info_func_,
                                         &log_message, 1);
    if (!is_good) {
      LOG(ERROR) << "ERROR - LogDebugInfo failed\n";
    }
    am_currently_logging_ = false;
  }
}

bool HostNPScriptObject::InvokeAndIgnoreResult(const ScopedRefNPObject& func,
                                               const NPVariant* args,
                                               uint32_t arg_count) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  NPVariant np_result;
  bool is_good = g_npnetscape_funcs->invokeDefault(plugin_, func.get(), args,
                                                   arg_count, &np_result);
  if (is_good)
      g_npnetscape_funcs->releasevariantvalue(&np_result);

  return is_good;
}

void HostNPScriptObject::SetException(const std::string& exception_string) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  g_npnetscape_funcs->setexception(parent_, exception_string.c_str());
  HOST_LOG << exception_string;
}

}  // namespace remoting
