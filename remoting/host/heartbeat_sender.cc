// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/iq_sender.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

using buzz::QName;
using buzz::XmlElement;

namespace remoting {

namespace {

const char kHeartbeatQueryTag[] = "heartbeat";
const char kHostIdAttr[] = "hostid";
const char kHeartbeatSignatureTag[] = "signature";
const char kSignatureTimeAttr[] = "time";

const char kHeartbeatResultTag[] = "heartbeat-result";
const char kSetIntervalTag[] = "set-interval";

const int64 kDefaultHeartbeatIntervalMs = 5 * 60 * 1000;  // 5 minutes.

}  // namespace

HeartbeatSender::HeartbeatSender(
    const std::string& host_id,
    SignalStrategy* signal_strategy,
    HostKeyPair* key_pair)
    : host_id_(host_id),
      signal_strategy_(signal_strategy),
      key_pair_(key_pair),
      interval_ms_(kDefaultHeartbeatIntervalMs) {
  DCHECK(signal_strategy_);
  DCHECK(key_pair_);

  signal_strategy_->AddListener(this);

  // Start heartbeats if the |signal_strategy_| is already connected.
  OnSignalStrategyStateChange(signal_strategy_->GetState());
}

HeartbeatSender::~HeartbeatSender() {
  signal_strategy_->RemoveListener(this);
}

void HeartbeatSender::OnSignalStrategyStateChange(SignalStrategy::State state) {
  if (state == SignalStrategy::CONNECTED) {
    iq_sender_.reset(new IqSender(signal_strategy_));
    DoSendStanza();
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(interval_ms_),
                 this, &HeartbeatSender::DoSendStanza);
  } else if (state == SignalStrategy::DISCONNECTED) {
    request_.reset();
    iq_sender_.reset();
    timer_.Stop();
  }
}

void HeartbeatSender::DoSendStanza() {
  VLOG(1) << "Sending heartbeat stanza to " << kChromotingBotJid;
  request_.reset(iq_sender_->SendIq(
      buzz::STR_SET, kChromotingBotJid, CreateHeartbeatMessage(),
      base::Bind(&HeartbeatSender::ProcessResponse,
                 base::Unretained(this))));
}

void HeartbeatSender::ProcessResponse(const XmlElement* response) {
  std::string type = response->Attr(buzz::QN_TYPE);
  if (type == buzz::STR_ERROR) {
    LOG(ERROR) << "Received error in response to heartbeat: "
               << response->Str();
    return;
  }

  // This method must only be called for error or result stanzas.
  DCHECK_EQ(std::string(buzz::STR_RESULT), type);

  const XmlElement* result_element =
      response->FirstNamed(QName(kChromotingXmlNamespace, kHeartbeatResultTag));
  if (result_element) {
    const XmlElement* set_interval_element =
        result_element->FirstNamed(QName(kChromotingXmlNamespace,
                                         kSetIntervalTag));
    if (set_interval_element) {
      const std::string& interval_str = set_interval_element->BodyText();
      int interval;
      if (!base::StringToInt(interval_str, &interval) || interval <= 0) {
        LOG(ERROR) << "Received invalid set-interval: "
                   << set_interval_element->Str();
      } else {
        SetInterval(interval * base::Time::kMillisecondsPerSecond);
      }
    }
  }
}

void HeartbeatSender::SetInterval(int interval) {
  if (interval != interval_ms_) {
    interval_ms_ = interval;

    // Restart the timer with the new interval.
    if (timer_.IsRunning()) {
      timer_.Stop();
      timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(interval_ms_),
                   this, &HeartbeatSender::DoSendStanza);
    }
  }
}

XmlElement* HeartbeatSender::CreateHeartbeatMessage() {
  XmlElement* query = new XmlElement(
      QName(kChromotingXmlNamespace, kHeartbeatQueryTag));
  query->AddAttr(QName(kChromotingXmlNamespace, kHostIdAttr), host_id_);
  query->AddElement(CreateSignature());
  return query;
}

XmlElement* HeartbeatSender::CreateSignature() {
  XmlElement* signature_tag = new XmlElement(
      QName(kChromotingXmlNamespace, kHeartbeatSignatureTag));

  int64 time = static_cast<int64>(base::Time::Now().ToDoubleT());
  std::string time_str(base::Int64ToString(time));
  signature_tag->AddAttr(
      QName(kChromotingXmlNamespace, kSignatureTimeAttr), time_str);

  std::string message = signal_strategy_->GetLocalJid() + ' ' + time_str;
  std::string signature(key_pair_->GetSignature(message));
  signature_tag->AddText(signature);

  return signature_tag;
}

}  // namespace remoting
