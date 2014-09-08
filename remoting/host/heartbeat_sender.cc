// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include <math.h>

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/time/time.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/host/server_log_entry_host.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/server_log_entry.h"
#include "remoting/signaling/signal_strategy.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

using buzz::QName;
using buzz::XmlElement;

namespace remoting {

namespace {

const char kHeartbeatQueryTag[] = "heartbeat";
const char kHostIdAttr[] = "hostid";
const char kHostVersionTag[] = "host-version";
const char kHeartbeatSignatureTag[] = "signature";
const char kSequenceIdAttr[] = "sequence-id";

const char kErrorTag[] = "error";
const char kNotFoundTag[] = "item-not-found";

const char kHeartbeatResultTag[] = "heartbeat-result";
const char kSetIntervalTag[] = "set-interval";
const char kExpectedSequenceIdTag[] = "expected-sequence-id";

const int64 kDefaultHeartbeatIntervalMs = 5 * 60 * 1000;  // 5 minutes.
const int64 kResendDelayMs = 10 * 1000;  // 10 seconds.
const int64 kResendDelayOnHostNotFoundMs = 10 * 1000; // 10 seconds.
const int kMaxResendOnHostNotFoundCount = 12;  // 2 minutes (12 x 10 seconds).

}  // namespace

HeartbeatSender::HeartbeatSender(
    Listener* listener,
    const std::string& host_id,
    SignalStrategy* signal_strategy,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& directory_bot_jid)
    : listener_(listener),
      host_id_(host_id),
      signal_strategy_(signal_strategy),
      key_pair_(key_pair),
      directory_bot_jid_(directory_bot_jid),
      interval_ms_(kDefaultHeartbeatIntervalMs),
      sequence_id_(0),
      sequence_id_was_set_(false),
      sequence_id_recent_set_num_(0),
      heartbeat_succeeded_(false),
      failed_startup_heartbeat_count_(0) {
  DCHECK(signal_strategy_);
  DCHECK(key_pair_.get());

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
    SendStanza();
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(interval_ms_),
                 this, &HeartbeatSender::SendStanza);
  } else if (state == SignalStrategy::DISCONNECTED) {
    request_.reset();
    iq_sender_.reset();
    timer_.Stop();
    timer_resend_.Stop();
  }
}

bool HeartbeatSender::OnSignalStrategyIncomingStanza(
    const buzz::XmlElement* stanza) {
  return false;
}

void HeartbeatSender::SendStanza() {
  DoSendStanza();
  // Make sure we don't send another heartbeat before the heartbeat interval
  // has expired.
  timer_resend_.Stop();
}

void HeartbeatSender::ResendStanza() {
  DoSendStanza();
  // Make sure we don't send another heartbeat before the heartbeat interval
  // has expired.
  timer_.Reset();
}

void HeartbeatSender::DoSendStanza() {
  VLOG(1) << "Sending heartbeat stanza to " << directory_bot_jid_;
  request_ = iq_sender_->SendIq(
      buzz::STR_SET, directory_bot_jid_, CreateHeartbeatMessage(),
      base::Bind(&HeartbeatSender::ProcessResponse,
                 base::Unretained(this)));
  ++sequence_id_;
}

void HeartbeatSender::ProcessResponse(IqRequest* request,
                                      const XmlElement* response) {
  std::string type = response->Attr(buzz::QN_TYPE);
  if (type == buzz::STR_ERROR) {
    const XmlElement* error_element =
        response->FirstNamed(QName(buzz::NS_CLIENT, kErrorTag));
    if (error_element) {
      if (error_element->FirstNamed(QName(buzz::NS_STANZA, kNotFoundTag))) {
        LOG(ERROR) << "Received error: Host ID not found";
        // If the host was registered immediately before it sends a heartbeat,
        // then server-side latency may prevent the server recognizing the
        // host ID in the heartbeat. So even if all of the first few heartbeats
        // get a "host ID not found" error, that's not a good enough reason to
        // exit.
        failed_startup_heartbeat_count_++;
        if (!heartbeat_succeeded_ && (failed_startup_heartbeat_count_ <=
                kMaxResendOnHostNotFoundCount)) {
          timer_resend_.Start(FROM_HERE,
                              base::TimeDelta::FromMilliseconds(
                                  kResendDelayOnHostNotFoundMs),
                              this,
                              &HeartbeatSender::ResendStanza);
          return;
        }
        listener_->OnUnknownHostIdError();
        return;
      }
    }

    LOG(ERROR) << "Received error in response to heartbeat: "
               << response->Str();
    return;
  }

  // Notify listener of the first successful heartbeat.
  if (!heartbeat_succeeded_) {
    listener_->OnHeartbeatSuccessful();
  }
  heartbeat_succeeded_ = true;

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

    bool did_set_sequence_id = false;
    const XmlElement* expected_sequence_id_element =
        result_element->FirstNamed(QName(kChromotingXmlNamespace,
                                         kExpectedSequenceIdTag));
    if (expected_sequence_id_element) {
      // The sequence ID sent in the previous heartbeat was not what the server
      // expected, so send another heartbeat with the expected sequence ID.
      const std::string& expected_sequence_id_str =
          expected_sequence_id_element->BodyText();
      int expected_sequence_id;
      if (!base::StringToInt(expected_sequence_id_str, &expected_sequence_id)) {
        LOG(ERROR) << "Received invalid " << kExpectedSequenceIdTag << ": " <<
            expected_sequence_id_element->Str();
      } else {
        SetSequenceId(expected_sequence_id);
        sequence_id_recent_set_num_++;
        did_set_sequence_id = true;
      }
    }
    if (!did_set_sequence_id) {
      sequence_id_recent_set_num_ = 0;
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
                   this, &HeartbeatSender::SendStanza);
    }
  }
}

void HeartbeatSender::SetSequenceId(int sequence_id) {
  sequence_id_ = sequence_id;
  // Setting the sequence ID may be a symptom of a temporary server-side
  // problem, which would affect many hosts, so don't send a new heartbeat
  // immediately, as many hosts doing so may overload the server.
  // But the server will usually set the sequence ID when it receives the first
  // heartbeat from a host. In that case, we can send a new heartbeat
  // immediately, as that only happens once per host instance.
  if (!sequence_id_was_set_) {
    ResendStanza();
  } else {
    HOST_LOG << "The heartbeat sequence ID has been set more than once: "
              << "the new value is " << sequence_id;
    double delay = pow(2.0, sequence_id_recent_set_num_) *
        (1 + base::RandDouble()) * kResendDelayMs;
    if (delay <= interval_ms_) {
      timer_resend_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(delay),
                          this, &HeartbeatSender::ResendStanza);
    }
  }
  sequence_id_was_set_ = true;
}

scoped_ptr<XmlElement> HeartbeatSender::CreateHeartbeatMessage() {
  // Create heartbeat stanza.
  scoped_ptr<XmlElement> heartbeat(new XmlElement(
      QName(kChromotingXmlNamespace, kHeartbeatQueryTag)));
  heartbeat->AddAttr(QName(kChromotingXmlNamespace, kHostIdAttr), host_id_);
  heartbeat->AddAttr(QName(kChromotingXmlNamespace, kSequenceIdAttr),
                 base::IntToString(sequence_id_));
  heartbeat->AddElement(CreateSignature().release());
  // Append host version.
  scoped_ptr<XmlElement> version_tag(new XmlElement(
      QName(kChromotingXmlNamespace, kHostVersionTag)));
  version_tag->AddText(STRINGIZE(VERSION));
  heartbeat->AddElement(version_tag.release());
  // Append log message (which isn't signed).
  scoped_ptr<XmlElement> log(ServerLogEntry::MakeStanza());
  scoped_ptr<ServerLogEntry> log_entry(MakeLogEntryForHeartbeat());
  AddHostFieldsToLogEntry(log_entry.get());
  log->AddElement(log_entry->ToStanza().release());
  heartbeat->AddElement(log.release());
  return heartbeat.Pass();
}

scoped_ptr<XmlElement> HeartbeatSender::CreateSignature() {
  scoped_ptr<XmlElement> signature_tag(new XmlElement(
      QName(kChromotingXmlNamespace, kHeartbeatSignatureTag)));

  std::string message = signal_strategy_->GetLocalJid() + ' ' +
      base::IntToString(sequence_id_);
  std::string signature(key_pair_->SignMessage(message));
  signature_tag->AddText(signature);

  return signature_tag.Pass();
}

}  // namespace remoting
