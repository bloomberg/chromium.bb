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

namespace remoting {

namespace {
const char kHeartbeatQueryTag[] = "heartbeat";
const char kHostIdAttr[] = "hostid";
const char kHeartbeatSignatureTag[] = "signature";
const char kSignatureTimeAttr[] = "time";

// TODO(sergeyu): Make this configurable by the cloud.
const int64 kHeartbeatPeriodMs = 5 * 60 * 1000;  // 5 minutes.
}

HeartbeatSender::HeartbeatSender()
    : state_(CREATED) {
}

HeartbeatSender::~HeartbeatSender() {
  DCHECK(state_ == CREATED || state_ == INITIALIZED || state_ == STOPPED);
}

bool HeartbeatSender::Init(MutableHostConfig* config,
                           JingleClient* jingle_client) {
  DCHECK(jingle_client);
  DCHECK(state_ == CREATED);

  jingle_client_ = jingle_client;
  config_ = config;

  if (!config_->GetString(kHostIdConfigPath, &host_id_)) {
    LOG(ERROR) << "host_id is not defined in the config.";
    return false;
  }

  if (!key_pair_.Load(config)) {
    return false;
  }

  state_ = INITIALIZED;

  return true;
}

void HeartbeatSender::Start() {
  if (MessageLoop::current() != jingle_client_->message_loop()) {
    jingle_client_->message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &HeartbeatSender::Start));
    return;
  }

  DCHECK_EQ(INITIALIZED, state_);
  state_ = STARTED;

  request_.reset(jingle_client_->CreateIqRequest());
  request_->set_callback(NewCallback(this, &HeartbeatSender::ProcessResponse));

  jingle_client_->message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &HeartbeatSender::DoSendStanza));
}

void HeartbeatSender::Stop() {
  if (MessageLoop::current() != jingle_client_->message_loop()) {
    jingle_client_->message_loop()->PostTask(
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
    DCHECK(MessageLoop::current() == jingle_client_->message_loop());

    VLOG(1) << "Sending heartbeat stanza to " << kChromotingBotJid;

    request_->SendIq(buzz::STR_SET, kChromotingBotJid,
                     CreateHeartbeatMessage());

    // Schedule next heartbeat.
    jingle_client_->message_loop()->PostDelayedTask(
        FROM_HERE, NewRunnableMethod(this, &HeartbeatSender::DoSendStanza),
        kHeartbeatPeriodMs);
  }
}

void HeartbeatSender::ProcessResponse(const buzz::XmlElement* response) {
  if (response->Attr(buzz::QN_TYPE) == buzz::STR_ERROR) {
    LOG(ERROR) << "Received error in response to heartbeat: "
               << response->Str();
  }
}

buzz::XmlElement* HeartbeatSender::CreateHeartbeatMessage() {
  buzz::XmlElement* query = new buzz::XmlElement(
      buzz::QName(kChromotingXmlNamespace, kHeartbeatQueryTag));
  query->AddAttr(buzz::QName(kChromotingXmlNamespace, kHostIdAttr), host_id_);
  query->AddElement(CreateSignature());
  return query;
}

buzz::XmlElement* HeartbeatSender::CreateSignature() {
  buzz::XmlElement* signature_tag = new buzz::XmlElement(
      buzz::QName(kChromotingXmlNamespace, kHeartbeatSignatureTag));

  int64 time = static_cast<int64>(base::Time::Now().ToDoubleT());
  std::string time_str(base::Int64ToString(time));
  signature_tag->AddAttr(
      buzz::QName(kChromotingXmlNamespace, kSignatureTimeAttr), time_str);

  std::string message = jingle_client_->GetFullJid() + ' ' + time_str;
  std::string signature(key_pair_.GetSignature(message));
  signature_tag->AddText(signature);

  return signature_tag;
}

}  // namespace remoting
