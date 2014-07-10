// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_native_messaging_host.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "net/base/net_util.h"
#include "net/url_request/url_fetcher.h"
#include "remoting/base/auth_token_util.h"
#include "remoting/base/service_urls.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/protocol/name_value_map.h"

namespace remoting {

namespace {

const remoting::protocol::NameMapElement<It2MeHostState> kIt2MeHostStates[] = {
    {kDisconnected, "DISCONNECTED"},
    {kStarting, "STARTING"},
    {kRequestedAccessCode, "REQUESTED_ACCESS_CODE"},
    {kReceivedAccessCode, "RECEIVED_ACCESS_CODE"},
    {kConnected, "CONNECTED"},
    {kDisconnecting, "DISCONNECTING"},
    {kError, "ERROR"},
    {kInvalidDomainError, "INVALID_DOMAIN_ERROR"}, };

}  // namespace

It2MeNativeMessagingHost::It2MeNativeMessagingHost(
    scoped_refptr<AutoThreadTaskRunner> task_runner,
    scoped_ptr<NativeMessagingChannel> channel,
    scoped_ptr<It2MeHostFactory> factory)
    : channel_(channel.Pass()),
      factory_(factory.Pass()),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();

  // Initialize the host context to manage the threads for the it2me host.
  // The native messaging host, rather than the It2MeHost object, owns and
  // maintains the lifetime of the host context.

  host_context_.reset(ChromotingHostContext::Create(task_runner).release());

  const ServiceUrls* service_urls = ServiceUrls::GetInstance();
  const bool xmpp_server_valid =
      net::ParseHostAndPort(service_urls->xmpp_server_address(),
                            &xmpp_server_config_.host,
                            &xmpp_server_config_.port);
  DCHECK(xmpp_server_valid);

  xmpp_server_config_.use_tls = service_urls->xmpp_server_use_tls();
  directory_bot_jid_ = service_urls->directory_bot_jid();
}

It2MeNativeMessagingHost::~It2MeNativeMessagingHost() {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (it2me_host_.get()) {
    it2me_host_->Disconnect();
    it2me_host_ = NULL;
  }
}

void It2MeNativeMessagingHost::Start(const base::Closure& quit_closure) const {
  DCHECK(task_runner()->BelongsToCurrentThread());

  channel_->Start(
      base::Bind(&It2MeNativeMessagingHost::ProcessMessage, weak_ptr_),
      quit_closure);
}

void It2MeNativeMessagingHost::ProcessMessage(
    scoped_ptr<base::DictionaryValue> message) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue());

  // If the client supplies an ID, it will expect it in the response. This
  // might be a string or a number, so cope with both.
  const base::Value* id;
  if (message->Get("id", &id))
    response->Set("id", id->DeepCopy());

  std::string type;
  if (!message->GetString("type", &type)) {
    SendErrorAndExit(response.Pass(), "'type' not found in request.");
    return;
  }

  response->SetString("type", type + "Response");

  if (type == "hello") {
    ProcessHello(*message, response.Pass());
  } else if (type == "connect") {
    ProcessConnect(*message, response.Pass());
  } else if (type == "disconnect") {
    ProcessDisconnect(*message, response.Pass());
  } else {
    SendErrorAndExit(response.Pass(), "Unsupported request type: " + type);
  }
}

void It2MeNativeMessagingHost::ProcessHello(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) const {
  DCHECK(task_runner()->BelongsToCurrentThread());

  response->SetString("version", STRINGIZE(VERSION));

  // This list will be populated when new features are added.
  scoped_ptr<base::ListValue> supported_features_list(new base::ListValue());
  response->Set("supportedFeatures", supported_features_list.release());

  channel_->SendMessage(response.Pass());
}

void It2MeNativeMessagingHost::ProcessConnect(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (it2me_host_.get()) {
    SendErrorAndExit(response.Pass(),
                     "Connect can be called only when disconnected.");
    return;
  }

  XmppSignalStrategy::XmppServerConfig xmpp_config = xmpp_server_config_;

  if (!message.GetString("userName", &xmpp_config.username)) {
    SendErrorAndExit(response.Pass(), "'userName' not found in request.");
    return;
  }

  std::string auth_service_with_token;
  if (!message.GetString("authServiceWithToken", &auth_service_with_token)) {
    SendErrorAndExit(response.Pass(),
                     "'authServiceWithToken' not found in request.");
    return;
  }

  ParseAuthTokenWithService(auth_service_with_token,
                            &xmpp_config.auth_token,
                            &xmpp_config.auth_service);
  if (xmpp_config.auth_token.empty()) {
    SendErrorAndExit(
        response.Pass(),
        "Invalid 'authServiceWithToken': " + auth_service_with_token);
    return;
  }

#if !defined(NDEBUG)
  std::string address;
  if (!message.GetString("xmppServerAddress", &address)) {
    SendErrorAndExit(response.Pass(),
                     "'xmppServerAddress' not found in request.");
    return;
  }

  if (!net::ParseHostAndPort(
           address, &xmpp_server_config_.host, &xmpp_server_config_.port)) {
    SendErrorAndExit(response.Pass(),
                     "Invalid 'xmppServerAddress': " + address);
    return;
  }

  if (!message.GetBoolean("xmppServerUseTls", &xmpp_server_config_.use_tls)) {
    SendErrorAndExit(response.Pass(),
                     "'xmppServerUseTls' not found in request.");
    return;
  }

  if (!message.GetString("directoryBotJid", &directory_bot_jid_)) {
    SendErrorAndExit(response.Pass(),
                     "'directoryBotJid' not found in request.");
    return;
  }
#endif  // !defined(NDEBUG)

  // Create the It2Me host and start connecting.
  it2me_host_ = factory_->CreateIt2MeHost(host_context_.get(),
                                          host_context_->ui_task_runner(),
                                          weak_ptr_,
                                          xmpp_config,
                                          directory_bot_jid_);
  it2me_host_->Connect();

  channel_->SendMessage(response.Pass());
}

void It2MeNativeMessagingHost::ProcessDisconnect(
    const base::DictionaryValue& message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  if (it2me_host_.get()) {
    it2me_host_->Disconnect();
    it2me_host_ = NULL;
  }
  channel_->SendMessage(response.Pass());
}

void It2MeNativeMessagingHost::SendErrorAndExit(
    scoped_ptr<base::DictionaryValue> response,
    const std::string& description) const {
  DCHECK(task_runner()->BelongsToCurrentThread());

  LOG(ERROR) << description;

  response->SetString("type", "error");
  response->SetString("description", description);
  channel_->SendMessage(response.Pass());

  // Trigger a host shutdown by sending a NULL message.
  channel_->SendMessage(scoped_ptr<base::DictionaryValue>());
}

void It2MeNativeMessagingHost::OnStateChanged(It2MeHostState state) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  state_ = state;

  scoped_ptr<base::DictionaryValue> message(new base::DictionaryValue());

  message->SetString("type", "hostStateChanged");
  message->SetString("state",
                     It2MeNativeMessagingHost::HostStateToString(state));

  switch (state_) {
    case kReceivedAccessCode:
      message->SetString("accessCode", access_code_);
      message->SetInteger("accessCodeLifetime",
                          access_code_lifetime_.InSeconds());
      break;

    case kConnected:
      message->SetString("client", client_username_);
      break;

    case kDisconnected:
      client_username_.clear();
      break;

    default:
      ;
  }

  channel_->SendMessage(message.Pass());
}

void It2MeNativeMessagingHost::OnNatPolicyChanged(bool nat_traversal_enabled) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  scoped_ptr<base::DictionaryValue> message(new base::DictionaryValue());

  message->SetString("type", "natPolicyChanged");
  message->SetBoolean("natTraversalEnabled", nat_traversal_enabled);
  channel_->SendMessage(message.Pass());
}

// Stores the Access Code for the web-app to query.
void It2MeNativeMessagingHost::OnStoreAccessCode(
    const std::string& access_code,
    base::TimeDelta access_code_lifetime) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  access_code_ = access_code;
  access_code_lifetime_ = access_code_lifetime;
}

// Stores the client user's name for the web-app to query.
void It2MeNativeMessagingHost::OnClientAuthenticated(
    const std::string& client_username) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  client_username_ = client_username;
}

scoped_refptr<AutoThreadTaskRunner>
It2MeNativeMessagingHost::task_runner() const {
  return host_context_->ui_task_runner();
}

/* static */
std::string It2MeNativeMessagingHost::HostStateToString(
    It2MeHostState host_state) {
  return ValueToName(kIt2MeHostStates, host_state);
}

}  // namespace remoting

