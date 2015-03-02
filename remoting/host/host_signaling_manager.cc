// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_signaling_manager.h"

#include "base/bind.h"
#include "base/time/time.h"
#include "net/socket/client_socket_factory.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/dns_blackhole_checker.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/host/signaling_connector.h"
#include "remoting/signaling/xmpp_signal_strategy.h"

namespace remoting {

HostSignalingManager::HostSignalingManager(
    scoped_ptr<SignalStrategy> signal_strategy,
    scoped_ptr<SignalingConnector> signaling_connector,
    scoped_ptr<HeartbeatSender> heartbeat_sender)
    : signal_strategy_(signal_strategy.Pass()),
      signaling_connector_(signaling_connector.Pass()),
      heartbeat_sender_(heartbeat_sender.Pass()) {
}

scoped_ptr<HostSignalingManager> HostSignalingManager::Create(
    Listener* listener,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter,
    const XmppSignalStrategy::XmppServerConfig& xmpp_server_config,
    const std::string& talkgadget_prefix_policy,
    const std::string& host_id,
    const scoped_refptr<const RsaKeyPair>& host_key_pair,
    const std::string& directory_bot_jid,
    scoped_ptr<OAuthTokenGetter::OAuthCredentials> oauth_credentials) {
  scoped_ptr<XmppSignalStrategy> signal_strategy(
      new XmppSignalStrategy(net::ClientSocketFactory::GetDefaultFactory(),
                             url_request_context_getter, xmpp_server_config));

  scoped_ptr<DnsBlackholeChecker> dns_blackhole_checker(new DnsBlackholeChecker(
      url_request_context_getter, talkgadget_prefix_policy));
  scoped_ptr<OAuthTokenGetter> oauth_token_getter(new OAuthTokenGetter(
      oauth_credentials.Pass(), url_request_context_getter, false));

  scoped_ptr<SignalingConnector> signaling_connector(new SignalingConnector(
      signal_strategy.get(), dns_blackhole_checker.Pass(),
      oauth_token_getter.Pass(),
      base::Bind(&Listener::OnAuthFailed, base::Unretained(listener))));

  scoped_ptr<HeartbeatSender> heartbeat_sender(new HeartbeatSender(
      base::Bind(&Listener::OnHeartbeatSuccessful, base::Unretained(listener)),
      base::Bind(&Listener::OnUnknownHostIdError, base::Unretained(listener)),
      host_id, signal_strategy.get(), host_key_pair, directory_bot_jid));

  return scoped_ptr<HostSignalingManager>(new HostSignalingManager(
      signal_strategy.Pass(), signaling_connector.Pass(),
      heartbeat_sender.Pass()));
}

HostSignalingManager::~HostSignalingManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void HostSignalingManager::SendHostOfflineReason(
    const std::string& host_offline_reason,
    const base::TimeDelta& timeout,
    const base::Callback<void(bool success)>& ack_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  HOST_LOG << "SendHostOfflineReason: sending " << host_offline_reason << ".";
  heartbeat_sender_->SetHostOfflineReason(host_offline_reason, timeout,
                                          ack_callback);
}

}  // namespace remoting
