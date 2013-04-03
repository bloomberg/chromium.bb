// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/negotiating_host_authenticator.h"

#include <algorithm>
#include <sstream>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/v2_authenticator.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

NegotiatingHostAuthenticator::NegotiatingHostAuthenticator(
    const std::string& local_cert,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& shared_secret_hash,
    AuthenticationMethod::HashFunction hash_function)
    : NegotiatingAuthenticatorBase(WAITING_MESSAGE),
      local_cert_(local_cert),
      local_key_pair_(key_pair),
      shared_secret_hash_(shared_secret_hash) {

  AddMethod(AuthenticationMethod::Spake2(hash_function));
}

NegotiatingHostAuthenticator::~NegotiatingHostAuthenticator() {
}

void NegotiatingHostAuthenticator::ProcessMessage(
    const buzz::XmlElement* message,
    const base::Closure& resume_callback) {
  DCHECK_EQ(state(), WAITING_MESSAGE);

  std::string method_attr = message->Attr(kMethodAttributeQName);
  AuthenticationMethod method = AuthenticationMethod::FromString(method_attr);

  // If the host has already chosen a method, it can't be changed by the client.
  if (current_method_.is_valid() && method != current_method_) {
    state_ = REJECTED;
    rejection_reason_ = PROTOCOL_ERROR;
    resume_callback.Run();
    return;
  }

  // If the client did not specify auth method or specified unknown method,
  // then select the first known method from the supported-methods attribute.
  if (!method.is_valid() ||
      std::find(methods_.begin(), methods_.end(), method) == methods_.end()) {

    method = AuthenticationMethod::Invalid();

    std::string supported_methods_attr =
        message->Attr(kSupportedMethodsAttributeQName);
    if (supported_methods_attr.empty()) {
      // Message contains neither method nor supported-methods attributes.
      state_ = REJECTED;
      rejection_reason_ = PROTOCOL_ERROR;
      resume_callback.Run();
      return;
    }

    // Find the first mutually-supported method in the client's list of
    // supported-methods.
    std::vector<std::string> supported_methods_strs;
    base::SplitString(supported_methods_attr, kSupportedMethodsSeparator,
                      &supported_methods_strs);
    for (std::vector<std::string>::iterator it = supported_methods_strs.begin();
         it != supported_methods_strs.end(); ++it) {
      AuthenticationMethod list_value = AuthenticationMethod::FromString(*it);
      if (list_value.is_valid() &&
          std::find(methods_.begin(),
                    methods_.end(), list_value) != methods_.end()) {
        // Found common method.
        method = list_value;
        break;
      }
    }

    if (!method.is_valid()) {
      // Failed to find a common auth method.
      state_ = REJECTED;
      rejection_reason_ = PROTOCOL_ERROR;
      resume_callback.Run();
      return;
    }

    // Drop the current message because we've chosen a different method.
    current_method_ = method;
    state_ = PROCESSING_MESSAGE;
    CreateAuthenticator(MESSAGE_READY, base::Bind(
        &NegotiatingHostAuthenticator::UpdateState,
        base::Unretained(this), resume_callback));
    return;
  }

  // If the client specified a supported method, and the host hasn't chosen a
  // method yet, use the client's preferred method and process the message.
  if (!current_method_.is_valid()) {
    current_method_ = method;
    state_ = PROCESSING_MESSAGE;
    // Copy the message since the authenticator may process it asynchronously.
    CreateAuthenticator(WAITING_MESSAGE, base::Bind(
        &NegotiatingAuthenticatorBase::ProcessMessageInternal,
        base::Unretained(this), base::Owned(new buzz::XmlElement(*message)),
        resume_callback));
    return;
  }

  // If the client is using the host's current method, just process the message.
  ProcessMessageInternal(message, resume_callback);
}

scoped_ptr<buzz::XmlElement> NegotiatingHostAuthenticator::GetNextMessage() {
  return GetNextMessageInternal();
}

void NegotiatingHostAuthenticator::CreateAuthenticator(
    Authenticator::State preferred_initial_state,
    const base::Closure& resume_callback) {
  current_authenticator_ = V2Authenticator::CreateForHost(
      local_cert_, local_key_pair_, shared_secret_hash_,
      preferred_initial_state);
  resume_callback.Run();
}

}  // namespace protocol
}  // namespace remoting
