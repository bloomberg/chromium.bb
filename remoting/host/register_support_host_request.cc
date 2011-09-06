// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/register_support_host_request.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
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

RegisterSupportHostRequest::RegisterSupportHostRequest()
    : message_loop_(NULL) {
}

RegisterSupportHostRequest::~RegisterSupportHostRequest() {
}

bool RegisterSupportHostRequest::Init(HostConfig* config,
                                      const RegisterCallback& callback) {
  callback_ = callback;

  if (!key_pair_.Load(config)) {
    return false;
  }
  return true;
}

void RegisterSupportHostRequest::OnSignallingConnected(
    SignalStrategy* signal_strategy,
    const std::string& jid) {
  DCHECK(!callback_.is_null());

  message_loop_ = MessageLoop::current();

  request_.reset(signal_strategy->CreateIqRequest());
  request_->set_callback(base::Bind(
      &RegisterSupportHostRequest::ProcessResponse, base::Unretained(this)));

  request_->SendIq(IqRequest::MakeIqStanza(
      buzz::STR_SET, kChromotingBotJid, CreateRegistrationRequest(jid)));
}

void RegisterSupportHostRequest::OnSignallingDisconnected() {
  if (!message_loop_) {
    // We will reach here with |message_loop_| NULL if the Host's
    // XMPP connection attempt fails.
    CHECK(!callback_.is_null());
    DCHECK(!request_.get());
    callback_.Run(false, std::string(), base::TimeDelta());
    return;
  }
  DCHECK_EQ(message_loop_, MessageLoop::current());
  request_.reset();
}

// Ignore any notifications other than signalling
// connected/disconnected events.
void RegisterSupportHostRequest::OnAccessDenied() { }
void RegisterSupportHostRequest::OnClientAuthenticated(
    remoting::protocol::ConnectionToClient* client) { }
void RegisterSupportHostRequest::OnClientDisconnected(
    remoting::protocol::ConnectionToClient* client) { }
void RegisterSupportHostRequest::OnShutdown() { }

XmlElement* RegisterSupportHostRequest::CreateRegistrationRequest(
    const std::string& jid) {
  XmlElement* query = new XmlElement(
      QName(kChromotingXmlNamespace, kRegisterQueryTag));
  XmlElement* public_key = new XmlElement(
      QName(kChromotingXmlNamespace, kPublicKeyTag));
  public_key->AddText(key_pair_.GetPublicKey());
  query->AddElement(public_key);
  query->AddElement(CreateSignature(jid));
  return query;
}

XmlElement* RegisterSupportHostRequest::CreateSignature(
    const std::string& jid) {
  XmlElement* signature_tag = new XmlElement(
      QName(kChromotingXmlNamespace, kSignatureTag));

  int64 time = static_cast<int64>(base::Time::Now().ToDoubleT());
  std::string time_str(base::Int64ToString(time));
  signature_tag->AddAttr(
      QName(kChromotingXmlNamespace, kSignatureTimeAttr), time_str);

  std::string message = jid + ' ' + time_str;
  std::string signature(key_pair_.GetSignature(message));
  signature_tag->AddText(signature);

  return signature_tag;
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
  DCHECK_EQ(buzz::STR_RESULT, type);

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


void RegisterSupportHostRequest::ProcessResponse(const XmlElement* response) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  std::string support_id;
  base::TimeDelta lifetime;
  bool success = ParseResponse(response, &support_id, &lifetime);
  callback_.Run(success, support_id, lifetime);
}

}  // namespace remoting
