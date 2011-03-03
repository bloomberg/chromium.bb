// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "remoting/base/constants.h"
#include "remoting/host/host_config.h"
#include "remoting/jingle_glue/iq_request.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "remoting/jingle_glue/jingle_thread.h"
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

HeartbeatSender::HeartbeatSender(MessageLoop* message_loop,
                                 JingleClient* jingle_client,
                                 MutableHostConfig* config)

    : state_(CREATED),
      message_loop_(message_loop),
      jingle_client_(jingle_client),
      config_(config),
      interval_ms_(kDefaultHeartbeatIntervalMs) {
  DCHECK(jingle_client_);
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

void HeartbeatSender::Start() {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &HeartbeatSender::Start));
    return;
  }

  DCHECK_EQ(INITIALIZED, state_);
  state_ = STARTED;

  request_.reset(jingle_client_->CreateIqRequest());
  request_->set_callback(NewCallback(this, &HeartbeatSender::ProcessResponse));

  message_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &HeartbeatSender::DoSendStanza));
}

void HeartbeatSender::Stop() {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &HeartbeatSender::Stop));
    return;
  }

  // We may call Stop() even if we have not started.
  if (state_ != STARTED)
    return;
  state_ = STOPPED;
  request_.reset(NULL);
}

void HeartbeatSender::DoSendStanza() {
  if (state_ == STARTED) {
    // |jingle_client_| may be already destroyed if |state_| is set to
    // |STOPPED|, so don't touch it here unless we are in |STARTED| state.
    DCHECK(MessageLoop::current() == message_loop_);

    VLOG(1) << "Sending heartbeat stanza to " << kChromotingBotJid;

    request_->SendIq(buzz::STR_SET, kChromotingBotJid,
                     CreateHeartbeatMessage());

    // Schedule next heartbeat.
    message_loop_->PostDelayedTask(
        FROM_HERE, NewRunnableMethod(this, &HeartbeatSender::DoSendStanza),
        interval_ms_);
  }
}

void HeartbeatSender::ProcessResponse(const XmlElement* response) {
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
        interval_ms_ = interval * base::Time::kMillisecondsPerSecond;
      }
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

  std::string message = jingle_client_->GetFullJid() + ' ' + time_str;
  std::string signature(key_pair_.GetSignature(message));
  signature_tag->AddText(signature);

  return signature_tag;
}

}  // namespace remoting
