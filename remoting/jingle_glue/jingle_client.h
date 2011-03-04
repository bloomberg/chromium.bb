// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_JINGLE_CLIENT_H_
#define REMOTING_JINGLE_GLUE_JINGLE_CLIENT_H_

#include <string>

#include "base/gtest_prod_util.h"
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
class HttpPortAllocator;
}  // namespace cricket

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

// TODO(ajwong): The SignalStrategy stuff needs to be separated out to separate
// files.
class SignalStrategy {
 public:
  class StatusObserver {
   public:
    enum State {
      START,
      CONNECTING,
      CONNECTED,
      CLOSED,
    };

    // Called when state of the connection is changed.
    virtual void OnStateChange(State state) = 0;
    virtual void OnJidChange(const std::string& full_jid) = 0;
  };

  SignalStrategy() {}
  virtual ~SignalStrategy() {}
  virtual void Init(StatusObserver* observer) = 0;
  virtual void ConfigureAllocator(
      cricket::HttpPortAllocator* port_allocator, Task* done) = 0;
  virtual void StartSession(cricket::SessionManager* session_manager) = 0;
  virtual void EndSession() = 0;
  virtual IqRequest* CreateIqRequest() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SignalStrategy);
};

class XmppSignalStrategy : public SignalStrategy, public sigslot::has_slots<> {
 public:
  XmppSignalStrategy(JingleThread* thread,
                     const std::string& username,
                     const std::string& auth_token,
                     const std::string& auth_token_service);
  virtual ~XmppSignalStrategy();

  virtual void Init(StatusObserver* observer);
  virtual void ConfigureAllocator(
      cricket::HttpPortAllocator* port_allocator, Task* done);
  virtual void StartSession(cricket::SessionManager* session_manager);
  virtual void EndSession();
  virtual IqRequest* CreateIqRequest();

 private:
  friend class JingleClientTest;

  void OnConnectionStateChanged(buzz::XmppEngine::State state);
  void OnJingleInfo(const std::string& token,
                    const std::vector<std::string>& relay_hosts,
                    const std::vector<talk_base::SocketAddress>& stun_hosts);
  static buzz::PreXmppAuth* CreatePreXmppAuth(
      const buzz::XmppClientSettings& settings);

  JingleThread* thread_;

  std::string username_;
  std::string auth_token_;
  std::string auth_token_service_;
  buzz::XmppClient* xmpp_client_;
  StatusObserver* observer_;
  scoped_ptr<talk_base::NetworkManager> network_manager_;

  cricket::HttpPortAllocator* port_allocator_;
  scoped_ptr<Task> allocator_config_cb_;

  DISALLOW_COPY_AND_ASSIGN(XmppSignalStrategy);
};

class JavascriptSignalStrategy : public SignalStrategy {
 public:
  JavascriptSignalStrategy();
  virtual ~JavascriptSignalStrategy();

  virtual void Init(StatusObserver* observer);
  virtual void ConfigureAllocator(
      cricket::HttpPortAllocator* port_allocator, Task* done);
  virtual void StartSession(cricket::SessionManager* session_manager);
  virtual void EndSession();
  virtual IqRequest* CreateIqRequest();

 private:
  DISALLOW_COPY_AND_ASSIGN(JavascriptSignalStrategy);
};


class JingleClient : public base::RefCountedThreadSafe<JingleClient>,
                     public SignalStrategy::StatusObserver {
 public:
  class Callback {
   public:
    virtual ~Callback() {}

    // Called when state of the connection is changed.
    virtual void OnStateChange(JingleClient* client, State state) = 0;
  };

  JingleClient(JingleThread* thread, SignalStrategy* signal_strategy,
               Callback* callback);
  ~JingleClient();

  // Starts the XMPP connection initialization. Must be called only once.
  // |callback| specifies callback object for the client and must not be NULL.
  void Init();

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
  IqRequest* CreateIqRequest();

  // The session manager used by this client. Must be called from the
  // jingle thread only. Returns NULL if the client is not active.
  cricket::SessionManager* session_manager();

  // Message loop used by this object to execute tasks.
  MessageLoop* message_loop();

 private:
  friend class HeartbeatSenderTest;
  friend class JingleClientTest;

  void DoInitialize();
  void DoStartSession();
  void DoClose();

  // Updates current state of the connection. Must be called only in
  // the jingle thread.
  void UpdateState(State new_state);

  virtual void OnStateChange(State state);
  virtual void OnJidChange(const std::string& full_jid);

  // JingleThread used for the connection. Set in the constructor.
  JingleThread* thread_;

  // Current state of the object.
  // Must be locked when accessing initialized_ or closed_.
  base::Lock state_lock_;
  State state_;
  bool initialized_;
  bool closed_;
  scoped_ptr<Task> closed_task_;

  // TODO(ajwong): HACK! remove this.  See DoStartSession() for details.
  bool initialized_finished_;

  // We need a separate lock for the jid since the |state_lock_| may be held
  // over a callback which can end up having a double lock.
  //
  // TODO(ajwong): Can we avoid holding the |state_lock_| over a callback and
  // remove this extra lock?
  base::Lock jid_lock_;
  std::string full_jid_;

  // Callback for this object. Callback must not be called if closed_ == true.
  Callback* callback_;

  SignalStrategy* signal_strategy_;
  scoped_ptr<talk_base::NetworkManager> network_manager_;
  scoped_ptr<cricket::HttpPortAllocator> port_allocator_;
  scoped_ptr<cricket::SessionManager> session_manager_;

  DISALLOW_COPY_AND_ASSIGN(JingleClient);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_JINGLE_CLIENT_H_
