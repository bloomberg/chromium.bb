// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_SESSION_H_
#define REMOTING_PROTOCOL_JINGLE_SESSION_H_

#include <list>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer/timer.h"
#include "crypto/rsa_private_key.h"
#include "net/base/completion_callback.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/datagram_channel_factory.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/transport.h"
#include "remoting/signaling/iq_sender.h"

namespace remoting {
namespace protocol {

class JingleSessionManager;
class QuicChannelFactory;

// JingleSessionManager and JingleSession implement the subset of the
// Jingle protocol used in Chromoting. Instances of this class are
// created by the JingleSessionManager.
class JingleSession : public base::NonThreadSafe,
                      public Session,
                      public Transport::EventHandler {
 public:
  ~JingleSession() override;

  // Session interface.
  void SetEventHandler(Session::EventHandler* event_handler) override;
  ErrorCode error() override;
  const std::string& jid() override;
  const SessionConfig& config() override;
  Transport* GetTransport() override;
  StreamChannelFactory* GetQuicChannelFactory() override;
  void Close(protocol::ErrorCode error) override;

 private:
  friend class JingleSessionManager;

  typedef base::Callback<void(JingleMessageReply::ErrorType)> ReplyCallback;

  explicit JingleSession(JingleSessionManager* session_manager);

  // Start connection by sending session-initiate message.
  void StartConnection(const std::string& peer_jid,
                       scoped_ptr<Authenticator> authenticator);

  // Called by JingleSessionManager for incoming connections.
  void InitializeIncomingConnection(const JingleMessage& initiate_message,
                                    scoped_ptr<Authenticator> authenticator);
  void AcceptIncomingConnection(const JingleMessage& initiate_message);

  // Sends |message| to the peer. The session is closed if the send fails or no
  // response is received within a reasonable time. All other responses are
  // ignored.
  void SendMessage(const JingleMessage& message);

  // Iq response handler.
  void OnMessageResponse(JingleMessage::ActionType request_type,
                         IqRequest* request,
                         const buzz::XmlElement* response);

  // Transport::EventHandler interface.
  void OnOutgoingTransportInfo(
    scoped_ptr<buzz::XmlElement> transport_info) override;
  void OnTransportRouteChange(const std::string& component,
                              const TransportRoute& route) override;
  void OnTransportError(ErrorCode error) override;

  // Response handler for transport-info responses. Transport-info timeouts are
  // ignored and don't terminate connection.
  void OnTransportInfoResponse(IqRequest* request,
                               const buzz::XmlElement* response);

  // Called by JingleSessionManager on incoming |message|. Must call
  // |reply_callback| to send reply message before sending any other
  // messages.
  void OnIncomingMessage(const JingleMessage& message,
                         const ReplyCallback& reply_callback);

  // Message handlers for incoming messages.
  void OnAccept(const JingleMessage& message,
                const ReplyCallback& reply_callback);
  void OnSessionInfo(const JingleMessage& message,
                     const ReplyCallback& reply_callback);
  void OnTerminate(const JingleMessage& message,
                   const ReplyCallback& reply_callback);

  // Called from OnAccept() to initialize session config.
  bool InitializeConfigFromDescription(const ContentDescription* description);

  // Called after the initial incoming authenticator message is processed.
  void ContinueAcceptIncomingConnection();

  // Called after subsequent authenticator messages are processed.
  void ProcessAuthenticationStep();

  // Called after the authenticating step is finished.
  void ContinueAuthenticationStep();

  // Called when authentication is finished.
  void OnAuthenticated();

  // Sets |state_| to |new_state| and calls state change callback.
  void SetState(State new_state);

  // Returns true if the state of the session is not CLOSED or FAILED
  bool is_session_active();

  JingleSessionManager* session_manager_;
  std::string peer_jid_;
  Session::EventHandler* event_handler_;

  std::string session_id_;
  State state_;
  ErrorCode error_;

  scoped_ptr<SessionConfig> config_;

  scoped_ptr<Authenticator> authenticator_;

  scoped_ptr<Transport> transport_;

  // Pending Iq requests. Used for all messages except transport-info.
  std::set<IqRequest*> pending_requests_;

  // Pending transport-info requests.
  std::list<IqRequest*> transport_info_requests_;

  scoped_ptr<QuicChannelFactory> quic_channel_factory_;

  base::WeakPtrFactory<JingleSession> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(JingleSession);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_SESSION_H_
