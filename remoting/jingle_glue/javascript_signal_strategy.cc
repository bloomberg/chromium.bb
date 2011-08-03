// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/javascript_signal_strategy.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "remoting/jingle_glue/iq_request.h"
#include "remoting/jingle_glue/xmpp_proxy.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {

JavascriptSignalStrategy::JavascriptSignalStrategy(const std::string& your_jid)
    : your_jid_(your_jid),
      listener_(NULL),
      last_id_(0) {
}

JavascriptSignalStrategy::~JavascriptSignalStrategy() {
  DCHECK(listener_ == NULL);
  Close();
}

void JavascriptSignalStrategy::AttachXmppProxy(
    scoped_refptr<XmppProxy> xmpp_proxy) {
  xmpp_proxy_ = xmpp_proxy;
  xmpp_proxy_->AttachCallback(AsWeakPtr());
}

void JavascriptSignalStrategy::Init(StatusObserver* observer) {
  DCHECK(CalledOnValidThread());

  // Blast through each state since for a JavascriptSignalStrategy, we're
  // already connected.
  //
  // TODO(ajwong): Clarify the status API contract to see if we have to actually
  // walk through each state.
  observer->OnStateChange(StatusObserver::START);
  observer->OnStateChange(StatusObserver::CONNECTING);
  observer->OnJidChange(your_jid_);
  observer->OnStateChange(StatusObserver::CONNECTED);
}

void JavascriptSignalStrategy::Close() {
  DCHECK(CalledOnValidThread());

  if (xmpp_proxy_) {
    xmpp_proxy_->DetachCallback();
    xmpp_proxy_ = NULL;
  }
}

void JavascriptSignalStrategy::SetListener(Listener* listener) {
  DCHECK(CalledOnValidThread());

  // Don't overwrite an listener without explicitly going
  // through "NULL" first.
  DCHECK(listener_ == NULL || listener == NULL);
  listener_ = listener;
}

void JavascriptSignalStrategy::SendStanza(buzz::XmlElement* stanza) {
  DCHECK(CalledOnValidThread());

  xmpp_proxy_->SendIq(stanza->Str());
  delete stanza;
}

std::string JavascriptSignalStrategy::GetNextId() {
  ++last_id_;
  return base::IntToString(last_id_);
}

IqRequest* JavascriptSignalStrategy::CreateIqRequest() {
  DCHECK(CalledOnValidThread());

  return new JavascriptIqRequest(this, &iq_registry_);
}

void JavascriptSignalStrategy::OnIq(const std::string& stanza_str) {
  scoped_ptr<buzz::XmlElement> stanza(buzz::XmlElement::ForStr(stanza_str));

  if (!stanza.get()) {
    LOG(WARNING) << "Malformed XMPP stanza received: " << stanza_str;
    return;
  }

  if (listener_ && listener_->OnIncomingStanza(stanza.get()))
    return;

  iq_registry_.OnIncomingStanza(stanza.get());
}

}  // namespace remoting
