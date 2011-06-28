// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The XmppSignalStrategy encapsulates all the logic to perform the signaling
// STUN/ICE for jingle via a direct XMPP connection.
//
// This class is not threadsafe.

#ifndef REMOTING_JINGLE_GLUE_XMPP_SIGNAL_STRATEGY_H_
#define REMOTING_JINGLE_GLUE_XMPP_SIGNAL_STRATEGY_H_

#include "remoting/jingle_glue/signal_strategy.h"

#include "base/compiler_specific.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"
#include "third_party/libjingle/source/talk/xmpp/xmppclient.h"

namespace remoting {

class JingleThread;

class XmppSignalStrategy : public SignalStrategy,
                           public buzz::XmppStanzaHandler,
                           public sigslot::has_slots<> {
 public:
  XmppSignalStrategy(JingleThread* thread,
                     const std::string& username,
                     const std::string& auth_token,
                     const std::string& auth_token_service);
  virtual ~XmppSignalStrategy();

  // SignalStrategy interface.
  virtual void Init(StatusObserver* observer) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void SetListener(Listener* listener) OVERRIDE;
  virtual void SendStanza(buzz::XmlElement* stanza) OVERRIDE;
  virtual IqRequest* CreateIqRequest() OVERRIDE;

  // buzz::XmppStanzaHandler interface.
  virtual bool HandleStanza(const buzz::XmlElement* stanza) OVERRIDE;

 private:
  friend class JingleClientTest;

  void OnConnectionStateChanged(buzz::XmppEngine::State state);
  static buzz::PreXmppAuth* CreatePreXmppAuth(
      const buzz::XmppClientSettings& settings);

  JingleThread* thread_;

  Listener* listener_;

  std::string username_;
  std::string auth_token_;
  std::string auth_token_service_;
  buzz::XmppClient* xmpp_client_;
  StatusObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(XmppSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_XMPP_SIGNAL_STRATEGY_H_
