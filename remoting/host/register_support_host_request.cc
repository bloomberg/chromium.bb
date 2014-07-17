// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/register_support_host_request.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "remoting/base/constants.h"
#include "remoting/host/host_config.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/signal_strategy.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

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
        CreateRegistrationRequest(signal_strategy_->GetLocalJid()).Pass(),
        base::Bind(&RegisterSupportHostRequest::ProcessResponse,
                   base::Unretained(this)));
  } else if (state == SignalStrategy::DISCONNECTED) {
    // We will reach here if signaling fails to connect.
    CallCallback(false, std::string(), base::TimeDelta());
  }
}

bool RegisterSupportHostRequest::OnSignalStrategyIncomingStanza(
    const buzz::XmlElement* stanza) {
  return false;
}

scoped_ptr<XmlElement> RegisterSupportHostRequest::CreateRegistrationRequest(
    const std::string& jid) {
  scoped_ptr<XmlElement> query(new XmlElement(
      QName(kChromotingXmlNamespace, kRegisterQueryTag)));
  XmlElement* public_key = new XmlElement(
      QName(kChromotingXmlNamespace, kPublicKeyTag));
  public_key->AddText(key_pair_->GetPublicKey());
  query->AddElement(public_key);
  query->AddElement(CreateSignature(jid).release());
  return query.Pass();
}

scoped_ptr<XmlElement> RegisterSupportHostRequest::CreateSignature(
    const std::string& jid) {
  scoped_ptr<XmlElement> signature_tag(new XmlElement(
      QName(kChromotingXmlNamespace, kSignatureTag)));

  int64 time = static_cast<int64>(base::Time::Now().ToDoubleT());
  std::string time_str(base::Int64ToString(time));
  signature_tag->AddAttr(
      QName(kChromotingXmlNamespace, kSignatureTimeAttr), time_str);

  std::string message = jid + ' ' + time_str;
  std::string signature(key_pair_->SignMessage(message));
  signature_tag->AddText(signature);

  return signature_tag.Pass();
}

bool RegisterSupportHostRequest::ParseResponse(const XmlElement* response,
                                               std::string* support_id,
                                               base::TimeDelta* lifetime) {
  std::string type = response->Attr(buzz::QN_TYPE);
  if (type == buzz::STR_ERROR) {
    LOG(ERROR) << "Received error in response to heartbeat: "
               << response->Str();
    return false;
  }

  // This method must only be called for error or result stanzas.
  if (type != buzz::STR_RESULT) {
    LOG(ERROR) << "Received unexpect stanza of type \"" << type << "\"";
    return false;
  }

  const XmlElement* result_element = response->FirstNamed(QName(
      kChromotingXmlNamespace, kRegisterQueryResultTag));
  if (!result_element) {
    LOG(ERROR) << "<" << kRegisterQueryResultTag
               << "> is missing in the host registration response: "
               << response->Str();
    return false;
  }

  const XmlElement* support_id_element =
      result_element->FirstNamed(QName(kChromotingXmlNamespace, kSupportIdTag));
  if (!support_id_element) {
    LOG(ERROR) << "<" << kSupportIdTag
               << "> is missing in the host registration response: "
               << response->Str();
    return false;
  }

  const XmlElement* lifetime_element =
      result_element->FirstNamed(QName(kChromotingXmlNamespace,
                                       kSupportIdLifetimeTag));
  if (!lifetime_element) {
    LOG(ERROR) << "<" << kSupportIdLifetimeTag
               << "> is missing in the host registration response: "
               << response->Str();
    return false;
  }

  int lifetime_int;
  if (!base::StringToInt(lifetime_element->BodyText().c_str(), &lifetime_int) ||
      lifetime_int <= 0) {
    LOG(ERROR) << "<" << kSupportIdLifetimeTag
               << "> is malformed in the host registration response: "
               << response->Str();
    return false;
  }

  *support_id = support_id_element->BodyText();
  *lifetime = base::TimeDelta::FromSeconds(lifetime_int);
  return true;
}

void RegisterSupportHostRequest::ProcessResponse(IqRequest* request,
                                                 const XmlElement* response) {
  std::string support_id;
  base::TimeDelta lifetime;
  bool success = ParseResponse(response, &support_id, &lifetime);
  CallCallback(success, support_id, lifetime);
}

void RegisterSupportHostRequest::CallCallback(
    bool success, const std::string& support_id, base::TimeDelta lifetime) {
  // Cleanup state before calling the callback.
  request_.reset();
  iq_sender_.reset();
  signal_strategy_->RemoveListener(this);
  signal_strategy_ = NULL;

  RegisterCallback callback = callback_;
  callback_.Reset();
  callback.Run(success, support_id, lifetime);
}

}  // namespace remoting
