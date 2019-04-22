/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"
#include "third_party/libjingle_xmpp/xmpp/saslmechanism.h"
#include "third_party/webrtc/rtc_base/third_party/base64/base64.h"

using rtc::Base64;

namespace jingle_xmpp {

XmlElement *
SaslMechanism::StartSaslAuth() {
  return new XmlElement(QN_SASL_AUTH, true);
}

XmlElement *
SaslMechanism::HandleSaslChallenge(const XmlElement * challenge) {
  return new XmlElement(QN_SASL_ABORT, true);
}

void
SaslMechanism::HandleSaslSuccess(const XmlElement * success) {
}

void
SaslMechanism::HandleSaslFailure(const XmlElement * failure) {
}

std::string
SaslMechanism::Base64Encode(const std::string & plain) {
  return Base64::Encode(plain);
}

std::string
SaslMechanism::Base64Decode(const std::string & encoded) {
  return Base64::Decode(encoded, Base64::DO_LAX);
}

std::string
SaslMechanism::Base64EncodeFromArray(const char * plain, size_t length) {
  std::string result;
  Base64::EncodeFromArray(plain, length, &result);
  return result;
}

}
