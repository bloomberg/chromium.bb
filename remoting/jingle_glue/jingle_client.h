// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_JINGLE_CLIENT_H_
#define REMOTING_JINGLE_GLUE_JINGLE_CLIENT_H_

#include <string>

#include "base/ref_counted.h"
#include "base/synchronization/lock.h"
#include "third_party/libjingle/source/talk/xmpp/xmppclient.h"

class MessageLoop;
class Task;

namespace talk_base {
class NetworkManager;
}  // namespace talk_base

namespace buzz {
class PreXmppAuth;
}  // namespace buzz

namespace cricket {
class BasicPortAllocator;
class SessionManager;
class TunnelSessionClient;
class SessionManagerTask;
class Session;
}  // namespace cricket

namespace remoting {

class IqRequest;
class JingleThread;

class JingleClient : public base::RefCountedThreadSafe<JingleClient>,
                     public sigslot::has_slots<> {
 public:
  enum State {
    START,  // Initial state.
    CONNECTING,
    CONNECTED,
    CLOSED,
  };

  class Callback {
   public:
    virtual ~Callback() {}

    // Called when state of the connection is changed.
    virtual void OnStateChange(JingleClient* client, State state) = 0;
  };

  // Creates a JingleClient object that executes on |thread|.  This does not
  // take ownership of |thread| and expects that the thread is started before
  // the constructor is called, and only stopped after the JingleClient object
  // has been destructed.
  explicit JingleClient(JingleThread* thread);
  virtual ~JingleClient();

  // Starts the XMPP connection initialization. Must be called only once.
  // |callback| specifies callback object for the client and must not be NULL.
  void Init(const std::string& username, const std::string& auth_token,
            const std::string& auth_token_service, Callback* callback);

  // Closes XMPP connection and stops the thread. Must be called before the
  // object is destroyed. If specified, |closed_task| is executed after the
  // connection is successfully closed.
  void Close();
  void Close(Task* closed_task);

  // Returns JID with resource ID. Empty string is returned if full JID is not
  // known yet, i.e. authentication hasn't finished.
  std::string GetFullJid();

  // Creates new IqRequest for this client. Ownership for of the created object
  // is transfered to the caller.
  virtual IqRequest* CreateIqRequest();

  // Current connection state of the client.
  State state() { return state_; }

  // Returns XmppClient object for the xmpp connection or NULL if not connected.
  buzz::XmppClient* xmpp_client() { return client_; }

  // Message loop used by this object to execute tasks.
  MessageLoop* message_loop();

  // The session manager used by this client. Must be called from the
  // jingle thread only. Returns NULL if the client is not active.
  cricket::SessionManager* session_manager();

 private:
  friend class HeartbeatSenderTest;
  friend class JingleClientTest;

  void OnConnectionStateChanged(buzz::XmppEngine::State state);

  void DoInitialize(const std::string& username,
                    const std::string& auth_token,
                    const std::string& auth_token_service);

  // Used by Close().
  void DoClose();

  void SetFullJid(const std::string& full_jid);

  // Updates current state of the connection. Must be called only in
  // the jingle thread.
  void UpdateState(State new_state);

  buzz::PreXmppAuth* CreatePreXmppAuth(
      const buzz::XmppClientSettings& settings);

  // JingleThread used for the connection. Set in the constructor.
  JingleThread* thread_;

  // Callback for this object. Callback must not be called if closed_ == true.
  Callback* callback_;

  // The XmppClient and its state and jid.
  buzz::XmppClient* client_;
  State state_;
  base::Lock full_jid_lock_;
  std::string full_jid_;

  // Current state of the object.
  // Must be locked when accessing initialized_ or closed_.
  base::Lock state_lock_;
  bool initialized_;
  bool closed_;
  scoped_ptr<Task> closed_task_;

  scoped_ptr<talk_base::NetworkManager> network_manager_;
  scoped_ptr<cricket::BasicPortAllocator> port_allocator_;
  scoped_ptr<cricket::SessionManager> session_manager_;

  DISALLOW_COPY_AND_ASSIGN(JingleClient);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_JINGLE_CLIENT_H_
