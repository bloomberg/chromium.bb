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
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/policy_constants.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/logging.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/service_urls.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/ftl_signaling_connector.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_secret.h"
#include "remoting/host/host_status_logger.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "remoting/host/it2me/it2me_helpers.h"
#include "remoting/host/it2me_desktop_environment.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/ice_transport.h"
#include "remoting/protocol/it2me_host_authenticator_factory.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/validating_authenticator.h"
#include "remoting/signaling/log_to_server.h"
#include "remoting/signaling/signaling_id_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

using protocol::ErrorCode;

namespace {

// This is used for tagging system event logs.
const char kApplicationName[] = "chromoting";
const int kMaxLoginAttempts = 5;

using protocol::ValidatingAuthenticator;
typedef ValidatingAuthenticator::Result ValidationResult;
typedef ValidatingAuthenticator::ValidationCallback ValidationCallback;
typedef ValidatingAuthenticator::ResultCallback ValidationResultCallback;

// The amount of time to wait before destroying the signal strategy.  This delay
// ensures there is time for the session-terminate message to be sent.
constexpr base::TimeDelta kDestroySignalingDelay = base::Seconds(2);

}  // namespace

It2MeHost::DeferredConnectContext::DeferredConnectContext() = default;

It2MeHost::DeferredConnectContext::~DeferredConnectContext() = default;

It2MeHost::It2MeHost() = default;

It2MeHost::~It2MeHost() {
  // Check that resources that need to be torn down on the UI thread are gone.
  DCHECK(!desktop_environment_factory_.get());
}

void It2MeHost::set_enable_dialogs(bool enable) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || !defined(NDEBUG)
  enable_dialogs_ = enable;
#else
  NOTREACHED() << "It2MeHost::set_enable_dialogs is only supported on ChromeOS";
#endif
}

void It2MeHost::set_enable_notifications(bool enable) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || !defined(NDEBUG)
  enable_notifications_ = enable;
#else
  NOTREACHED() << "It2MeHost::set_enable_notifications is only supported on "
               << "ChromeOS";
#endif
}

void It2MeHost::set_terminate_upon_input(bool terminate_upon_input) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || !defined(NDEBUG)
  terminate_upon_input_ = terminate_upon_input;
#else
  NOTREACHED()
      << "It2MeHost::set_terminate_upon_input is only supported on ChromeOS";
#endif
}

void It2MeHost::set_is_enterprise_session(bool is_enterprise_session) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || !defined(NDEBUG)
  is_enterprise_session_ = is_enterprise_session;
#else
  NOTREACHED()
      << "It2MeHost::set_is_enterprise_session is only supported on ChromeOS";
#endif
}

void It2MeHost::Connect(
    std::unique_ptr<ChromotingHostContext> host_context,
    std::unique_ptr<base::DictionaryValue> policies,
    std::unique_ptr<It2MeConfirmationDialogFactory> dialog_factory,
    base::WeakPtr<It2MeHost::Observer> observer,
    CreateDeferredConnectContext create_context,
    const std::string& username,
    const protocol::IceConfig& ice_config) {
  DCHECK(host_context->ui_task_runner()->BelongsToCurrentThread());

  host_context_ = std::move(host_context);
  observer_ = std::move(observer);
  confirmation_dialog_factory_ = std::move(dialog_factory);

  OnPolicyUpdate(std::move(policies));

  desktop_environment_factory_ =
      std::make_unique<It2MeDesktopEnvironmentFactory>(
          host_context_->network_task_runner(),
          host_context_->video_capture_task_runner(),
          host_context_->input_task_runner(), host_context_->ui_task_runner());

  // Switch to the network thread to start the actual connection.
  host_context_->network_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&It2MeHost::ConnectOnNetworkThread, this, username,
                     ice_config, std::move(create_context)));
}

void It2MeHost::Disconnect() {
  DCHECK(host_context_->ui_task_runner()->BelongsToCurrentThread());
  host_context_->network_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeHost::DisconnectOnNetworkThread, this,
                                protocol::ErrorCode::OK));
}

void It2MeHost::ConnectOnNetworkThread(
    const std::string& username,
    const protocol::IceConfig& ice_config,
    CreateDeferredConnectContext create_context) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(It2MeHostState::kDisconnected, state_);

  if (!remote_support_connections_allowed_) {
    SetState(It2MeHostState::kError, ErrorCode::DISALLOWED_BY_POLICY);
    return;
  }

  SetState(It2MeHostState::kStarting, ErrorCode::OK);

  auto connection_context = std::move(create_context).Run(host_context_.get());
  log_to_server_ = std::move(connection_context->log_to_server);
  signal_strategy_ = std::move(connection_context->signal_strategy);
  oauth_token_getter_ = std::move(connection_context->oauth_token_getter);
  DCHECK(log_to_server_);
  DCHECK(signal_strategy_);

  if (connection_context->use_ftl_signaling) {
    // If the host owns the signaling channel then we want to make sure that it
    // will reconnect that channel if a transient network error occurs.
    // FtlSignalingConnector takes a callback which will indicate whether an
    // auth error has occurred (e.g. token expired). For our purposes, there
    // isn't anything we need to do in this case since a new token will be
    // generated for the next connection.
    ftl_signaling_connector_ = std::make_unique<FtlSignalingConnector>(
        signal_strategy_.get(), base::DoNothing());
    ftl_signaling_connector_->Start();
  }

  // Check the host domain policy.
  if (!required_host_domain_list_.empty()) {
    bool matched = false;
    for (const auto& domain : required_host_domain_list_) {
      if (base::EndsWith(username, std::string("@") + domain,
                         base::CompareCase::INSENSITIVE_ASCII)) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      SetState(It2MeHostState::kInvalidDomainError, ErrorCode::OK);
      return;
    }
  }

  // Generate a key pair for the Host to use.
  // TODO(wez): Move this to the worker thread.
  host_key_pair_ = RsaKeyPair::Generate();

  // Request registration of the host for support.
  register_request_ = std::move(connection_context->register_request);
  register_request_->StartRequest(
      signal_strategy_.get(), host_key_pair_,
      base::BindOnce(&It2MeHost::OnReceivedSupportID, base::Unretained(this)));

  HOST_LOG << "NAT traversal enabled: " << nat_traversal_enabled_;
  HOST_LOG << "Relay connections allowed: " << relay_connections_allowed_;

  uint32_t network_flags = protocol::NetworkSettings::NAT_TRAVERSAL_DISABLED;
  if (nat_traversal_enabled_) {
    network_flags = protocol::NetworkSettings::NAT_TRAVERSAL_STUN |
                    protocol::NetworkSettings::NAT_TRAVERSAL_OUTGOING;
    if (relay_connections_allowed_) {
      network_flags |= protocol::NetworkSettings::NAT_TRAVERSAL_RELAY;
    }
  }

  protocol::NetworkSettings network_settings(network_flags);

  if (!udp_port_range_.is_null()) {
    network_settings.port_range = udp_port_range_;
  } else if (!nat_traversal_enabled_) {
    // For legacy reasons we have to restrict the port range to a set of default
    // values when nat traversal is disabled, even if the port range was not
    // set in policy.
    network_settings.port_range.min_port =
        protocol::NetworkSettings::kDefaultMinPort;
    network_settings.port_range.max_port =
        protocol::NetworkSettings::kDefaultMaxPort;
  }

  scoped_refptr<protocol::TransportContext> transport_context =
      new protocol::TransportContext(
          std::make_unique<protocol::ChromiumPortAllocatorFactory>(),
          host_context_->url_loader_factory(), oauth_token_getter_.get(),
          network_settings, protocol::TransportRole::SERVER);
  if (!ice_config.is_null()) {
    transport_context->set_turn_ice_config(ice_config);
  }

  std::unique_ptr<protocol::SessionManager> session_manager(
      new protocol::JingleSessionManager(signal_strategy_.get()));

  std::unique_ptr<protocol::CandidateSessionConfig> protocol_config =
      protocol::CandidateSessionConfig::CreateDefault();
  // Disable audio by default.
  // TODO(sergeyu): Add UI to enable it.
  protocol_config->DisableAudioChannel();
  protocol_config->set_webrtc_supported(true);
  session_manager->set_protocol_config(std::move(protocol_config));

  // Set up the desktop environment options.
  DesktopEnvironmentOptions options(DesktopEnvironmentOptions::CreateDefault());
  options.set_enable_user_interface(enable_dialogs_);
  options.set_enable_notifications(enable_notifications_);
  options.set_terminate_upon_input(terminate_upon_input_);

  if (max_clipboard_size_.has_value()) {
    options.set_clipboard_size(max_clipboard_size_.value());
  }

  // Create the host.
  host_ = std::make_unique<ChromotingHost>(
      desktop_environment_factory_.get(), std::move(session_manager),
      transport_context, host_context_->audio_task_runner(),
      host_context_->video_encode_task_runner(), options);
  host_->status_monitor()->AddStatusObserver(this);
  host_status_logger_ = std::make_unique<HostStatusLogger>(
      host_->status_monitor(), log_to_server_.get());

  // Create event logger.
  host_event_logger_ =
      HostEventLogger::Create(host_->status_monitor(), kApplicationName);

  // Connect signaling and start the host.
  signal_strategy_->Connect();
  host_->Start(username);

  SetState(It2MeHostState::kRequestedAccessCode, ErrorCode::OK);
  return;
}

void It2MeHost::OnAccessDenied(const std::string& jid) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  ++failed_login_attempts_;
  if (failed_login_attempts_ == kMaxLoginAttempts) {
    DisconnectOnNetworkThread();
  } else if (connecting_jid_ == NormalizeSignalingId(jid)) {
    DCHECK_EQ(state_, It2MeHostState::kConnecting);
    connecting_jid_.clear();
    confirmation_dialog_proxy_.reset();
    SetState(It2MeHostState::kReceivedAccessCode, ErrorCode::OK);
  }
}

void It2MeHost::OnClientConnected(const std::string& jid) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  // ChromotingHost doesn't allow concurrent connections and the host is
  // destroyed in OnClientDisconnected() after the first connection.
  CHECK_NE(state_, It2MeHostState::kConnected);

  std::string client_username;
  if (!SplitSignalingIdResource(jid, &client_username, /*resource=*/nullptr)) {
    LOG(WARNING) << "Incorrectly formatted JID received: " << jid;
    client_username = jid;
  }

  HOST_LOG << "Client " << client_username << " connected.";

  // Pass the client user name to the script object before changing state.
  host_context_->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeHost::Observer::OnClientAuthenticated,
                                observer_, client_username));

  SetState(It2MeHostState::kConnected, ErrorCode::OK);
}

void It2MeHost::OnClientDisconnected(const std::string& jid) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  DisconnectOnNetworkThread();
}

ValidationCallback It2MeHost::GetValidationCallbackForTesting() {
  return base::BindRepeating(&It2MeHost::ValidateConnectionDetails,
                             base::Unretained(this));
}

void It2MeHost::OnPolicyUpdate(
    std::unique_ptr<base::DictionaryValue> policies) {
  // The policy watcher runs on the |ui_task_runner|.
  if (!host_context_->network_task_runner()->BelongsToCurrentThread()) {
    host_context_->network_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&It2MeHost::OnPolicyUpdate, this, std::move(policies)));
    return;
  }

  // The policy to disallow remote support connections should not apply to
  // support sessions initiated by the enterprise admin via a RemoteCommand.
  if (!is_enterprise_session_) {
    // Retrieve the policy value on whether to allow connections but don't apply
    // it until after we've finished reading the rest of the policies and
    // started the connection process.
    absl::optional<bool> remote_support_connections_allowed =
        policies->FindBoolKey(
            policy::key::kRemoteAccessHostAllowRemoteSupportConnections);
    remote_support_connections_allowed_ =
        remote_support_connections_allowed.value_or(true);
  }

  absl::optional<bool> nat_policy_value =
      policies->FindBoolKey(policy::key::kRemoteAccessHostFirewallTraversal);
  if (!nat_policy_value.has_value()) {
    HOST_LOG << "Failed to read kRemoteAccessHostFirewallTraversal policy";
    nat_policy_value = nat_traversal_enabled_;
  }
  absl::optional<bool> relay_policy_value = policies->FindBoolKey(
      policy::key::kRemoteAccessHostAllowRelayedConnection);
  if (!relay_policy_value.has_value()) {
    HOST_LOG << "Failed to read kRemoteAccessHostAllowRelayedConnection policy";
    relay_policy_value = relay_connections_allowed_;
  }
  UpdateNatPolicies(nat_policy_value.value(), relay_policy_value.value());

  const base::ListValue* host_domain_list;
  if (policies->GetList(policy::key::kRemoteAccessHostDomainList,
                        &host_domain_list)) {
    std::vector<std::string> host_domain_list_vector;
    for (const auto& value : host_domain_list->GetList()) {
      host_domain_list_vector.push_back(value.GetString());
    }
    UpdateHostDomainListPolicy(std::move(host_domain_list_vector));
  }

  const base::ListValue* client_domain_list;
  if (policies->GetList(policy::key::kRemoteAccessHostClientDomainList,
                        &client_domain_list)) {
    std::vector<std::string> client_domain_list_vector;
    for (const auto& value : client_domain_list->GetList()) {
      client_domain_list_vector.push_back(value.GetString());
    }
    UpdateClientDomainListPolicy(std::move(client_domain_list_vector));
  }

  std::string port_range_string;
  if (policies->GetString(policy::key::kRemoteAccessHostUdpPortRange,
                          &port_range_string)) {
    UpdateHostUdpPortRangePolicy(port_range_string);
  }

  int max_clipboard_size;
  if (policies->GetInteger(policy::key::kRemoteAccessHostClipboardSizeBytes,
                           &max_clipboard_size)) {
    if (max_clipboard_size >= 0) {
      max_clipboard_size_ = max_clipboard_size;
    }
  }
}

void It2MeHost::UpdateNatPolicies(bool nat_policy_value,
                                  bool relay_policy_value) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateNatPolicies: nat_policy_value: " << nat_policy_value;
  bool nat_traversal_value_changed = nat_traversal_enabled_ != nat_policy_value;
  nat_traversal_enabled_ = nat_policy_value;

  VLOG(2) << "UpdateNatPolicies: relay_policy_value: " << relay_policy_value;
  bool relay_value_changed = relay_connections_allowed_ != relay_policy_value;
  relay_connections_allowed_ = relay_policy_value;

  // Force disconnect when transitioning either policy setting to disabled.
  if (((nat_traversal_value_changed && !nat_traversal_enabled_) ||
       (relay_value_changed && !relay_connections_allowed_)) &&
      IsRunning()) {
    DisconnectOnNetworkThread();
  }

  // Notify listeners of the policy setting change.
  host_context_->ui_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&It2MeHost::Observer::OnNatPoliciesChanged, observer_,
                     nat_traversal_enabled_, relay_connections_allowed_));
}

void It2MeHost::UpdateHostDomainListPolicy(
    std::vector<std::string> host_domain_list) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateHostDomainListPolicy: "
          << base::JoinString(host_domain_list, ", ");

  // When setting a host domain policy, force disconnect any existing session.
  if (!host_domain_list.empty() && IsRunning()) {
    DisconnectOnNetworkThread();
  }

  required_host_domain_list_ = std::move(host_domain_list);
}

void It2MeHost::UpdateClientDomainListPolicy(
    std::vector<std::string> client_domain_list) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateClientDomainPolicy: "
          << base::JoinString(client_domain_list, ", ");

  // When setting a client domain policy, disconnect any existing session.
  if (!client_domain_list.empty() && IsRunning()) {
    DisconnectOnNetworkThread();
  }

  required_client_domain_list_ = std::move(client_domain_list);
}

void It2MeHost::UpdateHostUdpPortRangePolicy(
    const std::string& port_range_string) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateHostUdpPortRangePolicy: " << port_range_string;

  if (IsRunning()) {
    DisconnectOnNetworkThread();
  }

  if (!PortRange::Parse(port_range_string, &udp_port_range_)) {
    // PolicyWatcher verifies that the value is formatted correctly.
    LOG(FATAL) << "Invalid port range: " << port_range_string;
  }
}

void It2MeHost::SetState(It2MeHostState state, ErrorCode error_code) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  switch (state_) {
    case It2MeHostState::kDisconnected:
      DCHECK(state == It2MeHostState::kStarting ||
             state == It2MeHostState::kError)
          << It2MeHostStateToString(state);
      break;
    case It2MeHostState::kStarting:
      DCHECK(state == It2MeHostState::kRequestedAccessCode ||
             state == It2MeHostState::kDisconnected ||
             state == It2MeHostState::kError ||
             state == It2MeHostState::kInvalidDomainError)
          << It2MeHostStateToString(state);
      break;
    case It2MeHostState::kRequestedAccessCode:
      DCHECK(state == It2MeHostState::kReceivedAccessCode ||
             state == It2MeHostState::kDisconnected ||
             state == It2MeHostState::kError)
          << It2MeHostStateToString(state);
      break;
    case It2MeHostState::kReceivedAccessCode:
      DCHECK(state == It2MeHostState::kConnecting ||
             state == It2MeHostState::kDisconnected ||
             state == It2MeHostState::kError)
          << It2MeHostStateToString(state);
      break;
    case It2MeHostState::kConnecting:
      DCHECK(state == It2MeHostState::kConnected ||
             state == It2MeHostState::kDisconnected ||
             state == It2MeHostState::kError)
          << It2MeHostStateToString(state);
      break;
    case It2MeHostState::kConnected:
      DCHECK(state == It2MeHostState::kDisconnected ||
             state == It2MeHostState::kError)
          << It2MeHostStateToString(state);
      break;
    case It2MeHostState::kError:
      DCHECK(state == It2MeHostState::kDisconnected)
          << It2MeHostStateToString(state);
      break;
    case It2MeHostState::kInvalidDomainError:
      DCHECK(state == It2MeHostState::kDisconnected)
          << It2MeHostStateToString(state);
      break;
  }

  state_ = state;

  // Post a state-change notification to the web-app.
  host_context_->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeHost::Observer::OnStateChanged, observer_,
                                state, error_code));
}

bool It2MeHost::IsRunning() const {
  return state_ == It2MeHostState::kRequestedAccessCode ||
         state_ == It2MeHostState::kReceivedAccessCode ||
         state_ == It2MeHostState::kConnected ||
         state_ == It2MeHostState::kConnecting;
}

void It2MeHost::OnReceivedSupportID(const std::string& support_id,
                                    const base::TimeDelta& lifetime,
                                    const ErrorCode error_code) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  if (error_code != ErrorCode::OK) {
    SetState(It2MeHostState::kError, error_code);
    DisconnectOnNetworkThread();
    return;
  }

  std::string host_secret = GenerateSupportHostSecret();
  std::string access_code = support_id + host_secret;
  std::string access_code_hash =
      protocol::GetSharedSecretHash(support_id, access_code);

  std::string local_certificate = host_key_pair_->GenerateCertificate();
  if (local_certificate.empty()) {
    LOG(ERROR) << "Failed to generate host certificate.";
    SetState(It2MeHostState::kError, ErrorCode::HOST_CERTIFICATE_ERROR);
    DisconnectOnNetworkThread();
    return;
  }

  std::unique_ptr<protocol::AuthenticatorFactory> factory(
      new protocol::It2MeHostAuthenticatorFactory(
          local_certificate, host_key_pair_, access_code_hash,
          base::BindRepeating(&It2MeHost::ValidateConnectionDetails,
                              base::Unretained(this))));
  host_->SetAuthenticatorFactory(std::move(factory));

  // Pass the Access Code to the script object before changing state.
  host_context_->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeHost::Observer::OnStoreAccessCode,
                                observer_, access_code, lifetime));

  SetState(It2MeHostState::kReceivedAccessCode, ErrorCode::OK);
}

void It2MeHost::DisconnectOnNetworkThread(protocol::ErrorCode error_code) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  // Disconnect() may be called even after the host has already been stopped.
  // Ignore repeated calls.
  if (state_ == It2MeHostState::kDisconnected) {
    return;
  }

  confirmation_dialog_proxy_.reset();

  if (host_) {
    host_->status_monitor()->RemoveStatusObserver(this);
    host_ = nullptr;
  }

  register_request_ = nullptr;
  host_status_logger_ = nullptr;
  log_to_server_ = nullptr;
  ftl_signaling_connector_ = nullptr;

  if (signal_strategy_) {
    // Delay destruction of the signaling strategy by a few seconds to give it
    // a chance to send any outgoing messages (e.g. session-terminate) so the
    // other end of the connection can display and log an accurate disconnect
    // reason.
    host_context_->network_task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce([](std::unique_ptr<SignalStrategy> signaling) {},
                       std::move(signal_strategy_)),
        kDestroySignalingDelay);
  }

  host_event_logger_ = nullptr;

  // Post tasks to delete UI objects on the UI thread.
  host_context_->ui_task_runner()->DeleteSoon(
      FROM_HERE, desktop_environment_factory_.release());

  SetState(It2MeHostState::kDisconnected, error_code);
}

void It2MeHost::ValidateConnectionDetails(
    const std::string& original_remote_jid,
    ValidationResultCallback result_callback) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  // First ensure the JID we received is valid.
  std::string client_username;
  if (!SplitSignalingIdResource(original_remote_jid, &client_username,
                                /*resource=*/nullptr)) {
    LOG(ERROR) << "Rejecting incoming connection from " << original_remote_jid
               << ": Invalid JID.";
    std::move(result_callback)
        .Run(protocol::ValidatingAuthenticator::Result::ERROR_INVALID_ACCOUNT);
    DisconnectOnNetworkThread();
    return;
  }
  std::string remote_jid = NormalizeSignalingId(original_remote_jid);

  if (client_username.empty()) {
    LOG(ERROR) << "Invalid user name passed in: " << remote_jid;
    std::move(result_callback)
        .Run(protocol::ValidatingAuthenticator::Result::ERROR_INVALID_ACCOUNT);
    DisconnectOnNetworkThread();
    return;
  }

  // Check the client domain policy.
  if (!required_client_domain_list_.empty()) {
    bool matched = false;
    for (const auto& domain : required_client_domain_list_) {
      if (base::EndsWith(client_username, std::string("@") + domain,
                         base::CompareCase::INSENSITIVE_ASCII)) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      LOG(ERROR) << "Rejecting incoming connection from " << remote_jid
                 << ": Domain not allowed.";
      std::move(result_callback).Run(ValidationResult::ERROR_INVALID_ACCOUNT);
      DisconnectOnNetworkThread();
      return;
    }
  }

  // If we receive valid connection details multiple times, then we don't know
  // which remote user (if either) is valid so disconnect everyone.
  if (state_ != It2MeHostState::kReceivedAccessCode) {
    DCHECK_EQ(It2MeHostState::kConnecting, state_);
    LOG(ERROR) << "Received too many connection requests.";
    std::move(result_callback)
        .Run(ValidationResult::ERROR_TOO_MANY_CONNECTIONS);
    DisconnectOnNetworkThread();
    return;
  }

  HOST_LOG << "Client " << client_username << " connecting.";
  connecting_jid_ = remote_jid;
  SetState(It2MeHostState::kConnecting, ErrorCode::OK);

  // Show a confirmation dialog to the user to allow them to confirm/reject it.
  // If dialogs are suppressed, just call the callback directly.
  if (enable_dialogs_) {
    confirmation_dialog_proxy_ = std::make_unique<It2MeConfirmationDialogProxy>(
        host_context_->ui_task_runner(),
        confirmation_dialog_factory_->Create());
    confirmation_dialog_proxy_->Show(
        client_username,
        base::BindOnce(&It2MeHost::OnConfirmationResult, base::Unretained(this),
                       std::move(result_callback)));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(result_callback), ValidationResult::SUCCESS));
  }
}

void It2MeHost::OnConfirmationResult(ValidationResultCallback result_callback,
                                     It2MeConfirmationDialog::Result result) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  connecting_jid_.clear();
  switch (result) {
    case It2MeConfirmationDialog::Result::OK:
      std::move(result_callback).Run(ValidationResult::SUCCESS);
      break;

    case It2MeConfirmationDialog::Result::CANCEL:
      std::move(result_callback).Run(ValidationResult::ERROR_REJECTED_BY_USER);
      DisconnectOnNetworkThread(ErrorCode::SESSION_REJECTED);
      break;
  }
}

It2MeHostFactory::It2MeHostFactory() = default;
It2MeHostFactory::~It2MeHostFactory() = default;

scoped_refptr<It2MeHost> It2MeHostFactory::CreateIt2MeHost() {
  return new It2MeHost();
}

}  // namespace remoting
