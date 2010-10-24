// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_CHROMOTING_SERVER_H_
#define REMOTING_PROTOCOL_JINGLE_CHROMOTING_SERVER_H_

#include <list>
#include <string>

#include "base/lock.h"
#include "base/ref_counted.h"
#include "remoting/protocol/chromoting_server.h"
#include "remoting/protocol/jingle_chromoting_connection.h"
#include "third_party/libjingle/source/talk/p2p/base/session.h"
#include "third_party/libjingle/source/talk/p2p/base/sessionclient.h"
#include "third_party/libjingle/source/talk/p2p/base/sessiondescription.h"

class MessageLoop;

namespace cricket {
class SessionManager;
}  // namespace cricket

namespace remoting {

// ContentDescription used for chromoting sessions. It simply wraps
// CandidateChromotocolConfig. CandidateChromotocolConfig doesn't inherit
// from ContentDescription to avoid dependency on libjingle in
// ChromotingConnection interface.
class ChromotingContentDescription : public cricket::ContentDescription {
 public:
  explicit ChromotingContentDescription(
      const CandidateChromotocolConfig* config);
  ~ChromotingContentDescription();

  const CandidateChromotocolConfig* config() const {
    return candidate_config_.get();
  }

 private:
  scoped_ptr<const CandidateChromotocolConfig> candidate_config_;
};

// This class implements SessionClient for Chromoting sessions. It acts as a
// server that accepts chromoting connections and can also make new connections
// to other hosts.
class JingleChromotingServer
    : public ChromotingServer,
      public cricket::SessionClient {
 public:
  explicit JingleChromotingServer(MessageLoop* message_loop);

  // Initializes the session client. Doesn't accept ownership of the
  // |session_manager|. Close() must be called _before_ the |session_manager|
  // is destroyed.
  virtual void Init(const std::string& local_jid,
                    cricket::SessionManager* session_manager,
                    NewConnectionCallback* new_connection_callback);

  // ChromotingServer interface.
  virtual scoped_refptr<ChromotingConnection> Connect(
      const std::string& jid,
      CandidateChromotocolConfig* chromotocol_config,
      ChromotingConnection::StateChangeCallback* state_change_callback);
  virtual void Close(Task* closed_task);

  void set_allow_local_ips(bool allow_local_ips);

 protected:
  virtual ~JingleChromotingServer();

 private:
  friend class JingleChromotingConnection;

  // Message loop used by this session client.
  MessageLoop* message_loop();

  // Called by JingleChromotingConnection when a new connection is initiated.
  void AcceptConnection(JingleChromotingConnection* connection,
                        cricket::Session* session);

  void DoConnect(
      scoped_refptr<JingleChromotingConnection> connection,
      const std::string& jid,
      ChromotingConnection::StateChangeCallback* state_change_callback);

  // Creates outgoing session description for an incoming connection.
  cricket::SessionDescription* CreateSessionDescription(
      const CandidateChromotocolConfig* config);

  // cricket::SessionClient interface.
  virtual void OnSessionCreate(cricket::Session* session,
                               bool received_initiate);
  virtual void OnSessionDestroy(cricket::Session* session);

  virtual bool ParseContent(cricket::SignalingProtocol protocol,
                            const buzz::XmlElement* elem,
                            const cricket::ContentDescription** content,
                            cricket::ParseError* error);
  virtual bool WriteContent(cricket::SignalingProtocol protocol,
                            const cricket::ContentDescription* content,
                            buzz::XmlElement** elem,
                            cricket::WriteError* error);

  std::string local_jid_;
  MessageLoop* message_loop_;
  cricket::SessionManager* session_manager_;
  scoped_ptr<NewConnectionCallback> new_connection_callback_;
  bool allow_local_ips_;

  bool closed_;

  std::list<scoped_refptr<JingleChromotingConnection> > connections_;

  DISALLOW_COPY_AND_ASSIGN(JingleChromotingServer);
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_CHROMOTING_SERVER_H_
