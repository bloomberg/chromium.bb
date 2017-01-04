// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/delegating_signal_strategy.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"

namespace remoting {

DelegatingSignalStrategy::DelegatingSignalStrategy(
    std::string local_jid,
    scoped_refptr<base::SingleThreadTaskRunner> client_task_runner,
    const SendIqCallback& send_iq_callback)
    : local_jid_(local_jid),
      delegate_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      client_task_runner_(client_task_runner),
      send_iq_callback_(send_iq_callback),
      weak_factory_(this) {}

DelegatingSignalStrategy::~DelegatingSignalStrategy() {}

void DelegatingSignalStrategy::OnIncomingMessage(const std::string& message) {
  if (!client_task_runner_->BelongsToCurrentThread()) {
    client_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DelegatingSignalStrategy::OnIncomingMessage,
                              weak_factory_.GetWeakPtr(), message));
    return;
  }

  std::unique_ptr<buzz::XmlElement> stanza(buzz::XmlElement::ForStr(message));
  if (!stanza.get()) {
    LOG(WARNING) << "Malformed XMPP stanza received: " << message;
    return;
  }

  for (auto& listener : listeners_) {
    if (listener.OnSignalStrategyIncomingStanza(stanza.get()))
      break;
  }
}

void DelegatingSignalStrategy::Connect() {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  for (auto& observer : listeners_)
    observer.OnSignalStrategyStateChange(CONNECTED);
}

void DelegatingSignalStrategy::Disconnect() {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
}

SignalStrategy::State DelegatingSignalStrategy::GetState() const {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  return CONNECTED;
}

SignalStrategy::Error DelegatingSignalStrategy::GetError() const {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  return OK;
}

std::string DelegatingSignalStrategy::GetLocalJid() const {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  return local_jid_;
}

void DelegatingSignalStrategy::AddListener(Listener* listener) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  listeners_.AddObserver(listener);
}

void DelegatingSignalStrategy::RemoveListener(Listener* listener) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  listeners_.RemoveObserver(listener);
}

bool DelegatingSignalStrategy::SendStanza(
    std::unique_ptr<buzz::XmlElement> stanza) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  stanza->SetAttr(buzz::QN_FROM, GetLocalJid());
  delegate_task_runner_->PostTask(FROM_HERE,
                                  base::Bind(send_iq_callback_, stanza->Str()));
  return true;
}

std::string DelegatingSignalStrategy::GetNextId() {
  return base::Uint64ToString(base::RandUint64());
}

}  // namespace remoting
