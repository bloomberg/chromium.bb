// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_JINGLE_CLIENT_H_
#define REMOTING_JINGLE_GLUE_JINGLE_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "remoting/jingle_glue/iq_request.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "remoting/jingle_glue/xmpp_proxy.h"

class MessageLoop;
class Task;

namespace talk_base {
class NetworkManager;
class PacketSocketFactory;
class SocketAddress;
}  // namespace talk_base

namespace cricket {
class HttpPortAllocator;
}  // namespace cricket

namespace cricket {
class BasicPortAllocator;
class SessionManager;
}  // namespace cricket

namespace remoting {

class JingleInfoRequest;
class JingleSignalingConnector;
class PortAllocatorSessionFactory;

class JingleClient : public base::RefCountedThreadSafe<JingleClient>,
                     public SignalStrategy::StatusObserver {
 public:
  class Callback {
   public:
    virtual ~Callback() {}

    // Called when state of the connection is changed.
    virtual void OnStateChange(JingleClient* client, State state) = 0;
  };

  // Physical sockets are used if |network_manager| and
  // |socket_factory| are set to NULL. Otherwise ownership of these
  // objects is given to JingleClient.
  JingleClient(MessageLoop* message_loop,
               SignalStrategy* signal_strategy,
               talk_base::NetworkManager* network_manager,
               talk_base::PacketSocketFactory* socket_factory,
               PortAllocatorSessionFactory* session_factory,
               Callback* callback);
  virtual ~JingleClient();

  // Starts the XMPP connection initialization. Must be called only once.
  // |callback| specifies callback object for the client and must not be NULL.
  void Init();

  // Closes XMPP connection and stops the thread. Must be called
  // before the object is destroyed. |closed_task| is executed after
  // the connection is successfully closed.
  void Close(const base::Closure& closed_task);

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

  // SignalStrategy::StatusObserver implementation.
  virtual void OnStateChange(State state);
  virtual void OnJidChange(const std::string& full_jid);

 private:
  friend class JingleClientTest;

  void DoInitialize();
  void DoStartSession();
  void DoClose(const base::Closure& closed_task);

  // Updates current state of the connection. Must be called only in
  // the jingle thread.
  void UpdateState(State new_state);

  // Virtual for mocking in a unittest.
  void OnJingleInfo(const std::string& token,
                    const std::vector<std::string>& relay_hosts,
                    const std::vector<talk_base::SocketAddress>& stun_hosts);

  // This must be set to true to enable NAT traversal. STUN/Relay
  // servers are not used when NAT traversal is disabled, so P2P
  // connection will works only when both peers are on the same
  // network.
  bool enable_nat_traversing_;

  MessageLoop* message_loop_;

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
  scoped_ptr<talk_base::PacketSocketFactory> socket_factory_;
  scoped_ptr<cricket::HttpPortAllocator> port_allocator_;
  scoped_ptr<PortAllocatorSessionFactory> port_allocator_session_factory_;
  scoped_ptr<cricket::SessionManager> session_manager_;

  scoped_ptr<JingleInfoRequest> jingle_info_request_;
  scoped_ptr<JingleSignalingConnector> jingle_signaling_connector_;

  DISALLOW_COPY_AND_ASSIGN(JingleClient);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_JINGLE_CLIENT_H_
