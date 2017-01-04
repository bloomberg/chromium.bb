// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/register_support_host_request.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "remoting/base/constants.h"
#include "remoting/host/host_config.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/jid_util.h"
#include "remoting/signaling/signal_strategy.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"

using buzz::QName;
using buzz::XmlElement;

namespace remoting {

namespace {
// Strings used in the request message we send to the bot.
const char kRegisterQueryTag[] = "register-support-host";
const char kPublicKeyTag[] = "public-key";
const char kSignatureTag[] = "signature";
const char kSignatureTimeAttr[] = "time";

// Strings used to parse responses received from the bot.
const char kRegisterQueryResultTag[] = "register-support-host-result";
const char kSupportIdTag[] = "support-id";
const char kSupportIdLifetimeTag[] = "support-id-lifetime";
}

RegisterSupportHostRequest::RegisterSupportHostRequest(
    SignalStrategy* signal_strategy,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& directory_bot_jid,
    const RegisterCallback& callback)
    : signal_strategy_(signal_strategy),
      key_pair_(key_pair),
      directory_bot_jid_(directory_bot_jid),
      callback_(callback) {
  DCHECK(signal_strategy_);
  DCHECK(key_pair_.get());
  signal_strategy_->AddListener(this);
  iq_sender_.reset(new IqSender(signal_strategy_));
}

RegisterSupportHostRequest::~RegisterSupportHostRequest() {
  if (signal_strategy_)
    signal_strategy_->RemoveListener(this);
}

void RegisterSupportHostRequest::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  if (state == SignalStrategy::CONNECTED) {
    DCHECK(!callback_.is_null());

    request_ = iq_sender_->SendIq(
        buzz::STR_SET, directory_bot_jid_,
        CreateRegistrationRequest(signal_strategy_->GetLocalJid()),
        base::Bind(&RegisterSupportHostRequest::ProcessResponse,
                   base::Unretained(this)));
  } else if (state == SignalStrategy::DISCONNECTED) {
    // We will reach here if signaling fails to connect.
    std::string error_message = "Signal strategy disconnected.";
    LOG(ERROR) << error_message;
    CallCallback(std::string(), base::TimeDelta(), error_message);
  }
}

bool RegisterSupportHostRequest::OnSignalStrategyIncomingStanza(
    const buzz::XmlElement* stanza) {
  return false;
}

std::unique_ptr<XmlElement>
RegisterSupportHostRequest::CreateRegistrationRequest(const std::string& jid) {
  std::unique_ptr<XmlElement> query(
      new XmlElement(QName(kChromotingXmlNamespace, kRegisterQueryTag)));
  XmlElement* public_key = new XmlElement(
      QName(kChromotingXmlNamespace, kPublicKeyTag));
  public_key->AddText(key_pair_->GetPublicKey());
  query->AddElement(public_key);
  query->AddElement(CreateSignature(jid).release());
  return query;
}

std::unique_ptr<XmlElement> RegisterSupportHostRequest::CreateSignature(
    const std::string& jid) {
  std::unique_ptr<XmlElement> signature_tag(
      new XmlElement(QName(kChromotingXmlNamespace, kSignatureTag)));

  int64_t time = static_cast<int64_t>(base::Time::Now().ToDoubleT());
  std::string time_str(base::Int64ToString(time));
  signature_tag->AddAttr(
      QName(kChromotingXmlNamespace, kSignatureTimeAttr), time_str);

  std::string message = NormalizeJid(jid) + ' ' + time_str;
  std::string signature(key_pair_->SignMessage(message));
  signature_tag->AddText(signature);

  return signature_tag;
}

void RegisterSupportHostRequest::ParseResponse(const XmlElement* response,
                                               std::string* support_id,
                                               base::TimeDelta* lifetime,
                                               std::string* error_message) {
  std::ostringstream error;

  std::string type = response->Attr(buzz::QN_TYPE);
  if (type == buzz::STR_ERROR) {
    error << "Received error in response to heartbeat: " << response->Str();
    *error_message = error.str();
    LOG(ERROR) << *error_message;
    return;
  }

  // This method must only be called for error or result stanzas.
  if (type != buzz::STR_RESULT) {
    error << "Received unexpect stanza of type \"" << type << "\"";
    *error_message = error.str();
    LOG(ERROR) << *error_message;
    return;
  }

  const XmlElement* result_element = response->FirstNamed(QName(
      kChromotingXmlNamespace, kRegisterQueryResultTag));
  if (!result_element) {
    error << "<" << kRegisterQueryResultTag
          << "> is missing in the host registration response: "
          << response->Str();
    *error_message = error.str();
    LOG(ERROR) << *error_message;
    return;
  }

  const XmlElement* support_id_element =
      result_element->FirstNamed(QName(kChromotingXmlNamespace, kSupportIdTag));
  if (!support_id_element) {
    error << "<" << kSupportIdTag
          << "> is missing in the host registration response: "
          << response->Str();
    *error_message = error.str();
    LOG(ERROR) << *error_message;
    return;
  }

  const XmlElement* lifetime_element =
      result_element->FirstNamed(QName(kChromotingXmlNamespace,
                                       kSupportIdLifetimeTag));
  if (!lifetime_element) {
    error << "<" << kSupportIdLifetimeTag
          << "> is missing in the host registration response: "
          << response->Str();
    *error_message = error.str();
    LOG(ERROR) << *error_message;
    return;
  }

  int lifetime_int;
  if (!base::StringToInt(lifetime_element->BodyText().c_str(), &lifetime_int) ||
      lifetime_int <= 0) {
    error << "<" << kSupportIdLifetimeTag
          << "> is malformed in the host registration response: "
          << response->Str();
    *error_message = error.str();
    LOG(ERROR) << *error_message;
    return;
  }

  *support_id = support_id_element->BodyText();
  *lifetime = base::TimeDelta::FromSeconds(lifetime_int);
  return;
}

void RegisterSupportHostRequest::ProcessResponse(IqRequest* request,
                                                 const XmlElement* response) {
  std::string support_id;
  base::TimeDelta lifetime;
  std::string error_message;
  ParseResponse(response, &support_id, &lifetime, &error_message);
  CallCallback(support_id, lifetime, error_message);
}

void RegisterSupportHostRequest::CallCallback(
    const std::string& support_id,
    base::TimeDelta lifetime,
    const std::string& error_message) {
  // Cleanup state before calling the callback.
  request_.reset();
  iq_sender_.reset();
  signal_strategy_->RemoveListener(this);
  signal_strategy_ = nullptr;

  base::ResetAndReturn(&callback_).Run(support_id, lifetime, error_message);
}

}  // namespace remoting
