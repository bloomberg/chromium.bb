// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_SIGNALING_MANAGER_H_
#define REMOTING_HOST_HOST_SIGNALING_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/oauth_token_getter.h"
#include "remoting/signaling/xmpp_signal_strategy.h"

namespace base {
class TimeDelta;
}

namespace net {
class NetworkChangeNotifier;
}

namespace remoting {

class ChromotingHostContext;
class DnsBlackholeChecker;
class HeartbeatSender;
class OAuthTokenGetter;
class SignalStrategy;
class SignalingConnector;

// HostSignalingManager has 2 functions:
// 1. Keep sending regular heartbeats to the Chromoting Directory.
// 2. Keep the host process alive while sending host-offline-reason heartbeat.
class HostSignalingManager {
 public:
  class Listener {
   public:
    virtual ~Listener() {}

    // Invoked after the first successful heartbeat.
    virtual void OnHeartbeatSuccessful() = 0;

    // Invoked when the host ID is permanently not recognized by the server.
    virtual void OnUnknownHostIdError() = 0;

    // Invoked when authentication fails.
    virtual void OnAuthFailed() = 0;
  };

  // TODO(lukasza): Refactor to limit the number of parameters below.
  // Probably necessitates refactoring HostProcess to extract a new
  // class to read and store config/policy/cmdline values.
  //
  // |listener| has to be valid until either
  //    1) the returned HostSignalingManager is destroyed
  // or 2) SendHostOfflineReasonAndDelete is called.
  static scoped_ptr<HostSignalingManager> Create(
      Listener* listener,
      const scoped_refptr<AutoThreadTaskRunner>& network_task_runner,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter,
      const XmppSignalStrategy::XmppServerConfig& xmpp_server_config,
      const std::string& talkgadget_prefix_policy,
      const std::string& host_id,
      const scoped_refptr<const RsaKeyPair>& host_key_pair,
      const std::string& directory_bot_jid,
      scoped_ptr<OAuthTokenGetter::OAuthCredentials> oauth_credentials);

  ~HostSignalingManager();

  // Get the SignalStrategy to use for talking to the Chromoting bot.
  // Returned SignalStrategy remains owned by the HostSignalingManager.
  SignalStrategy* signal_strategy() { return signal_strategy_.get(); }

  // Kicks off sending a heartbeat containing a host-offline-reason attribute.
  // Prevents future calls to the |listener| provided to the Create method.
  //
  // Will delete |this| once either the bot acks receiving the
  // |host_offline_reason|, or the |timeout| is reached.  Deleting
  // |this| will release |network_task_runner_| and allow the host
  // process to exit.
  void SendHostOfflineReasonAndDelete(const std::string& host_offline_reason,
                                      const base::TimeDelta& timeout);

 private:
  HostSignalingManager(
      scoped_ptr<base::WeakPtrFactory<Listener>> weak_factory_for_listener,
      const scoped_refptr<AutoThreadTaskRunner>& network_task_runner,
      scoped_ptr<SignalStrategy> signal_strategy,
      scoped_ptr<SignalingConnector> signaling_connector,
      scoped_ptr<HeartbeatSender> heartbeat_sender);

  void OnHostOfflineReasonAck(bool success);

  // Used to bind HeartbeatSender and SignalingConnector callbacks to |listener|
  // in a way that allows "detaching" the |listener| when either |this| is
  // destroyed or when SendHostOfflineReasonAndDelete method is called.
  scoped_ptr<base::WeakPtrFactory<Listener>> weak_factory_for_listener_;

  // By holding a reference to |network_task_runner_|, HostSignalingManager is
  // extending the lifetime of the host process.  This is needed for the case
  // where an instance of HostProcess has already been destroyed, but we want
  // to keep the process running while we try to establish a connection and send
  // host-offline-reason.
  scoped_refptr<AutoThreadTaskRunner> network_task_runner_;

  // |heartbeat_sender_| and |signaling_connector_| have to be destroyed before
  // |signal_strategy_| because their destructors need to call
  // signal_strategy_->RemoveListener(this)
  scoped_ptr<SignalStrategy> signal_strategy_;
  scoped_ptr<SignalingConnector> signaling_connector_;
  scoped_ptr<HeartbeatSender> heartbeat_sender_;

  DISALLOW_COPY_AND_ASSIGN(HostSignalingManager);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_SIGNALING_MANAGER_H_
