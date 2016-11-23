// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_host.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "components/policy/policy_constants.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/chromium_url_request.h"
#include "remoting/base/logging.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_secret.h"
#include "remoting/host/host_status_logger.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "remoting/host/it2me_desktop_environment.h"
#include "remoting/host/policy_watcher.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/host/service_urls.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/ice_transport.h"
#include "remoting/protocol/it2me_host_authenticator_factory.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/validating_authenticator.h"
#include "remoting/signaling/jid_util.h"
#include "remoting/signaling/server_log_entry.h"

namespace remoting {

namespace {

// This is used for tagging system event logs.
const char kApplicationName[] = "chromoting";
const int kMaxLoginAttempts = 5;

using protocol::ValidatingAuthenticator;
typedef ValidatingAuthenticator::Result ValidationResult;
typedef ValidatingAuthenticator::ValidationCallback ValidationCallback;

}  // namespace

It2MeHost::It2MeHost(
    std::unique_ptr<ChromotingHostContext> host_context,
    std::unique_ptr<PolicyWatcher> policy_watcher,
    std::unique_ptr<It2MeConfirmationDialog> confirmation_dialog,
    base::WeakPtr<It2MeHost::Observer> observer,
    std::unique_ptr<SignalStrategy> signal_strategy,
    const std::string& username,
    const std::string& directory_bot_jid)
    : host_context_(std::move(host_context)),
      observer_(observer),
      signal_strategy_(std::move(signal_strategy)),
      username_(username),
      directory_bot_jid_(directory_bot_jid),
      policy_watcher_(std::move(policy_watcher)),
      confirmation_dialog_(std::move(confirmation_dialog)) {
  DCHECK(host_context_->ui_task_runner()->BelongsToCurrentThread());
}

It2MeHost::~It2MeHost() {
  // Check that resources that need to be torn down on the UI thread are gone.
  DCHECK(!desktop_environment_factory_.get());
  DCHECK(!policy_watcher_.get());
}

void It2MeHost::Connect() {
  if (!host_context_->ui_task_runner()->BelongsToCurrentThread()) {
    host_context_->ui_task_runner()->PostTask(
        FROM_HERE, base::Bind(&It2MeHost::Connect, this));
    return;
  }

  desktop_environment_factory_.reset(new It2MeDesktopEnvironmentFactory(
      host_context_->network_task_runner(),
      host_context_->video_capture_task_runner(),
      host_context_->input_task_runner(), host_context_->ui_task_runner()));

  // Start monitoring configured policies.
  policy_watcher_->StartWatching(
      base::Bind(&It2MeHost::OnPolicyUpdate, this),
      base::Bind(&It2MeHost::OnPolicyError, this));

  // Switch to the network thread to start the actual connection.
  host_context_->network_task_runner()->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::ReadPolicyAndConnect, this));
}

void It2MeHost::Disconnect() {
  DCHECK(host_context_->ui_task_runner()->BelongsToCurrentThread());
  host_context_->network_task_runner()->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::DisconnectOnNetworkThread, this));
}

void It2MeHost::DisconnectOnNetworkThread() {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  // Disconnect() may be called even when after the host been already stopped.
  // Ignore repeated calls.
  if (state_ == kDisconnected) {
    return;
  }

  confirmation_dialog_proxy_.reset();

  host_event_logger_.reset();
  if (host_) {
    host_->RemoveStatusObserver(this);
    host_.reset();
  }

  register_request_.reset();
  host_status_logger_.reset();
  signal_strategy_.reset();

  // Post tasks to delete UI objects on the UI thread.
  host_context_->ui_task_runner()->DeleteSoon(
      FROM_HERE, desktop_environment_factory_.release());
  host_context_->ui_task_runner()->DeleteSoon(FROM_HERE,
                                              policy_watcher_.release());

  SetState(kDisconnected, "");
}

void It2MeHost::RequestNatPolicy() {
  if (!host_context_->network_task_runner()->BelongsToCurrentThread()) {
    DCHECK(host_context_->ui_task_runner()->BelongsToCurrentThread());
    host_context_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&It2MeHost::RequestNatPolicy, this));
    return;
  }

  if (policy_received_)
    UpdateNatPolicy(nat_traversal_enabled_);
}

void It2MeHost::ReadPolicyAndConnect() {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(kDisconnected, state_);

  SetState(kStarting, "");

  // Only proceed to FinishConnect() if at least one policy update has been
  // received.  Otherwise, create the policy watcher and thunk the connect.
  if (policy_received_) {
    FinishConnect();
  } else {
    pending_connect_ = base::Bind(&It2MeHost::FinishConnect, this);
  }
}

void It2MeHost::FinishConnect() {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  if (state_ != kStarting) {
    // Host has been stopped while we were fetching policy.
    return;
  }

  // Check the host domain policy.
  if (!required_host_domain_.empty() &&
      !base::EndsWith(username_, std::string("@") + required_host_domain_,
                      base::CompareCase::INSENSITIVE_ASCII)) {
    SetState(kInvalidDomainError, "");
    return;
  }

  // Generate a key pair for the Host to use.
  // TODO(wez): Move this to the worker thread.
  host_key_pair_ = RsaKeyPair::Generate();

  // Request registration of the host for support.
  std::unique_ptr<RegisterSupportHostRequest> register_request(
      new RegisterSupportHostRequest(
          signal_strategy_.get(), host_key_pair_, directory_bot_jid_,
          base::Bind(&It2MeHost::OnReceivedSupportID, base::Unretained(this))));

  // Beyond this point nothing can fail, so save the config and request.
  register_request_ = std::move(register_request);

  // If NAT traversal is off then limit port range to allow firewall pin-holing.
  HOST_LOG << "NAT state: " << nat_traversal_enabled_;
  protocol::NetworkSettings network_settings(
     nat_traversal_enabled_ ?
     protocol::NetworkSettings::NAT_TRAVERSAL_FULL :
     protocol::NetworkSettings::NAT_TRAVERSAL_DISABLED);
  if (!nat_traversal_enabled_) {
    network_settings.port_range.min_port =
        protocol::NetworkSettings::kDefaultMinPort;
    network_settings.port_range.max_port =
        protocol::NetworkSettings::kDefaultMaxPort;
  }

  scoped_refptr<protocol::TransportContext> transport_context =
      new protocol::TransportContext(
          signal_strategy_.get(),
          base::WrapUnique(new protocol::ChromiumPortAllocatorFactory()),
          base::WrapUnique(new ChromiumUrlRequestFactory(
              host_context_->url_request_context_getter())),
          network_settings, protocol::TransportRole::SERVER);
  transport_context->set_ice_config_url(
      ServiceUrls::GetInstance()->ice_config_url());

  std::unique_ptr<protocol::SessionManager> session_manager(
      new protocol::JingleSessionManager(signal_strategy_.get()));

  std::unique_ptr<protocol::CandidateSessionConfig> protocol_config =
      protocol::CandidateSessionConfig::CreateDefault();
  // Disable audio by default.
  // TODO(sergeyu): Add UI to enable it.
  protocol_config->DisableAudioChannel();
  protocol_config->set_webrtc_supported(true);
  session_manager->set_protocol_config(std::move(protocol_config));

  // Create the host.
  host_.reset(new ChromotingHost(desktop_environment_factory_.get(),
                                 std::move(session_manager), transport_context,
                                 host_context_->audio_task_runner(),
                                 host_context_->video_encode_task_runner(),
                                 DesktopEnvironmentOptions::CreateDefault()));
  host_->AddStatusObserver(this);
  host_status_logger_.reset(
      new HostStatusLogger(host_->AsWeakPtr(), ServerLogEntry::IT2ME,
                           signal_strategy_.get(), directory_bot_jid_));

  // Create event logger.
  host_event_logger_ =
      HostEventLogger::Create(host_->AsWeakPtr(), kApplicationName);

  // Connect signaling and start the host.
  signal_strategy_->Connect();
  host_->Start(username_);

  SetState(kRequestedAccessCode, "");
  return;
}

void It2MeHost::OnAccessDenied(const std::string& jid) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  ++failed_login_attempts_;
  if (failed_login_attempts_ == kMaxLoginAttempts) {
    DisconnectOnNetworkThread();
  }
}

void It2MeHost::OnClientConnected(const std::string& jid) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  // ChromotingHost doesn't allow multiple concurrent connection and the
  // host is destroyed in OnClientDisconnected() after the first connection.
  CHECK_NE(state_, kConnected);

  std::string client_username;
  if (!SplitJidResource(jid, &client_username, /*resource=*/nullptr)) {
    LOG(WARNING) << "Incorrectly formatted JID received: " << jid;
    client_username = jid;
  }

  HOST_LOG << "Client " << client_username << " connected.";

  // Pass the client user name to the script object before changing state.
  host_context_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::Observer::OnClientAuthenticated,
                            observer_, client_username));

  SetState(kConnected, "");
}

void It2MeHost::OnClientDisconnected(const std::string& jid) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  DisconnectOnNetworkThread();
}

void It2MeHost::SetPolicyForTesting(
    std::unique_ptr<base::DictionaryValue> policies,
    const base::Closure& done_callback) {
  host_context_->network_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&It2MeHost::OnPolicyUpdate, this, base::Passed(&policies)),
      done_callback);
}

ValidationCallback It2MeHost::GetValidationCallbackForTesting() {
  return base::Bind(&It2MeHost::ValidateConnectionDetails,
                    base::Unretained(this));
}

void It2MeHost::OnPolicyUpdate(
    std::unique_ptr<base::DictionaryValue> policies) {
  // The policy watcher runs on the |ui_task_runner|.
  if (!host_context_->network_task_runner()->BelongsToCurrentThread()) {
    host_context_->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&It2MeHost::OnPolicyUpdate, this, base::Passed(&policies)));
    return;
  }

  bool nat_policy;
  if (policies->GetBoolean(policy::key::kRemoteAccessHostFirewallTraversal,
                           &nat_policy)) {
    UpdateNatPolicy(nat_policy);
  }
  std::string host_domain;
  if (policies->GetString(policy::key::kRemoteAccessHostDomain, &host_domain)) {
    UpdateHostDomainPolicy(host_domain);
  }
  std::string client_domain;
  if (policies->GetString(policy::key::kRemoteAccessHostClientDomain,
                          &client_domain)) {
    UpdateClientDomainPolicy(client_domain);
  }

  policy_received_ = true;

  if (!pending_connect_.is_null()) {
    base::ResetAndReturn(&pending_connect_).Run();
  }
}

void It2MeHost::OnPolicyError() {
  // TODO(lukasza): Report the policy error to the user.  crbug.com/433009
  NOTIMPLEMENTED();
}

void It2MeHost::UpdateNatPolicy(bool nat_traversal_enabled) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateNatPolicy: " << nat_traversal_enabled;

  // When transitioning from enabled to disabled, force disconnect any
  // existing session.
  if (nat_traversal_enabled_ && !nat_traversal_enabled && IsRunning()) {
    DisconnectOnNetworkThread();
  }

  nat_traversal_enabled_ = nat_traversal_enabled;

  // Notify the web-app of the policy setting.
  host_context_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::Observer::OnNatPolicyChanged, observer_,
                            nat_traversal_enabled_));
}

void It2MeHost::UpdateHostDomainPolicy(const std::string& host_domain) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateHostDomainPolicy: " << host_domain;

  // When setting a host domain policy, force disconnect any existing session.
  if (!host_domain.empty() && IsRunning()) {
    DisconnectOnNetworkThread();
  }

  required_host_domain_ = host_domain;
}

void It2MeHost::UpdateClientDomainPolicy(const std::string& client_domain) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateClientDomainPolicy: " << client_domain;

  // When setting a client  domain policy, disconnect any existing session.
  if (!client_domain.empty() && IsRunning()) {
    DisconnectOnNetworkThread();
  }

  required_client_domain_ = client_domain;
}

void It2MeHost::SetState(It2MeHostState state,
                         const std::string& error_message) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  switch (state_) {
    case kDisconnected:
      DCHECK(state == kStarting ||
             state == kError) << state;
      break;
    case kStarting:
      DCHECK(state == kRequestedAccessCode ||
             state == kDisconnected ||
             state == kError ||
             state == kInvalidDomainError) << state;
      break;
    case kRequestedAccessCode:
      DCHECK(state == kReceivedAccessCode ||
             state == kDisconnected ||
             state == kError) << state;
      break;
    case kReceivedAccessCode:
      DCHECK(state == kConnecting ||
             state == kDisconnected ||
             state == kError) << state;
      break;
    case kConnecting:
      DCHECK(state == kConnected ||
             state == kDisconnected ||
             state == kError) << state;
      break;
    case kConnected:
      DCHECK(state == kDisconnected ||
             state == kError) << state;
      break;
    case kError:
      DCHECK(state == kDisconnected) << state;
      break;
    case kInvalidDomainError:
      DCHECK(state == kDisconnected) << state;
      break;
  };

  state_ = state;

  // Post a state-change notification to the web-app.
  host_context_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::Observer::OnStateChanged, observer_,
                            state, error_message));
}

bool It2MeHost::IsRunning() const {
  return state_ == kRequestedAccessCode || state_ == kReceivedAccessCode ||
         state_ == kConnected || state_ == kConnecting;
}

void It2MeHost::OnReceivedSupportID(
    const std::string& support_id,
    const base::TimeDelta& lifetime,
    const std::string& error_message) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  if (!error_message.empty()) {
    SetState(kError, error_message);
    DisconnectOnNetworkThread();
    return;
  }

  std::string host_secret = GenerateSupportHostSecret();
  std::string access_code = support_id + host_secret;
  std::string access_code_hash =
      protocol::GetSharedSecretHash(support_id, access_code);

  std::string local_certificate = host_key_pair_->GenerateCertificate();
  if (local_certificate.empty()) {
    std::string error_message = "Failed to generate host certificate.";
    LOG(ERROR) << error_message;
    SetState(kError, error_message);
    DisconnectOnNetworkThread();
    return;
  }

  std::unique_ptr<protocol::AuthenticatorFactory> factory(
      new protocol::It2MeHostAuthenticatorFactory(
          local_certificate, host_key_pair_, access_code_hash,
          base::Bind(&It2MeHost::ValidateConnectionDetails,
                     base::Unretained(this))));
  host_->SetAuthenticatorFactory(std::move(factory));

  // Pass the Access Code to the script object before changing state.
  host_context_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::Observer::OnStoreAccessCode, observer_,
                            access_code, lifetime));

  SetState(kReceivedAccessCode, "");
}

void It2MeHost::ValidateConnectionDetails(
    const std::string& remote_jid,
    const protocol::ValidatingAuthenticator::ResultCallback& result_callback) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  // First ensure the JID we received is valid.
  std::string client_username;
  if (!SplitJidResource(remote_jid, &client_username, /*resource=*/nullptr)) {
    LOG(ERROR) << "Rejecting incoming connection from " << remote_jid
               << ": Invalid JID.";
    result_callback.Run(
        protocol::ValidatingAuthenticator::Result::ERROR_INVALID_ACCOUNT);
    DisconnectOnNetworkThread();
    return;
  }

  if (client_username.empty()) {
    LOG(ERROR) << "Invalid user name passed in: " << remote_jid;
    result_callback.Run(
        protocol::ValidatingAuthenticator::Result::ERROR_INVALID_ACCOUNT);
    DisconnectOnNetworkThread();
    return;
  }

  // Check the client domain policy.
  if (!required_client_domain_.empty()) {
    if (!base::EndsWith(client_username,
                        std::string("@") + required_client_domain_,
                        base::CompareCase::INSENSITIVE_ASCII)) {
      LOG(ERROR) << "Rejecting incoming connection from " << remote_jid
                 << ": Domain mismatch.";
      result_callback.Run(ValidationResult::ERROR_INVALID_ACCOUNT);
      DisconnectOnNetworkThread();
      return;
    }
  }

  HOST_LOG << "Client " << client_username << " connecting.";
  SetState(kConnecting, std::string());

  // Show a confirmation dialog to the user to allow them to confirm/reject it.
  confirmation_dialog_proxy_.reset(new It2MeConfirmationDialogProxy(
      host_context_->ui_task_runner(), std::move(confirmation_dialog_)));

  confirmation_dialog_proxy_->Show(
      client_username, base::Bind(&It2MeHost::OnConfirmationResult,
                                  base::Unretained(this), result_callback));
}

void It2MeHost::OnConfirmationResult(
    const protocol::ValidatingAuthenticator::ResultCallback& result_callback,
    It2MeConfirmationDialog::Result result) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  switch (result) {
    case It2MeConfirmationDialog::Result::OK:
      result_callback.Run(ValidationResult::SUCCESS);
      break;

    case It2MeConfirmationDialog::Result::CANCEL:
      result_callback.Run(ValidationResult::ERROR_REJECTED_BY_USER);
      DisconnectOnNetworkThread();
      break;
  }
}

It2MeHostFactory::It2MeHostFactory() {}

It2MeHostFactory::~It2MeHostFactory() {}

scoped_refptr<It2MeHost> It2MeHostFactory::CreateIt2MeHost(
    std::unique_ptr<ChromotingHostContext> context,
    policy::PolicyService* policy_service,
    base::WeakPtr<It2MeHost::Observer> observer,
    std::unique_ptr<SignalStrategy> signal_strategy,
    const std::string& username,
    const std::string& directory_bot_jid) {
  DCHECK(context->ui_task_runner()->BelongsToCurrentThread());

  std::unique_ptr<PolicyWatcher> policy_watcher =
      PolicyWatcher::Create(policy_service, context->file_task_runner());
  return new It2MeHost(std::move(context), std::move(policy_watcher),
                       It2MeConfirmationDialog::Create(), observer,
                       std::move(signal_strategy), username, directory_bot_jid);
}

}  // namespace remoting
