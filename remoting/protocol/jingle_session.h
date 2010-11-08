// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_SESSION_H_
#define REMOTING_PROTOCOL_JINGLE_SESSION_H_

#include "base/lock.h"
#include "base/ref_counted.h"
#include "remoting/protocol/session.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"
#include "third_party/libjingle/source/talk/p2p/base/session.h"

namespace cricket {
class PseudoTcpChannel;
}  // namespace cricket

namespace net {
class Socket;
}  // namespace net

namespace remoting {

class StreamSocketAdapter;
class TransportChannelSocketAdapter;

namespace protocol {

class JingleSessionManager;

// Implements protocol::Session that work over libjingle session (the
// cricket::Session object is passed to Init() method). Created
// by JingleSessionManager for incoming and outgoing connections.
class JingleSession : public protocol::Session,
                      public sigslot::has_slots<> {
 public:
  static const char kChromotingContentName[];

  explicit JingleSession(JingleSessionManager* client);

  // Chromotocol Session interface.
  virtual void SetStateChangeCallback(StateChangeCallback* callback);

  virtual net::Socket* control_channel();
  virtual net::Socket* event_channel();
  virtual net::Socket* video_channel();

  virtual net::Socket* video_rtp_channel();
  virtual net::Socket* video_rtcp_channel();

  virtual const std::string& jid();
  virtual MessageLoop* message_loop();

  virtual const CandidateSessionConfig* candidate_config();
  virtual const SessionConfig* config();

  virtual void set_config(const SessionConfig* config);

  virtual void Close(Task* closed_task);

 protected:
  virtual ~JingleSession();

 private:
  friend class JingleSessionManager;

  // Called by JingleSessionManager.
  void set_candidate_config(const CandidateSessionConfig* candidate_config);
  void Init(cricket::Session* cricket_session);
  bool HasSession(cricket::Session* cricket_session);
  cricket::Session* ReleaseSession();

  // Used for Session.SignalState sigslot.
  void OnSessionState(cricket::BaseSession* session,
                      cricket::BaseSession::State state);

  void OnInitiate();
  void OnAccept();
  void OnTerminate();

  void SetState(State new_state);

  // JingleSessionManager that created this session.
  scoped_refptr<JingleSessionManager> jingle_session_manager_;

  State state_;
  scoped_ptr<StateChangeCallback> state_change_callback_;

  bool closed_;

  // JID of the other side. Set when the connection is initialized,
  // and never changed after that.
  std::string jid_;

  // The corresponding libjingle session.
  cricket::Session* cricket_session_;

  scoped_ptr<const CandidateSessionConfig> candidate_config_;
  scoped_ptr<const SessionConfig> config_;

  cricket::PseudoTcpChannel* control_channel_;
  scoped_ptr<StreamSocketAdapter> control_channel_adapter_;
  cricket::PseudoTcpChannel* event_channel_;
  scoped_ptr<StreamSocketAdapter> event_channel_adapter_;
  cricket::PseudoTcpChannel* video_channel_;
  scoped_ptr<StreamSocketAdapter> video_channel_adapter_;
  scoped_ptr<TransportChannelSocketAdapter> video_rtp_channel_;
  scoped_ptr<TransportChannelSocketAdapter> video_rtcp_channel_;

  DISALLOW_COPY_AND_ASSIGN(JingleSession);
};

}  // namespace protocol

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_SESSION_H_
