// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "remoting/base/constants.h"
#include "remoting/host/host_config.h"
#include "remoting/jingle_glue/iq_request.h"
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
}

HeartbeatSender::HeartbeatSender(base::MessageLoopProxy* message_loop,
                                 MutableHostConfig* config)

    : state_(CREATED),
      message_loop_(message_loop),
      config_(config),
      interval_ms_(kDefaultHeartbeatIntervalMs) {
  DCHECK(config_);
}

HeartbeatSender::~HeartbeatSender() {
  DCHECK(state_ == CREATED || state_ == INITIALIZED || state_ == STOPPED);
}

bool HeartbeatSender::Init() {
  DCHECK(state_ == CREATED);

  if (!config_->GetString(kHostIdConfigPath, &host_id_)) {
    LOG(ERROR) << "host_id is not defined in the config.";
    return false;
  }

  if (!key_pair_.Load(config_)) {
    return false;
  }

  state_ = INITIALIZED;

  return true;
}

void HeartbeatSender::OnSignallingConnected(SignalStrategy* signal_strategy,
                                            const std::string& full_jid) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == INITIALIZED || state_ == STOPPED);
  state_ = STARTED;

  full_jid_ = full_jid;
  request_.reset(signal_strategy->CreateIqRequest());
  request_->set_callback(base::Bind(&HeartbeatSender::ProcessResponse,
                                    base::Unretained(this)));

  DoSendStanza();
  timer_.Start(base::TimeDelta::FromMilliseconds(interval_ms_), this,
               &HeartbeatSender::DoSendStanza);
}

void HeartbeatSender::OnSignallingDisconnected() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  state_ = STOPPED;
  request_.reset(NULL);
}

// Ignore any notifications other than signalling
// connected/disconnected events.
void HeartbeatSender::OnAccessDenied() { }
void HeartbeatSender::OnClientAuthenticated(
    remoting::protocol::ConnectionToClient* client) { }
void HeartbeatSender::OnClientDisconnected(
    remoting::protocol::ConnectionToClient* client) { }
void HeartbeatSender::OnShutdown() { }

void HeartbeatSender::DoSendStanza() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STARTED);

  VLOG(1) << "Sending heartbeat stanza to " << kChromotingBotJid;
  request_->SendIq(buzz::STR_SET, kChromotingBotJid, CreateHeartbeatMessage());
}

void HeartbeatSender::ProcessResponse(const XmlElement* response) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  std::string type = response->Attr(buzz::QN_TYPE);
  if (type == buzz::STR_ERROR) {
    LOG(ERROR) << "Received error in response to heartbeat: "
               << response->Str();
    return;
  }

  // This method must only be called for error or result stanzas.
  DCHECK_EQ(buzz::STR_RESULT, type);

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
    if (state_ == STARTED) {
      timer_.Stop();
      timer_.Start(base::TimeDelta::FromMilliseconds(interval_ms_), this,
                   &HeartbeatSender::DoSendStanza);
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

  std::string message = full_jid_ + ' ' + time_str;
  std::string signature(key_pair_.GetSignature(message));
  signature_tag->AddText(signature);

  return signature_tag;
}

}  // namespace remoting
