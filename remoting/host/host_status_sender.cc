// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_status_sender.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/time/time.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/host/server_log_entry_host.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/server_log_entry.h"
#include "remoting/signaling/signal_strategy.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

using buzz::QName;
using buzz::XmlElement;

namespace remoting {

namespace {

const char kHostStatusTag[] = "host-status";
const char kHostIdAttr[] = "hostid";
const char kExitCodeAttr[] = "exit-code";
const char kHostVersionTag[] = "host-version";
const char kSignatureTag[] = "signature";
const char kStatusAttr[] = "status";
const char kSignatureTimeAttr[] = "time";

}  // namespace

const char* const HostStatusSender::host_status_strings_[] =
{"OFFLINE", "ONLINE"};

HostStatusSender::HostStatusSender(
    const std::string& host_id,
    SignalStrategy* signal_strategy,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& directory_bot_jid)
    : host_id_(host_id),
      signal_strategy_(signal_strategy),
      key_pair_(key_pair),
      directory_bot_jid_(directory_bot_jid) {
  DCHECK(signal_strategy_);
  DCHECK(key_pair_.get());

  signal_strategy_->AddListener(this);
}

HostStatusSender::~HostStatusSender() {
  signal_strategy_->RemoveListener(this);
}

void HostStatusSender::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  if (state == SignalStrategy::CONNECTED)
    iq_sender_.reset(new IqSender(signal_strategy_));
  else if (state == SignalStrategy::DISCONNECTED)
    iq_sender_.reset();
}

bool HostStatusSender::OnSignalStrategyIncomingStanza(
    const XmlElement* stanza) {
  return false;
}

void HostStatusSender::SendOfflineStatus(HostExitCodes exit_code) {
  SendHostStatus(OFFLINE, exit_code);
}

void HostStatusSender::SendOnlineStatus() {
  SendHostStatus(ONLINE, kSuccessExitCode);
}

void HostStatusSender::SendHostStatus(HostStatus status,
                                      HostExitCodes exit_code) {
  SignalStrategy::State state = signal_strategy_->GetState();
  if (state == SignalStrategy::CONNECTED) {
    HOST_LOG << "Sending host status '"
              << HostStatusToString(status)
              << "' to "
              << directory_bot_jid_;

    iq_sender_->SendIq(buzz::STR_SET,
                       directory_bot_jid_,
                       CreateHostStatusMessage(status, exit_code),
                       IqSender::ReplyCallback());
  } else {
    HOST_LOG << "Cannot send host status to '"
              << directory_bot_jid_
              << " ' because the state of the SignalStrategy is "
              << state;
  }
}

scoped_ptr<XmlElement> HostStatusSender::CreateHostStatusMessage(
    HostStatus status, HostExitCodes exit_code) {
  // Create host status stanza.
  scoped_ptr<XmlElement> host_status(new XmlElement(
      QName(kChromotingXmlNamespace, kHostStatusTag)));
  host_status->AddAttr(
      QName(kChromotingXmlNamespace, kHostIdAttr), host_id_);
  host_status->AddAttr(
      QName(kChromotingXmlNamespace, kStatusAttr), HostStatusToString(status));

  if (status == OFFLINE) {
    host_status->AddAttr(
        QName(kChromotingXmlNamespace, kExitCodeAttr),
        ExitCodeToString(exit_code));
  }

  host_status->AddElement(CreateSignature(status, exit_code).release());

  // Append host version.
  scoped_ptr<XmlElement> version_tag(new XmlElement(
      QName(kChromotingXmlNamespace, kHostVersionTag)));
  version_tag->AddText(STRINGIZE(VERSION));
  host_status->AddElement(version_tag.release());

  // Append log message (which isn't signed).
  scoped_ptr<XmlElement> log(ServerLogEntry::MakeStanza());
  scoped_ptr<ServerLogEntry> log_entry(
      MakeLogEntryForHostStatus(status, exit_code));
  AddHostFieldsToLogEntry(log_entry.get());
  log->AddElement(log_entry->ToStanza().release());
  host_status->AddElement(log.release());
  return host_status.Pass();
}

scoped_ptr<XmlElement> HostStatusSender::CreateSignature(
    HostStatus status, HostExitCodes exit_code) {
  scoped_ptr<XmlElement> signature_tag(new XmlElement(
      QName(kChromotingXmlNamespace, kSignatureTag)));

  // Number of seconds since epoch (Jan 1, 1970).
  int64 time = static_cast<int64>(base::Time::Now().ToDoubleT());
  std::string time_str(base::Int64ToString(time));

  signature_tag->AddAttr(
      QName(kChromotingXmlNamespace, kSignatureTimeAttr), time_str);

  // Add a time stamp to the signature to prevent replay attacks.
  std::string message =
      signal_strategy_->GetLocalJid() +
      " " +
      time_str +
      " " +
      HostStatusToString(status);

  if (status == OFFLINE)
    message += std::string(" ") + ExitCodeToString(exit_code);

  std::string signature(key_pair_->SignMessage(message));
  signature_tag->AddText(signature);

  return signature_tag.Pass();
}

}  // namespace remoting
