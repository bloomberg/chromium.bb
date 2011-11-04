// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/javascript_signal_strategy.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "remoting/jingle_glue/xmpp_proxy.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {

JavascriptSignalStrategy::JavascriptSignalStrategy(const std::string& your_jid)
    : your_jid_(your_jid),
      last_id_(0) {
}

JavascriptSignalStrategy::~JavascriptSignalStrategy() {
  DCHECK(listeners_.empty());
  Close();
}

void JavascriptSignalStrategy::AttachXmppProxy(
    scoped_refptr<XmppProxy> xmpp_proxy) {
  DCHECK(CalledOnValidThread());
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

void JavascriptSignalStrategy::AddListener(Listener* listener) {
  DCHECK(CalledOnValidThread());
  DCHECK(std::find(listeners_.begin(), listeners_.end(), listener) ==
         listeners_.end());
  listeners_.push_back(listener);
}

void JavascriptSignalStrategy::RemoveListener(Listener* listener) {
  DCHECK(CalledOnValidThread());
  std::vector<Listener*>::iterator it =
      std::find(listeners_.begin(), listeners_.end(), listener);
  CHECK(it != listeners_.end());
  listeners_.erase(it);
}

bool JavascriptSignalStrategy::SendStanza(buzz::XmlElement* stanza) {
  DCHECK(CalledOnValidThread());
  xmpp_proxy_->SendIq(stanza->Str());
  delete stanza;
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

  for (std::vector<Listener*>::iterator it = listeners_.begin();
       it != listeners_.end(); ++it) {
    if ((*it)->OnIncomingStanza(stanza.get()))
      break;
  }
}

}  // namespace remoting
