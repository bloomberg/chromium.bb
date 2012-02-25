// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PEPPER_SESSION_H_
#define REMOTING_PROTOCOL_PEPPER_SESSION_H_

#include <list>
#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/timer.h"
#include "crypto/rsa_private_key.h"
#include "net/base/completion_callback.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/transport.h"

namespace net {
class Socket;
class StreamSocket;
}  // namespace net

namespace remoting {

class IqRequest;

namespace protocol {

class JingleSessionManager;

// JingleSessionManager and JingleSession implement the subset of the
// Jingle protocol used in Chromoting. Instances of this class are
// created by the JingleSessionManager.
class JingleSession : public Session,
                      public Transport::EventHandler {
 public:
  virtual ~JingleSession();

  // Session interface.
  virtual void SetStateChangeCallback(
      const StateChangeCallback& callback) OVERRIDE;
  virtual void SetRouteChangeCallback(
      const RouteChangeCallback& callback) OVERRIDE;
  virtual Error error() OVERRIDE;
  virtual void CreateStreamChannel(
      const std::string& name,
      const StreamChannelCallback& callback) OVERRIDE;
  virtual void CreateDatagramChannel(
      const std::string& name,
      const DatagramChannelCallback& callback) OVERRIDE;
  virtual void CancelChannelCreation(const std::string& name) OVERRIDE;
  virtual const std::string& jid() OVERRIDE;
  virtual const CandidateSessionConfig* candidate_config() OVERRIDE;
  virtual const SessionConfig& config() OVERRIDE;
  virtual void set_config(const SessionConfig& config) OVERRIDE;
  virtual void Close() OVERRIDE;

  // Transport::EventHandler interface.
  virtual void OnTransportCandidate(
      Transport* transport,
      const cricket::Candidate& candidate) OVERRIDE;
  virtual void OnTransportRouteChange(Transport* transport,
                                      const TransportRoute& route) OVERRIDE;
  virtual void OnTransportDeleted(Transport* transport) OVERRIDE;

 private:
  friend class JingleSessionManager;

  typedef std::map<std::string, Transport*> ChannelsMap;
  typedef base::Callback<void(JingleMessageReply::ErrorType)> ReplyCallback;

  explicit JingleSession(JingleSessionManager* session_manager);

  // Start connection by sending session-initiate message.
  void StartConnection(const std::string& peer_jid,
                       scoped_ptr<Authenticator> authenticator,
                       scoped_ptr<CandidateSessionConfig> config,
                       const StateChangeCallback& state_change_callback);

  // Called by JingleSessionManager for incoming connections.
  void InitializeIncomingConnection(const JingleMessage& initiate_message,
                                    scoped_ptr<Authenticator> authenticator);
  void AcceptIncomingConnection(const JingleMessage& initiate_message);

  // Handler for session-initiate response.
  void OnSessionInitiateResponse(const buzz::XmlElement* response);

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
  void ProcessTransportInfo(const JingleMessage& message);

  // Called from OnAccept() to initialize session config.
  bool InitializeConfigFromDescription(const ContentDescription* description);

  void ProcessAuthenticationStep();
  void OnSessionInfoResponse(const buzz::XmlElement* response);

  void SendTransportInfo();
  void OnTransportInfoResponse(const buzz::XmlElement* response);

  // Terminates the session and sends session-terminate if it is
  // necessary. |error| specifies the error code in case when the
  // session is being closed due to an error.
  void CloseInternal(Error error);

  // Sets |state_| to |new_state| and calls state change callback.
  void SetState(State new_state);

  JingleSessionManager* session_manager_;
  std::string peer_jid_;
  scoped_ptr<CandidateSessionConfig> candidate_config_;
  StateChangeCallback state_change_callback_;
  RouteChangeCallback route_change_callback_;

  std::string session_id_;
  State state_;
  Error error_;

  SessionConfig config_;
  bool config_is_set_;

  scoped_ptr<Authenticator> authenticator_;

  scoped_ptr<IqRequest> initiate_request_;
  scoped_ptr<IqRequest> session_info_request_;
  scoped_ptr<IqRequest> transport_info_request_;

  ChannelsMap channels_;

  base::OneShotTimer<JingleSession> transport_infos_timer_;
  std::list<cricket::Candidate> pending_candidates_;

  DISALLOW_COPY_AND_ASSIGN(JingleSession);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PEPPER_SESSION_H_
