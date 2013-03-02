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
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/net_util.h"
#include "remoting/base/auth_token_util.h"
#include "remoting/base/auto_thread.h"
#include "remoting/host/basic_desktop_environment.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/host_secret.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/it2me_host_user_interface.h"
#include "remoting/host/network_settings.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/plugin/host_log_handler.h"
#include "remoting/host/policy_hack/policy_watcher.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/host/service_urls.h"
#include "remoting/host/session_manager_factory.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "remoting/protocol/it2me_host_authenticator_factory.h"
#include "third_party/npapi/bindings/npruntime.h"

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
const char* kAttrNameXmppServerAddress = "xmppServerAddress";
const char* kAttrNameXmppServerUseTls = "xmppServerUseTls";
const char* kAttrNameDirectoryBotJid = "directoryBotJid";
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
const char* kAttrNameInvalidDomainError = "INVALID_DOMAIN_ERROR";

const int kMaxLoginAttempts = 5;

}  // namespace

// Internal implementation of the plugin's It2Me host function.
class HostNPScriptObject::It2MeImpl
    : public base::RefCountedThreadSafe<It2MeImpl>,
      public HostStatusObserver {
 public:
  It2MeImpl(
      scoped_ptr<ChromotingHostContext> context,
      scoped_refptr<base::SingleThreadTaskRunner> plugin_task_runner,
      base::WeakPtr<HostNPScriptObject> script_object,
      const XmppSignalStrategy::XmppServerConfig& xmpp_server_config,
      const std::string& directory_bot_jid);

  // Methods called by the script object, from the plugin thread.

  // Creates It2Me host structures and starts the host.
  void Connect(const std::string& uid,
               const std::string& auth_token,
               const std::string& auth_service,
               const UiStrings& ui_strings);

  // Disconnects the host, ready for tear-down.
  // Also called internally, from the network thread.
  void Disconnect();

  // Request a NAT policy notification.
  void RequestNatPolicy();

  // remoting::HostStatusObserver implementation.
  virtual void OnAccessDenied(const std::string& jid) OVERRIDE;
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<It2MeImpl>;

  virtual ~It2MeImpl();

  // Updates state of the host. Can be called only on the network thread.
  void SetState(State state);

  // Returns true if the host is connected.
  bool IsConnected() const;

  // Called by Connect() to check for policies and start connection process.
  void ReadPolicyAndConnect(const std::string& uid,
                            const std::string& auth_token,
                            const std::string& auth_service);

  // Called by ReadPolicyAndConnect once policies have been read.
  void FinishConnect(const std::string& uid,
                     const std::string& auth_token,
                     const std::string& auth_service);

  // Called when the support host registration completes.
  void OnReceivedSupportID(bool success,
                           const std::string& support_id,
                           const base::TimeDelta& lifetime);

  // Called when ChromotingHost::Shutdown() has completed.
  void OnShutdownFinished();

  // Called when initial policies are read, and when they change.
  void OnPolicyUpdate(scoped_ptr<base::DictionaryValue> policies);

  // Handlers for NAT traversal and host domain policies.
  void UpdateNatPolicy(bool nat_traversal_enabled);
  void UpdateHostDomainPolicy(const std::string& host_domain);

  // Caller supplied fields.
  scoped_ptr<ChromotingHostContext> host_context_;
  scoped_refptr<base::SingleThreadTaskRunner> plugin_task_runner_;
  base::WeakPtr<HostNPScriptObject> script_object_;
  XmppSignalStrategy::XmppServerConfig xmpp_server_config_;
  std::string directory_bot_jid_;

  State state_;

  HostKeyPair host_key_pair_;
  scoped_ptr<SignalStrategy> signal_strategy_;
  scoped_ptr<RegisterSupportHostRequest> register_request_;
  scoped_ptr<LogToServer> log_to_server_;
  scoped_ptr<DesktopEnvironmentFactory> desktop_environment_factory_;
  scoped_ptr<It2MeHostUserInterface> it2me_host_user_interface_;
  scoped_ptr<HostEventLogger> host_event_logger_;

  scoped_refptr<ChromotingHost> host_;
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

HostNPScriptObject::It2MeImpl::It2MeImpl(
    scoped_ptr<ChromotingHostContext> host_context,
    scoped_refptr<base::SingleThreadTaskRunner> plugin_task_runner,
    base::WeakPtr<HostNPScriptObject> script_object,
    const XmppSignalStrategy::XmppServerConfig& xmpp_server_config,
    const std::string& directory_bot_jid)
  : host_context_(host_context.Pass()),
    plugin_task_runner_(plugin_task_runner),
    script_object_(script_object),
    xmpp_server_config_(xmpp_server_config),
    directory_bot_jid_(directory_bot_jid),
    state_(kDisconnected),
    failed_login_attempts_(0),
    nat_traversal_enabled_(false),
    policy_received_(false) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
}

void HostNPScriptObject::It2MeImpl::Connect(
    const std::string& uid,
    const std::string& auth_token,
    const std::string& auth_service,
    const UiStrings& ui_strings) {
  if (!host_context_->ui_task_runner()->BelongsToCurrentThread()) {
    DCHECK(plugin_task_runner_->BelongsToCurrentThread());
    host_context_->ui_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&It2MeImpl::Connect, this, uid, auth_token, auth_service,
                   ui_strings));
    return;
  }

  // Create the desktop environment factory. Do not use X DAMAGE, since it is
  // broken on many systems - see http://crbug.com/73423.
  desktop_environment_factory_.reset(new BasicDesktopEnvironmentFactory(false));

  // Start monitoring configured policies.
  policy_watcher_.reset(
      policy_hack::PolicyWatcher::Create(host_context_->network_task_runner()));
  policy_watcher_->StartWatching(
      base::Bind(&It2MeImpl::OnPolicyUpdate, this));

  // The UserInterface object needs to be created on the UI thread.
  it2me_host_user_interface_.reset(
      new It2MeHostUserInterface(host_context_->network_task_runner(),
                                 host_context_->ui_task_runner(), ui_strings));
  it2me_host_user_interface_->Init();

  // Switch to the network thread to start the actual connection.
  host_context_->network_task_runner()->PostTask(
      FROM_HERE, base::Bind(
          &It2MeImpl::ReadPolicyAndConnect, this,
          uid, auth_token, auth_service));
}

void HostNPScriptObject::It2MeImpl::Disconnect() {
  if (!host_context_->network_task_runner()->BelongsToCurrentThread()) {
    DCHECK(plugin_task_runner_->BelongsToCurrentThread());
    host_context_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&It2MeImpl::Disconnect, this));
    return;
  }

  switch (state_) {
    case kDisconnected:
      OnShutdownFinished();
      return;

    case kStarting:
      SetState(kDisconnecting);
      SetState(kDisconnected);
      OnShutdownFinished();
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
              base::Bind(&It2MeImpl::OnShutdownFinished, this)));
      return;
  }
}

void HostNPScriptObject::It2MeImpl::RequestNatPolicy() {
  if (!host_context_->network_task_runner()->BelongsToCurrentThread()) {
    DCHECK(plugin_task_runner_->BelongsToCurrentThread());
    host_context_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&It2MeImpl::RequestNatPolicy, this));
    return;
  }

  if (policy_received_)
    UpdateNatPolicy(nat_traversal_enabled_);
}

void HostNPScriptObject::It2MeImpl::ReadPolicyAndConnect(
    const std::string& uid,
    const std::string& auth_token,
    const std::string& auth_service) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  SetState(kStarting);

  // Only proceed to FinishConnect() if at least one policy update has been
  // received.
  if (policy_received_) {
    FinishConnect(uid, auth_token, auth_service);
  } else {
    // Otherwise, create the policy watcher, and thunk the connect.
    pending_connect_ =
        base::Bind(&It2MeImpl::FinishConnect, this,
                   uid, auth_token, auth_service);
  }
}

void HostNPScriptObject::It2MeImpl::FinishConnect(
    const std::string& uid,
    const std::string& auth_token,
    const std::string& auth_service) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  if (state_ != kStarting) {
    // Host has been stopped while we were fetching policy.
    return;
  }

  // Check the host domain policy.
  if (!required_host_domain_.empty() &&
      !EndsWith(uid, std::string("@") + required_host_domain_, false)) {
    SetState(kInvalidDomainError);
    return;
  }

  // Generate a key pair for the Host to use.
  // TODO(wez): Move this to the worker thread.
  host_key_pair_.Generate();

  // Create XMPP connection.
  scoped_ptr<SignalStrategy> signal_strategy(
      new XmppSignalStrategy(host_context_->url_request_context_getter(),
                             uid, auth_token, auth_service,
                             xmpp_server_config_));

  // Request registration of the host for support.
  scoped_ptr<RegisterSupportHostRequest> register_request(
      new RegisterSupportHostRequest(
          signal_strategy.get(), &host_key_pair_, directory_bot_jid_,
          base::Bind(&It2MeImpl::OnReceivedSupportID,
                     base::Unretained(this))));

  // Beyond this point nothing can fail, so save the config and request.
  signal_strategy_ = signal_strategy.Pass();
  register_request_ = register_request.Pass();

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

  // Create the host.
  host_ = new ChromotingHost(
      signal_strategy_.get(),
      desktop_environment_factory_.get(),
      CreateHostSessionManager(network_settings,
                               host_context_->url_request_context_getter()),
      host_context_->audio_task_runner(),
      host_context_->input_task_runner(),
      host_context_->video_capture_task_runner(),
      host_context_->video_encode_task_runner(),
      host_context_->network_task_runner(),
      host_context_->ui_task_runner());
  host_->AddStatusObserver(this);
  log_to_server_.reset(
      new LogToServer(host_->AsWeakPtr(), ServerLogEntry::IT2ME,
                      signal_strategy_.get(), directory_bot_jid_));

  // Disable audio by default.
  // TODO(sergeyu): Add UI to enable it.
  scoped_ptr<protocol::CandidateSessionConfig> protocol_config =
      protocol::CandidateSessionConfig::CreateDefault();
  protocol::CandidateSessionConfig::DisableAudioChannel(protocol_config.get());
  host_->set_protocol_config(protocol_config.Pass());

  // Create user interface.
  it2me_host_user_interface_->Start(host_.get(),
                                    base::Bind(&It2MeImpl::Disconnect, this));

  // Create event logger.
  host_event_logger_ =
      HostEventLogger::Create(host_->AsWeakPtr(), kApplicationName);

  // Connect signaling and start the host.
  signal_strategy_->Connect();
  host_->Start(uid);

  SetState(kRequestedAccessCode);
  return;
}

void HostNPScriptObject::It2MeImpl::OnShutdownFinished() {
  if (!host_context_->ui_task_runner()->BelongsToCurrentThread()) {
    host_context_->ui_task_runner()->PostTask(
        FROM_HERE, base::Bind(&It2MeImpl::OnShutdownFinished, this));
    return;
  }

  // Note that OnShutdownFinished() may be called more than once.

  // UI needs to be shut down on the UI thread before we destroy the
  // host context (because it depends on the context object), but
  // only after the host has been shut down (becase the UI object is
  // registered as status observer for the host, and we can't
  // unregister it from this thread).
  it2me_host_user_interface_.reset();

  // Destroy the DesktopEnvironmentFactory, to free thread references.
  desktop_environment_factory_.reset();

  // Stop listening for policy updates.
  if (policy_watcher_.get()) {
    base::WaitableEvent policy_watcher_stopped_(true, false);
    policy_watcher_->StopWatching(&policy_watcher_stopped_);
    policy_watcher_stopped_.Wait();
    policy_watcher_.reset();
  }
}

void HostNPScriptObject::It2MeImpl::OnAccessDenied(const std::string& jid) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  ++failed_login_attempts_;
  if (failed_login_attempts_ == kMaxLoginAttempts) {
    Disconnect();
  }
}

void HostNPScriptObject::It2MeImpl::OnClientAuthenticated(
    const std::string& jid) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  if (state_ == kDisconnecting) {
    // Ignore the new connection if we are disconnecting.
    return;
  }
  if (state_ == kConnected) {
    // If we already connected another client then one of the connections may be
    // an attacker, so both are suspect and we have to reject the second
    // connection and shutdown the host.
    host_->RejectAuthenticatingClient();
    Disconnect();
    return;
  }

  std::string client_username = jid;
  size_t pos = client_username.find('/');
  if (pos != std::string::npos)
    client_username.replace(pos, std::string::npos, "");

  LOG(INFO) << "Client " << client_username << " connected.";

  // Pass the client user name to the script object before changing state.
  plugin_task_runner_->PostTask(
      FROM_HERE, base::Bind(&HostNPScriptObject::StoreClientUsername,
                            script_object_, client_username));

  SetState(kConnected);
}

void HostNPScriptObject::It2MeImpl::OnClientDisconnected(
    const std::string& jid) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  // Pass the client user name to the script object before changing state.
  plugin_task_runner_->PostTask(
      FROM_HERE, base::Bind(&HostNPScriptObject::StoreClientUsername,
                            script_object_, std::string()));

  Disconnect();
}

void HostNPScriptObject::It2MeImpl::OnShutdown() {
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

void HostNPScriptObject::It2MeImpl::OnPolicyUpdate(
    scoped_ptr<base::DictionaryValue> policies) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

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

  policy_received_ = true;

  if (!pending_connect_.is_null()) {
    pending_connect_.Run();
    pending_connect_.Reset();
  }
}

void HostNPScriptObject::It2MeImpl::UpdateNatPolicy(
    bool nat_traversal_enabled) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateNatPolicy: " << nat_traversal_enabled;

  // When transitioning from enabled to disabled, force disconnect any
  // existing session.
  if (nat_traversal_enabled_ && !nat_traversal_enabled && IsConnected()) {
    Disconnect();
  }

  nat_traversal_enabled_ = nat_traversal_enabled;

  // Notify the web-app of the policy setting.
  plugin_task_runner_->PostTask(
      FROM_HERE, base::Bind(&HostNPScriptObject::NotifyNatPolicyChanged,
                            script_object_, nat_traversal_enabled_));
}

void HostNPScriptObject::It2MeImpl::UpdateHostDomainPolicy(
    const std::string& host_domain) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateHostDomainPolicy: " << host_domain;

  // When setting a host domain policy, force disconnect any existing session.
  if (!host_domain.empty() && IsConnected()) {
    Disconnect();
  }

  required_host_domain_ = host_domain;
}

HostNPScriptObject::It2MeImpl::~It2MeImpl() {
  // Check that resources that need to be torn down on the UI thread are gone.
  DCHECK(!it2me_host_user_interface_.get());
  DCHECK(!desktop_environment_factory_.get());
  DCHECK(!policy_watcher_.get());
}

void HostNPScriptObject::It2MeImpl::SetState(State state) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  switch (state_) {
    case kDisconnected:
      DCHECK(state == kStarting ||
             state == kError) << state;
      break;
    case kStarting:
      DCHECK(state == kRequestedAccessCode ||
             state == kDisconnecting ||
             state == kError ||
             state == kInvalidDomainError) << state;
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
    case kInvalidDomainError:
      DCHECK(state == kDisconnecting) << state;
      break;
  };

  state_ = state;

  // Post a state-change notification to the web-app.
  plugin_task_runner_->PostTask(
      FROM_HERE, base::Bind(&HostNPScriptObject::NotifyStateChanged,
                            script_object_, state));
}

bool HostNPScriptObject::It2MeImpl::IsConnected() const {
  return state_ == kRequestedAccessCode || state_ == kReceivedAccessCode ||
      state_ == kConnected;
}

void HostNPScriptObject::It2MeImpl::OnReceivedSupportID(
    bool success,
    const std::string& support_id,
    const base::TimeDelta& lifetime) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  if (!success) {
    SetState(kError);
    Disconnect();
    return;
  }

  std::string host_secret = GenerateSupportHostSecret();
  std::string access_code = support_id + host_secret;

  std::string local_certificate = host_key_pair_.GenerateCertificate();
  if (local_certificate.empty()) {
    LOG(ERROR) << "Failed to generate host certificate.";
    SetState(kError);
    Disconnect();
    return;
  }

  scoped_ptr<protocol::AuthenticatorFactory> factory(
      new protocol::It2MeHostAuthenticatorFactory(
          local_certificate, *host_key_pair_.private_key(), access_code));
  host_->SetAuthenticatorFactory(factory.Pass());

  // Pass the Access Code to the script object before changing state.
  plugin_task_runner_->PostTask(
      FROM_HERE, base::Bind(&HostNPScriptObject::StoreAccessCode,
                            script_object_, access_code, lifetime));

  SetState(kReceivedAccessCode);
}

HostNPScriptObject::HostNPScriptObject(
    NPP plugin,
    NPObject* parent,
    scoped_refptr<AutoThreadTaskRunner> plugin_task_runner)
    : plugin_(plugin),
      parent_(parent),
      plugin_task_runner_(plugin_task_runner),
      am_currently_logging_(false),
      state_(kDisconnected),
      daemon_controller_(DaemonController::Create()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      weak_ptr_(weak_factory_.GetWeakPtr()) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
  ServiceUrls* service_urls = ServiceUrls::GetInstance();
  bool xmpp_server_valid = net::ParseHostAndPort(
      service_urls->xmpp_server_address(),
      &xmpp_server_config_.host, &xmpp_server_config_.port);
  // For the plugin, this is always the default address, which must be valid.
  DCHECK(xmpp_server_valid);
  xmpp_server_config_.use_tls = service_urls->xmpp_server_use_tls();
  directory_bot_jid_ = service_urls->directory_bot_jid();

  // Create worker thread for encryption key generation.
  worker_thread_ = AutoThread::Create("ChromotingWorkerThread",
                                      plugin_task_runner_);
}

HostNPScriptObject::~HostNPScriptObject() {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  HostLogHandler::UnregisterLoggingScriptObject(this);

  // Stop the It2Me host if the caller forgot to.
  if (it2me_impl_.get()) {
    it2me_impl_->Disconnect();
    it2me_impl_ = NULL;
  }
}

bool HostNPScriptObject::HasMethod(const std::string& method_name) {
  VLOG(2) << "HasMethod " << method_name;
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());
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
          property_name == kAttrNameDirectoryBotJid);
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
      if (it2me_impl_) {
        // Ask the It2Me implementation to notify the web-app of the policy.
        it2me_impl_->RequestNatPolicy();
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

  if (it2me_impl_) {
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

  // Create threads for the Chromoting host & desktop environment to use.
  scoped_ptr<ChromotingHostContext> host_context =
    ChromotingHostContext::Create(plugin_task_runner_);
  if (!host_context) {
    SetException("connect: failed to start threads");
    return false;
  }

  // Create the It2Me host implementation and start connecting.
  it2me_impl_ = new It2MeImpl(
      host_context.Pass(), plugin_task_runner_, weak_ptr_,
      xmpp_server_config_, directory_bot_jid_);
  it2me_impl_->Connect(uid, auth_token, auth_service, ui_strings_);

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

  if (it2me_impl_) {
    it2me_impl_->Disconnect();
    it2me_impl_ = NULL;
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
  InvokeAndIgnoreResult(callback_obj.get(), &host_name_val, 1);
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
  InvokeAndIgnoreResult(callback_obj.get(), &pin_hash_val, 1);
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

  ScopedRefNPObject callback_obj(ObjectFromNPVariant(args[0]));
  if (!callback_obj.get()) {
    SetException("generateKeyPair: invalid callback parameter");
    return false;
  }

  // TODO(wez): HostNPScriptObject needn't be touched on worker
  // thread, so make DoGenerateKeyPair static and pass it a callback
  // to run (crbug.com/156257).
  worker_thread_->PostTask(
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

  // TODO(wez): Pass a static method here, that will post the result
  // back to us on the right thread (crbug.com/156257).
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

  // TODO(wez): Pass a static method here, that will post the result
  // back to us on the right thread (crbug.com/156257).
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

  // TODO(wez): Pass a static method here, that will post the result
  // back to us on the right thread (crbug.com/156257).
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

  // TODO(wez): Pass a static method here, that will post the result
  // back to us on the right thread (crbug.com/156257).
  daemon_controller_->GetUsageStatsConsent(
      base::Bind(&HostNPScriptObject::InvokeGetUsageStatsConsentCallback,
                 base::Unretained(this), callback_obj));
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

  ScopedRefNPObject callback_obj(ObjectFromNPVariant(args[2]));
  if (!callback_obj.get()) {
    SetException("startDaemon: invalid callback parameter");
    return false;
  }

  // TODO(wez): Pass a static method here, that will post the result
  // back to us on the right thread (crbug.com/156257).
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
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  if (arg_count != 1) {
    SetException("stopDaemon: bad number of arguments");
    return false;
  }

  ScopedRefNPObject callback_obj(ObjectFromNPVariant(args[0]));
  if (!callback_obj.get()) {
    SetException("stopDaemon: invalid callback parameter");
    return false;
  }

  // TODO(wez): Pass a static method here, that will post the result
  // back to us on the right thread (crbug.com/156257).
  daemon_controller_->Stop(
      base::Bind(&HostNPScriptObject::InvokeAsyncResultCallback,
                 base::Unretained(this), callback_obj));
  return true;
}

void HostNPScriptObject::NotifyStateChanged(State state) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  state_ = state;

  if (on_state_changed_func_.get()) {
    NPVariant state_var;
    INT32_TO_NPVARIANT(state, state_var);
    InvokeAndIgnoreResult(on_state_changed_func_.get(), &state_var, 1);
  }
}

void HostNPScriptObject::NotifyNatPolicyChanged(bool nat_traversal_enabled) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  if (on_nat_traversal_policy_changed_func_.get()) {
    NPVariant policy;
    BOOLEAN_TO_NPVARIANT(nat_traversal_enabled, policy);
    InvokeAndIgnoreResult(on_nat_traversal_policy_changed_func_.get(),
                          &policy, 1);
  }
}

// Stores the Access Code for the web-app to query.
void HostNPScriptObject::StoreAccessCode(const std::string& access_code,
                                         base::TimeDelta access_code_lifetime) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  access_code_ = access_code;
  access_code_lifetime_ = access_code_lifetime;
}

// Stores the client user's name for the web-app to query.
void HostNPScriptObject::StoreClientUsername(
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

  string16 direction;
  LocalizeString(localize_func, "@@bidi_dir", &direction);
  ui_strings_.direction = UTF16ToUTF8(direction) == "rtl" ?
      remoting::UiStrings::RTL : remoting::UiStrings::LTR;
  LocalizeString(localize_func, /*i18n-content*/"PRODUCT_NAME",
                 &ui_strings_.product_name);
  LocalizeString(localize_func, /*i18n-content*/"DISCONNECT_OTHER_BUTTON",
                 &ui_strings_.disconnect_button_text);
  LocalizeString(localize_func, /*i18n-content*/"CONTINUE_PROMPT",
                 &ui_strings_.continue_prompt);
  LocalizeString(localize_func, /*i18n-content*/"CONTINUE_BUTTON",
                 &ui_strings_.continue_button_text);
  LocalizeString(localize_func, /*i18n-content*/"STOP_SHARING_BUTTON",
                 &ui_strings_.stop_sharing_button_text);
  LocalizeStringWithSubstitution(localize_func,
                                 /*i18n-content*/"MESSAGE_SHARED", "$1",
                                 &ui_strings_.disconnect_message);
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
            weak_ptr_, callback, private_key, public_key));
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
            weak_ptr_, callback, result));
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
            weak_ptr_, callback, base::Passed(&config)));
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
            weak_ptr_, callback, version));
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
            weak_ptr_, callback, supported, allowed,
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
