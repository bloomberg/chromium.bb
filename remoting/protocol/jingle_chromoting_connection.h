// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_CHROMOTING_CONNECTION_H_
#define REMOTING_PROTOCOL_JINGLE_CHROMOTING_CONNECTION_H_

#include "base/lock.h"
#include "base/ref_counted.h"
#include "remoting/protocol/chromoting_connection.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"
#include "third_party/libjingle/source/talk/p2p/base/session.h"

namespace cricket {
class PseudoTcpChannel;
}  // namespace cricket

namespace net {
class Socket;
}  // namespace net

namespace remoting {

class JingleChromotingServer;
class StreamSocketAdapter;
class TransportChannelSocketAdapter;

// Implements ChromotingConnection that work over libjingle session (the
// cricket::Session object is passed to Init() method). Created
// by JingleChromotingServer for incoming and outgoing connections.
class JingleChromotingConnection : public ChromotingConnection,
                                   public sigslot::has_slots<> {
 public:
  explicit JingleChromotingConnection(JingleChromotingServer* client);

  // ChromotingConnection interface.
  virtual void SetStateChangeCallback(StateChangeCallback* callback) ;

  virtual net::Socket* GetVideoChannel();
  virtual net::Socket* GetEventsChannel();
  virtual net::Socket* GetVideoRtpChannel();
  virtual net::Socket* GetVideoRtcpChannel();

  // No synchronization is needed because jid_ is not changed
  // after new connection is passed to JingleChromotingServer callback.
  virtual const std::string& jid() { return jid_; };

  virtual void Close(Task* closed_task);

 protected:
  virtual ~JingleChromotingConnection();

 private:
  friend class JingleChromotingServer;

  // Called by JingleChromotingServer.
  void Init(cricket::Session* session);
  bool HasSession(cricket::Session* session);
  cricket::Session* ReleaseSession();

  // Used for Session.SignalState sigslot.
  void OnSessionState(cricket::BaseSession* session,
                      cricket::BaseSession::State state);
  void OnInitiate(bool incoming);
  void OnAccept();
  void OnTerminate();

  void SetState(State new_state);

  // JingleChromotingServer that created this connection.
  scoped_refptr<JingleChromotingServer> session_client_;

  State state_;
  scoped_ptr<StateChangeCallback> state_change_callback_;

  bool closed_;

  // JID of the other side. Set when the connection is initialized,
  // and never changed after that.
  std::string jid_;

  // The corresponding libjingle session.
  cricket::Session* session_;

  cricket::PseudoTcpChannel* events_channel_;
  scoped_ptr<StreamSocketAdapter> events_channel_adapter_;
  cricket::PseudoTcpChannel* video_channel_;
  scoped_ptr<StreamSocketAdapter> video_channel_adapter_;
  scoped_ptr<TransportChannelSocketAdapter> video_rtp_channel_;
  scoped_ptr<TransportChannelSocketAdapter> video_rtcp_channel_;

  DISALLOW_COPY_AND_ASSIGN(JingleChromotingConnection);
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_CHROMOTING_CONNECTION_H_
