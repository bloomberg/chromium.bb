// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HEARTBEAT_SENDER_H_
#define REMOTING_HOST_HEARTBEAT_SENDER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"
#include "remoting/proto/remoting/v1/directory_messages.pb.h"
#include "remoting/signaling/signal_strategy.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace grpc {
class Status;
}  // namespace grpc

namespace remoting {

class OAuthTokenGetter;

// HeartbeatSender periodically sends heartbeat to the directory service. See
// the HeartbeatRequest message in directory_messages.proto for more details.
//
// Normally the heartbeat indicates that the host is healthy and ready to
// accept new connections from a client, but the message can optionally include
// a host_offline_reason field, which indicates that the host cannot accept
// connections from the client (and might possibly be shutting down).  The value
// of the host_offline_reason field can be either a string from
// host_exit_codes.cc (i.e. "INVALID_HOST_CONFIGURATION" string) or one of
// kHostOfflineReasonXxx constants (i.e. "POLICY_READ_ERROR" string).
//
// The heartbeat sender will verify that the channel is in fact active before
// sending out the heartbeat. If not, it will disconnect the signaling strategy
// so that the signaling connector will try to reconnect signaling.
//
// The server sends a HeartbeatResponse in response to each successful
// heartbeat, which may contain a remote command to be executed on the host,
// e.g. restarting the host process upon reception of the response.
class HeartbeatSender final : public SignalStrategy::Listener {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked after the first successful heartbeat.
    virtual void OnFirstHeartbeatSuccessful() = 0;

    // Invoked when the host is not found in the directory.
    virtual void OnHostNotFound() = 0;

    // Invoked when the heartbeat sender permanently fails to authenticate the
    // requests.
    virtual void OnAuthFailed() = 0;

    // Invoked when the host has been asked to restart.
    virtual void OnRemoteRestartHost() = 0;

   protected:
    Delegate() = default;
  };

  // All raw pointers must be non-null and outlive this object.
  HeartbeatSender(Delegate* delegate,
                  const std::string& host_id,
                  SignalStrategy* signal_strategy,
                  OAuthTokenGetter* oauth_token_getter);
  ~HeartbeatSender() override;

  // Sets host offline reason for future heartbeat, and initiates sending a
  // heartbeat right away.
  //
  // For discussion of allowed values for |host_offline_reason| argument,
  // please see the description in the class-level comments above.
  //
  // |ack_callback| will be called when the server acks receiving the
  // |host_offline_reason| or when |timeout| is reached.
  void SetHostOfflineReason(
      const std::string& host_offline_reason,
      const base::TimeDelta& timeout,
      base::OnceCallback<void(bool success)> ack_callback);

 private:
  class HeartbeatClient {
   public:
    using HeartbeatResponseCallback =
        base::OnceCallback<void(const grpc::Status&,
                                const apis::v1::HeartbeatResponse&)>;

    virtual ~HeartbeatClient() = default;

    virtual void Heartbeat(const apis::v1::HeartbeatRequest& request,
                           HeartbeatResponseCallback callback) = 0;
    virtual void CancelPendingRequests() = 0;
  };

  class HeartbeatClientImpl;

  friend class HeartbeatSenderTest;

  // SignalStrategy::Listener interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

  void SendHeartbeat();
  void OnResponse(const grpc::Status& status,
                  const apis::v1::HeartbeatResponse& response);

  // Handlers for host-offline-reason completion and timeout.
  void OnHostOfflineReasonTimeout();
  void OnHostOfflineReasonAck();

  void OnRemoteCommand(
      apis::v1::HeartbeatResponse::RemoteCommand remote_command);

  // Helper methods used by DoSendStanza() to generate heartbeat stanzas.
  apis::v1::HeartbeatRequest CreateHeartbeatRequest();

  Delegate* delegate_;
  std::string host_id_;
  SignalStrategy* const signal_strategy_;
  std::unique_ptr<HeartbeatClient> client_;
  OAuthTokenGetter* const oauth_token_getter_;

  base::OneShotTimer heartbeat_timer_;

  net::BackoffEntry backoff_;

  bool initial_heartbeat_sent_ = false;

  // Fields to send and indicate completion of sending host-offline-reason.
  std::string host_offline_reason_;
  base::OnceCallback<void(bool success)> host_offline_reason_ack_callback_;
  base::OneShotTimer host_offline_reason_timeout_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(HeartbeatSender);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HEARTBEAT_SENDER_H_
