// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_XMPP_SIGNAL_STRATEGY_H_
#define REMOTING_SIGNALING_XMPP_SIGNAL_STRATEGY_H_

#include "remoting/signaling/signal_strategy.h"

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace net {
class ClientSocketFactory;
class URLRequestContextGetter;
}  // namespace net

namespace remoting {

// XmppSignalStrategy implements SignalStrategy using direct XMPP connection.
class XmppSignalStrategy : public SignalStrategy {
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

  // This method is used to update the auth info (for example when the OAuth
  // access token is renewed). It is OK to call this even when we are in the
  // CONNECTED state. It will be used on the next Connect() call.
  void SetAuthInfo(const std::string& username,
                   const std::string& auth_token);

 private:
  class Core;

  scoped_ptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(XmppSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_XMPP_SIGNAL_STRATEGY_H_
