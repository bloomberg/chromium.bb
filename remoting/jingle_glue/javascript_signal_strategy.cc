// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/javascript_signal_strategy.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "remoting/jingle_glue/xmpp_proxy.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {

JavascriptSignalStrategy::JavascriptSignalStrategy(const std::string& local_jid)
    : local_jid_(local_jid),
      last_id_(0) {
}

JavascriptSignalStrategy::~JavascriptSignalStrategy() {
  DCHECK_EQ(listeners_.size(), 0U);
  Disconnect();
}

void JavascriptSignalStrategy::AttachXmppProxy(
    scoped_refptr<XmppProxy> xmpp_proxy) {
  DCHECK(CalledOnValidThread());
  xmpp_proxy_ = xmpp_proxy;
}

void JavascriptSignalStrategy::Connect() {
  DCHECK(CalledOnValidThread());

  xmpp_proxy_->AttachCallback(AsWeakPtr());
  FOR_EACH_OBSERVER(Listener, listeners_,
                    OnSignalStrategyStateChange(CONNECTED));
}

void JavascriptSignalStrategy::Disconnect() {
  DCHECK(CalledOnValidThread());

  if (xmpp_proxy_)
    xmpp_proxy_->DetachCallback();
  FOR_EACH_OBSERVER(Listener, listeners_,
                    OnSignalStrategyStateChange(DISCONNECTED));
}

SignalStrategy::State JavascriptSignalStrategy::GetState() const {
  DCHECK(CalledOnValidThread());
  // TODO(sergeyu): Extend XmppProxy to provide status of the
  // connection.
  return CONNECTED;
}

SignalStrategy::Error JavascriptSignalStrategy::GetError() const {
  DCHECK(CalledOnValidThread());
  // TODO(sergeyu): Extend XmppProxy to provide status of the
  // connection.
  return OK;
}

std::string JavascriptSignalStrategy::GetLocalJid() const {
  DCHECK(CalledOnValidThread());
  return local_jid_;
}

void JavascriptSignalStrategy::AddListener(Listener* listener) {
  DCHECK(CalledOnValidThread());
  listeners_.AddObserver(listener);
}

void JavascriptSignalStrategy::RemoveListener(Listener* listener) {
  DCHECK(CalledOnValidThread());
  listeners_.RemoveObserver(listener);
}

bool JavascriptSignalStrategy::SendStanza(scoped_ptr<buzz::XmlElement> stanza) {
  DCHECK(CalledOnValidThread());
  xmpp_proxy_->SendIq(stanza->Str());
  return true;
}

std::string JavascriptSignalStrategy::GetNextId() {
  DCHECK(CalledOnValidThread());
  ++last_id_;
  return base::IntToString(last_id_);
}

void JavascriptSignalStrategy::OnIq(const std::string& stanza_str) {
  DCHECK(CalledOnValidThread());
  scoped_ptr<buzz::XmlElement> stanza(buzz::XmlElement::ForStr(stanza_str));
  if (!stanza.get()) {
    LOG(WARNING) << "Malformed XMPP stanza received: " << stanza_str;
    return;
  }

  ObserverListBase<Listener>::Iterator it(listeners_);
  Listener* listener;
  while ((listener = it.GetNext()) != NULL) {
    if (listener->OnSignalStrategyIncomingStanza(stanza.get()))
      break;
  }
}

}  // namespace remoting
