// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SessionManagerPair class is used by unittests to create a pair of session
// managers connected to each other. These session managers are then can be
// passed to a pair of JingleChromotocolConnection objects, so that it is
// possible to simulate connection between host and client.

#ifndef REMOTING_PROTOCOL_MOCK_SESSION_MANAGER_H_
#define REMOTING_PROTOCOL_MOCK_SESSION_MANAGER_H_

#include <base/ref_counted.h>
#include <base/scoped_ptr.h>

#include "third_party/libjingle/source/talk/base/sigslot.h"

class MessageLoop;

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace cricket {
class BasicPortAllocator;
class SessionManager;
}  // namespace cricket

namespace talk_base {
class NetworkManager;
class PacketSocketFactory;
}  // namespace talk_base

namespace remoting {

class JingleThread;

class SessionManagerPair
    : public sigslot::has_slots<>,
      public base::RefCountedThreadSafe<SessionManagerPair>{
 public:
  static const char kHostJid[];
  static const char kClientJid[];

  SessionManagerPair(JingleThread* thread);
  virtual ~SessionManagerPair();

  void Init();

  // The session managers are named 'host' and 'client' just for convenience.
  // Both can be used for client or host.
  cricket::SessionManager* host_session_manager();
  cricket::SessionManager* client_session_manager();

 private:
  void ProcessMessage(cricket::SessionManager* manager,
                      const buzz::XmlElement* stanza);
  void DoProcessMessage(cricket::SessionManager* manager,
                       buzz::XmlElement* stanza);
  void DeliverMessage(cricket::SessionManager* to,
                      buzz::XmlElement* stanza);

  MessageLoop* message_loop_;
  scoped_ptr<talk_base::NetworkManager> network_manager_;
  scoped_ptr<talk_base::PacketSocketFactory> socket_factory_;
  scoped_ptr<cricket::BasicPortAllocator> port_allocator_;
  scoped_ptr<cricket::SessionManager> host_session_manager_;
  scoped_ptr<cricket::SessionManager> client_session_manager_;
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_MOCK_SESSION_MANAGER_H_
