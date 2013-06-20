// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_signal_strategy.h"

#include "base/strings/string_number_conversions.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {

PepperSignalStrategy::PepperSignalStrategy(
    std::string local_jid,
    const SendIqCallback& send_iq_callback)
    : local_jid_(local_jid),
      send_iq_callback_(send_iq_callback),
      last_id_(0) {
}

PepperSignalStrategy::~PepperSignalStrategy() {
}

void PepperSignalStrategy::OnIncomingMessage(const std::string& message) {
 scoped_ptr<buzz::XmlElement> stanza(buzz::XmlElement::ForStr(message));
  if (!stanza.get()) {
    LOG(WARNING) << "Malformed XMPP stanza received: " << message;
    return;
  }

  ObserverListBase<Listener>::Iterator it(listeners_);
  Listener* listener;
  while ((listener = it.GetNext()) != NULL) {
    if (listener->OnSignalStrategyIncomingStanza(stanza.get()))
      break;
  }
}

void PepperSignalStrategy::Connect() {
}

void PepperSignalStrategy::Disconnect() {
}

SignalStrategy::State PepperSignalStrategy::GetState() const {
  return CONNECTED;
}

SignalStrategy::Error PepperSignalStrategy::GetError() const {
  return OK;
}

std::string PepperSignalStrategy::GetLocalJid() const {
  return local_jid_;
}

void PepperSignalStrategy::AddListener(Listener* listener) {
  listeners_.AddObserver(listener);
}

void PepperSignalStrategy::RemoveListener(Listener* listener) {
  listeners_.RemoveObserver(listener);
}

bool PepperSignalStrategy::SendStanza(scoped_ptr<buzz::XmlElement> stanza) {
  send_iq_callback_.Run(stanza->Str());
  return true;
}

std::string PepperSignalStrategy::GetNextId() {
  ++last_id_;
  return base::IntToString(last_id_);
}

}  // namespace remoting
