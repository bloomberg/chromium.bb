// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/xmpp_heartbeat_sender.h"

#include <math.h>

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/host/host_details.h"
#include "remoting/host/server_log_entry_host.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/server_log_entry.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"

#ifdef ERROR
#undef ERROR  // Defined by windows.h
#endif

using jingle_xmpp::QName;
using jingle_xmpp::XmlElement;

namespace remoting {

namespace {

const char kHeartbeatQueryTag[] = "heartbeat";
const char kHostIdAttr[] = "hostid";
const char kHostVersionTag[] = "host-version";
const char kHeartbeatSignatureTag[] = "signature";
const char kSequenceIdAttr[] = "sequence-id";
const char kHostOfflineReasonAttr[] = "host-offline-reason";
const char kHostOperatingSystemNameTag[] = "host-os-name";
const char kHostOperatingSystemVersionTag[] = "host-os-version";

const char kErrorTag[] = "error";
const char kNotFoundTag[] = "item-not-found";

const char kHeartbeatResultTag[] = "heartbeat-result";
const char kSetIntervalTag[] = "set-interval";
const char kExpectedSequenceIdTag[] = "expected-sequence-id";

constexpr base::TimeDelta kDefaultHeartbeatInterval =
    base::TimeDelta::FromMinutes(5);
constexpr base::TimeDelta kHeartbeatResponseTimeout =
    base::TimeDelta::FromSeconds(30);
constexpr base::TimeDelta kResendDelay = base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kResendDelayOnHostNotFound =
    base::TimeDelta::FromSeconds(10);

const int kMaxResendOnHostNotFoundCount = 12;  // 2 minutes (12 x 10 seconds).
const int kMaxHeartbeatTimeouts = 2;

}  // namespace

XmppHeartbeatSender::XmppHeartbeatSender(
    const base::RepeatingClosure& on_heartbeat_successful_callback,
    const base::RepeatingClosure& on_unknown_host_id_error,
    const std::string& host_id,
    SignalStrategy* signal_strategy,
    const scoped_refptr<const RsaKeyPair>& host_key_pair,
    const std::string& directory_bot_jid)
    : on_heartbeat_successful_callback_(on_heartbeat_successful_callback),
      on_unknown_host_id_error_(on_unknown_host_id_error),
      host_id_(host_id),
      signal_strategy_(signal_strategy),
      host_key_pair_(host_key_pair),
      directory_bot_jid_(directory_bot_jid),
      interval_(kDefaultHeartbeatInterval) {
  DCHECK(signal_strategy_);
  DCHECK(host_key_pair_.get());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  signal_strategy_->AddListener(this);

  // Start heartbeats if the |signal_strategy_| is already connected.
  OnSignalStrategyStateChange(signal_strategy_->GetState());
}

XmppHeartbeatSender::~XmppHeartbeatSender() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  signal_strategy_->RemoveListener(this);
}

void XmppHeartbeatSender::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (state == SignalStrategy::CONNECTED) {
    DCHECK(!iq_sender_);
    iq_sender_ = std::make_unique<IqSender>(signal_strategy_);
    SendHeartbeat();
  } else if (state == SignalStrategy::DISCONNECTED) {
    request_.reset();
    iq_sender_.reset();
    timer_.AbandonAndStop();
  }
}

bool XmppHeartbeatSender::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

void XmppHeartbeatSender::OnHostOfflineReasonTimeout() {
  DCHECK(host_offline_reason_ack_callback_);

  std::move(host_offline_reason_ack_callback_).Run(false);
}

void XmppHeartbeatSender::OnHostOfflineReasonAck() {
  if (!host_offline_reason_ack_callback_) {
    DCHECK(!host_offline_reason_timeout_timer_.IsRunning());
    return;
  }

  DCHECK(host_offline_reason_timeout_timer_.IsRunning());
  host_offline_reason_timeout_timer_.AbandonAndStop();

  std::move(host_offline_reason_ack_callback_).Run(true);
}

void XmppHeartbeatSender::SetHostOfflineReason(
    const std::string& host_offline_reason,
    const base::TimeDelta& timeout,
    const base::RepeatingCallback<void(bool success)>& ack_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!host_offline_reason_ack_callback_);

  host_offline_reason_ = host_offline_reason;
  host_offline_reason_ack_callback_ = ack_callback;
  host_offline_reason_timeout_timer_.Start(
      FROM_HERE, timeout, this,
      &XmppHeartbeatSender::OnHostOfflineReasonTimeout);
  if (signal_strategy_->GetState() == SignalStrategy::CONNECTED) {
    // Drop timer or pending heartbeat and send a new heartbeat immediately.
    request_.reset();
    timer_.AbandonAndStop();

    SendHeartbeat();
  }
}

void XmppHeartbeatSender::SendHeartbeat() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(1) << "Sending heartbeat stanza to " << directory_bot_jid_;

  if (iq_sender_) {
    DCHECK_EQ(signal_strategy_->GetState(), SignalStrategy::CONNECTED);
    request_ = iq_sender_->SendIq(
        jingle_xmpp::STR_SET, directory_bot_jid_, CreateHeartbeatMessage(),
        base::BindRepeating(&XmppHeartbeatSender::OnResponse,
                            base::Unretained(this)));
  } else {
    DCHECK_EQ(signal_strategy_->GetState(), SignalStrategy::DISCONNECTED);
  }

  if (request_) {
    request_->SetTimeout(kHeartbeatResponseTimeout);
  } else {
    // If we failed to send a new heartbeat, call into the handler to determine
    // whether to retry later or disconnect.
    OnResponse(nullptr, nullptr);
  }
  ++sequence_id_;
}

void XmppHeartbeatSender::OnResponse(IqRequest* request,
                                     const jingle_xmpp::XmlElement* response) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  HeartbeatResult result = ProcessResponse(response);

  if (result == HeartbeatResult::SUCCESS) {
    heartbeat_succeeded_ = true;
    failed_heartbeat_count_ = 0;

    // Notify listener of the first successful heartbeat.
    if (on_heartbeat_successful_callback_) {
      std::move(on_heartbeat_successful_callback_).Run();
    }

    // Notify caller of SetHostOfflineReason that we got an ack and don't
    // schedule another heartbeat.
    if (!host_offline_reason_.empty()) {
      OnHostOfflineReasonAck();
      return;
    }
  } else {
    failed_heartbeat_count_++;
  }

  if (result == HeartbeatResult::TIMEOUT) {
    timed_out_heartbeats_count_++;
    if (timed_out_heartbeats_count_ >= kMaxHeartbeatTimeouts) {
      LOG(ERROR) << "Heartbeat timed out. Reconnecting XMPP.";
      timed_out_heartbeats_count_ = 0;
      // SignalingConnector will reconnect SignalStrategy.
      signal_strategy_->Disconnect();
      return;
    }

    LOG(ERROR) << "Heartbeat timed out.";
  } else {
    timed_out_heartbeats_count_ = 0;
  }

  // If the host was registered immediately before it sends a heartbeat,
  // then server-side latency may prevent the server recognizing the
  // host ID in the heartbeat. So even if all of the first few heartbeats
  // get a "host ID not found" error, that's not a good enough reason to
  // exit.
  if (result == HeartbeatResult::INVALID_HOST_ID &&
      (heartbeat_succeeded_ ||
       (failed_heartbeat_count_ > kMaxResendOnHostNotFoundCount))) {
    on_unknown_host_id_error_.Run();
    return;
  }

  // Response can be received only while signaling connection is active, so we
  // should be able to schedule next message.
  DCHECK_EQ(signal_strategy_->GetState(), SignalStrategy::CONNECTED);

  // Calculate delay before sending the next message.
  base::TimeDelta delay;
  switch (result) {
    case HeartbeatResult::SUCCESS:
      delay = interval_;
      break;

    case HeartbeatResult::INVALID_HOST_ID:
      delay = kResendDelayOnHostNotFound;
      break;

    case HeartbeatResult::SET_SEQUENCE_ID:
      if (failed_heartbeat_count_ == 1 && !heartbeat_succeeded_) {
        // Send next heartbeat immediately after receiving the first
        // set-sequence-id response. Repeated set-sequence-id responses are
        // unexpected, and therefore are handled as errors to avoid overloading
        // the server when it malfunctions.
        delay = base::TimeDelta();
        break;
      }
      // Fall through to handle other cases as errors.
      FALLTHROUGH;
    case HeartbeatResult::TIMEOUT:
    case HeartbeatResult::ERROR:
      delay = pow(2.0, failed_heartbeat_count_) * (1 + base::RandDouble()) *
              kResendDelay;
      break;
  }

  timer_.Start(FROM_HERE, delay, this, &XmppHeartbeatSender::SendHeartbeat);
}

XmppHeartbeatSender::HeartbeatResult XmppHeartbeatSender::ProcessResponse(
    const jingle_xmpp::XmlElement* response) {
  if (!response) {
    return HeartbeatResult::TIMEOUT;
  }

  std::string type = response->Attr(jingle_xmpp::QN_TYPE);
  if (type == jingle_xmpp::STR_ERROR) {
    const XmlElement* error_element =
        response->FirstNamed(QName(jingle_xmpp::NS_CLIENT, kErrorTag));
    if (error_element && error_element->FirstNamed(
                             QName(jingle_xmpp::NS_STANZA, kNotFoundTag))) {
      LOG(ERROR) << "Received error: Host ID not found";
      return HeartbeatResult::INVALID_HOST_ID;
    }
    LOG(ERROR) << "Received error in response to heartbeat: "
               << response->Str();

    return HeartbeatResult::ERROR;
  }

  // This method must only be called for error or result stanzas.
  DCHECK_EQ(std::string(jingle_xmpp::STR_RESULT), type);

  const XmlElement* result_element =
      response->FirstNamed(QName(kChromotingXmlNamespace, kHeartbeatResultTag));
  if (result_element) {
    const XmlElement* set_interval_element = result_element->FirstNamed(
        QName(kChromotingXmlNamespace, kSetIntervalTag));
    if (set_interval_element) {
      const std::string& interval_str = set_interval_element->BodyText();
      int interval_seconds;
      if (!base::StringToInt(interval_str, &interval_seconds) ||
          interval_seconds <= 0) {
        LOG(ERROR) << "Received invalid set-interval: "
                   << set_interval_element->Str();
      } else {
        interval_ = base::TimeDelta::FromSeconds(interval_seconds);
      }
    }

    const XmlElement* expected_sequence_id_element = result_element->FirstNamed(
        QName(kChromotingXmlNamespace, kExpectedSequenceIdTag));
    if (expected_sequence_id_element) {
      // The sequence ID sent in the previous heartbeat was not what the server
      // expected, so send another heartbeat with the expected sequence ID.
      const std::string& expected_sequence_id_str =
          expected_sequence_id_element->BodyText();
      int expected_sequence_id;
      if (!base::StringToInt(expected_sequence_id_str, &expected_sequence_id)) {
        LOG(ERROR) << "Received invalid " << kExpectedSequenceIdTag << ": "
                   << expected_sequence_id_element->Str();

        return HeartbeatResult::ERROR;
      }

      sequence_id_ = expected_sequence_id;
      return HeartbeatResult::SET_SEQUENCE_ID;
    }
  }

  return HeartbeatResult::SUCCESS;
}

std::unique_ptr<XmlElement> XmppHeartbeatSender::CreateHeartbeatMessage() {
  // Create heartbeat stanza.
  std::unique_ptr<XmlElement> heartbeat(
      new XmlElement(QName(kChromotingXmlNamespace, kHeartbeatQueryTag)));
  heartbeat->AddAttr(QName(kChromotingXmlNamespace, kHostIdAttr), host_id_);
  heartbeat->AddAttr(QName(kChromotingXmlNamespace, kSequenceIdAttr),
                     base::NumberToString(sequence_id_));
  if (!host_offline_reason_.empty()) {
    heartbeat->AddAttr(QName(kChromotingXmlNamespace, kHostOfflineReasonAttr),
                       host_offline_reason_);
  }
  heartbeat->AddElement(CreateSignature().release());
  // Append host version.
  std::unique_ptr<XmlElement> version_tag(
      new XmlElement(QName(kChromotingXmlNamespace, kHostVersionTag)));
  version_tag->AddText(STRINGIZE(VERSION));
  heartbeat->AddElement(version_tag.release());
  // If we have not recorded a heartbeat success, continue sending host OS info.
  if (!heartbeat_succeeded_) {
    // Append host OS name.
    std::unique_ptr<XmlElement> os_name_tag(new XmlElement(
        QName(kChromotingXmlNamespace, kHostOperatingSystemNameTag)));
    os_name_tag->AddText(GetHostOperatingSystemName());
    heartbeat->AddElement(os_name_tag.release());
    // Append host OS version.
    std::unique_ptr<XmlElement> os_version_tag(new XmlElement(
        QName(kChromotingXmlNamespace, kHostOperatingSystemVersionTag)));
    os_version_tag->AddText(GetHostOperatingSystemVersion());
    heartbeat->AddElement(os_version_tag.release());
  }
  // Append log message (which isn't signed).
  std::unique_ptr<XmlElement> log(ServerLogEntry::MakeStanza());
  std::unique_ptr<ServerLogEntry> log_entry(MakeLogEntryForHeartbeat());
  AddHostFieldsToLogEntry(log_entry.get());
  log->AddElement(log_entry->ToStanza().release());
  heartbeat->AddElement(log.release());
  return heartbeat;
}

std::unique_ptr<XmlElement> XmppHeartbeatSender::CreateSignature() {
  std::unique_ptr<XmlElement> signature_tag(
      new XmlElement(QName(kChromotingXmlNamespace, kHeartbeatSignatureTag)));

  std::string message = signal_strategy_->GetLocalAddress().jid() + ' ' +
                        base::NumberToString(sequence_id_);
  std::string signature(host_key_pair_->SignMessage(message));
  signature_tag->AddText(signature);

  return signature_tag;
}

}  // namespace remoting
