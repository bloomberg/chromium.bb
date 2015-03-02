// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The XmppSignalStrategy encapsulates all the logic to perform the signaling
// STUN/ICE for jingle via a direct XMPP connection.
//
// This class is not threadsafe.

#ifndef REMOTING_SIGNALING_XMPP_SIGNAL_STRATEGY_H_
#define REMOTING_SIGNALING_XMPP_SIGNAL_STRATEGY_H_

#include "remoting/signaling/signal_strategy.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer/timer.h"
#include "third_party/webrtc/base/sigslot.h"
#include "third_party/webrtc/libjingle/xmpp/xmppclient.h"

namespace net {
class ClientSocketFactory;
class URLRequestContextGetter;
}  // namespace net

namespace rtc {
class TaskRunner;
}  // namespace rtc

namespace remoting {

class JingleThread;

class XmppSignalStrategy : public base::NonThreadSafe,
                           public SignalStrategy,
                           public buzz::XmppStanzaHandler,
                           public sigslot::has_slots<> {
 public:
  // XMPP Server configuration for XmppSignalStrategy.
  struct XmppServerConfig {
    XmppServerConfig();
    ~XmppServerConfig();

    std::string host;
    int port;
    bool use_tls;

    std::string username;
    std::string auth_token;
  };

  XmppSignalStrategy(
      net::ClientSocketFactory* socket_factory,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const XmppServerConfig& xmpp_server_config);
  ~XmppSignalStrategy() override;

  // SignalStrategy interface.
  void Connect() override;
  void Disconnect() override;
  State GetState() const override;
  Error GetError() const override;
  std::string GetLocalJid() const override;
  void AddListener(Listener* listener) override;
  void RemoveListener(Listener* listener) override;
  bool SendStanza(scoped_ptr<buzz::XmlElement> stanza) override;
  std::string GetNextId() override;

  // buzz::XmppStanzaHandler interface.
  bool HandleStanza(const buzz::XmlElement* stanza) override;

  // This method is used to update the auth info (for example when the OAuth
  // access token is renewed). It is OK to call this even when we are in the
  // CONNECTED state. It will be used on the next Connect() call.
  void SetAuthInfo(const std::string& username,
                   const std::string& auth_token);

  // Use this method to override the default resource name used (optional).
  // This will be used on the next Connect() call.
  void SetResourceName(const std::string& resource_name);

 private:
  static buzz::PreXmppAuth* CreatePreXmppAuth(
      const buzz::XmppClientSettings& settings);

  void OnConnectionStateChanged(buzz::XmppEngine::State state);
  void SetState(State new_state);

  void SendKeepAlive();

  net::ClientSocketFactory* socket_factory_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  std::string resource_name_;
  scoped_ptr<rtc::TaskRunner> task_runner_;
  buzz::XmppClient* xmpp_client_;
  XmppServerConfig xmpp_server_config_;

  State state_;
  Error error_;

  ObserverList<Listener, true> listeners_;

  base::RepeatingTimer<XmppSignalStrategy> keep_alive_timer_;

  DISALLOW_COPY_AND_ASSIGN(XmppSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_XMPP_SIGNAL_STRATEGY_H_
