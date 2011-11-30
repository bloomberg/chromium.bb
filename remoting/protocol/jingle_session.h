// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_SESSION_H_
#define REMOTING_PROTOCOL_JINGLE_SESSION_H_

#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "net/base/completion_callback.h"
#include "remoting/protocol/session.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"
#include "third_party/libjingle/source/talk/p2p/base/session.h"

namespace remoting {
namespace protocol {

class Authenticator;
class JingleChannelConnector;
class JingleSessionManager;

// Implements protocol::Session that work over libjingle session (the
// cricket::Session object is passed to Init() method). Created
// by JingleSessionManager for incoming and outgoing connections.
class JingleSession : public protocol::Session,
                      public sigslot::has_slots<> {
 public:
  // Session interface.
  virtual void SetStateChangeCallback(
      const StateChangeCallback& callback) OVERRIDE;
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

 private:
  friend class JingleDatagramConnector;
  friend class JingleSessionManager;
  friend class JingleStreamConnector;

  typedef std::map<std::string, JingleChannelConnector*> ChannelConnectorsMap;

  // Takes ownership of |authenticator|.
  JingleSession(JingleSessionManager* jingle_session_manager,
                cricket::Session* cricket_session,
                Authenticator* authenticator);
  virtual ~JingleSession();

  // Called by JingleSessionManager.
  void set_candidate_config(const CandidateSessionConfig* candidate_config);

  // Sends session-initiate for new session.
  void SendSessionInitiate();

  // Close all the channels and terminate the session. |result|
  // defines error code that should returned to currently opened
  // channels. |error| specified new value for error().
  void CloseInternal(int result, Error error);

  bool HasSession(cricket::Session* cricket_session);
  cricket::Session* ReleaseSession();

  // Initialize the session configuration from a received connection response
  // stanza.
  bool InitializeConfigFromDescription(
      const cricket::SessionDescription* description);

  // Used for Session.SignalState sigslot.
  void OnSessionState(cricket::BaseSession* session,
                      cricket::BaseSession::State state);
  // Used for Session.SignalError sigslot.
  void OnSessionError(cricket::BaseSession* session,
                      cricket::BaseSession::Error error);

  void OnInitiate();
  void OnAccept();
  void OnTerminate();

  // Notifies upper layer about incoming connection and
  // accepts/rejects connection.
  void AcceptConnection();

  void AddChannelConnector(const std::string& name,
                           JingleChannelConnector* connector);

  // Called by JingleChannelConnector when it has finished connecting
  // the channel, to remove itself from the table of pending connectors.  The
  // connector assumes responsibility for destroying itself after this call.
  void OnChannelConnectorFinished(const std::string& name,
                                  JingleChannelConnector* connector);

  const cricket::ContentInfo* GetContentInfo() const;

  void SetState(State new_state);

  static cricket::SessionDescription* CreateSessionDescription(
      const CandidateSessionConfig* candidate_config,
      const buzz::XmlElement* authenticator_message);

  // JingleSessionManager that created this session. Guaranteed to
  // exist throughout the lifetime of the session.
  JingleSessionManager* jingle_session_manager_;

  scoped_ptr<Authenticator> authenticator_;

  State state_;
  StateChangeCallback state_change_callback_;

  Error error_;

  bool closing_;

  // JID of the other side. Set when the connection is initialized,
  // and never changed after that.
  std::string jid_;

  // The corresponding libjingle session.
  cricket::Session* cricket_session_;

  SessionConfig config_;
  bool config_set_;

  // These data members are only set on the receiving side.
  scoped_ptr<const CandidateSessionConfig> candidate_config_;

  // Channels that are currently being connected.
  ChannelConnectorsMap channel_connectors_;

  ScopedRunnableMethodFactory<JingleSession> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(JingleSession);
};

}  // namespace protocol

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_SESSION_H_
