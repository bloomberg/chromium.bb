// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/md5.h"
#include "base/memory/ref_counted.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace net {

const int WebSocketHandshake::kWebSocketPort = 80;
const int WebSocketHandshake::kSecureWebSocketPort = 443;

WebSocketHandshake::WebSocketHandshake(
    const GURL& url,
    const std::string& origin,
    const std::string& location,
    const std::string& protocol)
    : url_(url),
      origin_(origin),
      location_(location),
      protocol_(protocol),
      mode_(MODE_INCOMPLETE) {
}

WebSocketHandshake::~WebSocketHandshake() {
}

bool WebSocketHandshake::is_secure() const {
  return url_.SchemeIs("wss");
}

std::string WebSocketHandshake::CreateClientHandshakeMessage() {
  if (!parameter_.get()) {
    parameter_.reset(new Parameter);
    parameter_->GenerateKeys();
  }
  std::string msg;

  // WebSocket protocol 4.1 Opening handshake.

  msg = "GET ";
  msg += GetResourceName();
  msg += " HTTP/1.1\r\n";

  std::vector<std::string> fields;

  fields.push_back("Upgrade: WebSocket");
  fields.push_back("Connection: Upgrade");

  fields.push_back("Host: " + GetHostFieldValue());

  fields.push_back("Origin: " + GetOriginFieldValue());

  if (!protocol_.empty())
    fields.push_back("Sec-WebSocket-Protocol: " + protocol_);

  // TODO(ukai): Add cookie if necessary.

  fields.push_back("Sec-WebSocket-Key1: " + parameter_->GetSecWebSocketKey1());
  fields.push_back("Sec-WebSocket-Key2: " + parameter_->GetSecWebSocketKey2());

  std::random_shuffle(fields.begin(), fields.end(), base::RandGenerator);

  for (size_t i = 0; i < fields.size(); i++) {
    msg += fields[i] + "\r\n";
  }
  msg += "\r\n";

  msg.append(parameter_->GetKey3());
  return msg;
}

int WebSocketHandshake::ReadServerHandshake(const char* data, size_t len) {
  mode_ = MODE_INCOMPLETE;
  int eoh = HttpUtil::LocateEndOfHeaders(data, len);
  if (eoh < 0)
    return -1;

  scoped_refptr<HttpResponseHeaders> headers(
      new HttpResponseHeaders(HttpUtil::AssembleRawHeaders(data, eoh)));

  if (headers->response_code() != 101) {
    mode_ = MODE_FAILED;
    DVLOG(1) << "Bad response code: " << headers->response_code();
    return eoh;
  }
  mode_ = MODE_NORMAL;
  if (!ProcessHeaders(*headers) || !CheckResponseHeaders()) {
    DVLOG(1) << "Process Headers failed: " << std::string(data, eoh);
    mode_ = MODE_FAILED;
    return eoh;
  }
  if (len < static_cast<size_t>(eoh + Parameter::kExpectedResponseSize)) {
    mode_ = MODE_INCOMPLETE;
    return -1;
  }
  uint8 expected[Parameter::kExpectedResponseSize];
  parameter_->GetExpectedResponse(expected);
  if (memcmp(&data[eoh], expected, Parameter::kExpectedResponseSize)) {
    mode_ = MODE_FAILED;
    return eoh + Parameter::kExpectedResponseSize;
  }
  mode_ = MODE_CONNECTED;
  return eoh + Parameter::kExpectedResponseSize;
}

std::string WebSocketHandshake::GetResourceName() const {
  std::string resource_name = url_.path();
  if (url_.has_query()) {
    resource_name += "?";
    resource_name += url_.query();
  }
  return resource_name;
}

std::string WebSocketHandshake::GetHostFieldValue() const {
  // url_.host() is expected to be encoded in punnycode here.
  std::string host = StringToLowerASCII(url_.host());
  if (url_.has_port()) {
    bool secure = is_secure();
    int port = url_.EffectiveIntPort();
    if ((!secure &&
         port != kWebSocketPort && port != url_parse::PORT_UNSPECIFIED) ||
        (secure &&
         port != kSecureWebSocketPort && port != url_parse::PORT_UNSPECIFIED)) {
      host += ":";
      host += base::IntToString(port);
    }
  }
  return host;
}

std::string WebSocketHandshake::GetOriginFieldValue() const {
  // It's OK to lowercase the origin as the Origin header does not contain
  // the path or query portions, as per
  // http://tools.ietf.org/html/draft-abarth-origin-00.
  //
  // TODO(satorux): Should we trim the port portion here if it's 80 for
  // http:// or 443 for https:// ? Or can we assume it's done by the
  // client of the library?
  return StringToLowerASCII(origin_);
}

/* static */
bool WebSocketHandshake::GetSingleHeader(const HttpResponseHeaders& headers,
                                         const std::string& name,
                                         std::string* value) {
  std::string first_value;
  void* iter = NULL;
  if (!headers.EnumerateHeader(&iter, name, &first_value))
    return false;

  // Checks no more |name| found in |headers|.
  // Second call of EnumerateHeader() must return false.
  std::string second_value;
  if (headers.EnumerateHeader(&iter, name, &second_value))
    return false;
  *value = first_value;
  return true;
}

bool WebSocketHandshake::ProcessHeaders(const HttpResponseHeaders& headers) {
  std::string value;
  if (!GetSingleHeader(headers, "upgrade", &value) ||
      value != "WebSocket")
    return false;

  if (!GetSingleHeader(headers, "connection", &value) ||
      !LowerCaseEqualsASCII(value, "upgrade"))
    return false;

  if (!GetSingleHeader(headers, "sec-websocket-origin", &ws_origin_))
    return false;

  if (!GetSingleHeader(headers, "sec-websocket-location", &ws_location_))
    return false;

  // If |protocol_| is not specified by client, we don't care if there's
  // protocol field or not as specified in the spec.
  if (!protocol_.empty()
      && !GetSingleHeader(headers, "sec-websocket-protocol", &ws_protocol_))
    return false;
  return true;
}

bool WebSocketHandshake::CheckResponseHeaders() const {
  DCHECK(mode_ == MODE_NORMAL);
  if (!LowerCaseEqualsASCII(origin_, ws_origin_.c_str()))
    return false;
  if (location_ != ws_location_)
    return false;
  if (!protocol_.empty() && protocol_ != ws_protocol_)
    return false;
  return true;
}

namespace {

// unsigned int version of base::RandInt().
// we can't use base::RandInt(), because max would be negative if it is
// represented as int, so DCHECK(min <= max) fails.
uint32 RandUint32(uint32 min, uint32 max) {
  DCHECK(min <= max);

  uint64 range = static_cast<int64>(max) - min + 1;
  uint64 number = base::RandUint64();
  // TODO(ukai): fix to be uniform.
  // the distribution of the result of modulo will be biased.
  uint32 result = min + static_cast<uint32>(number % range);
  DCHECK(result >= min && result <= max);
  return result;
}

}

uint32 (*WebSocketHandshake::Parameter::rand_)(uint32 min, uint32 max) =
    RandUint32;
uint8 randomCharacterInSecWebSocketKey[0x2F - 0x20 + 0x7E - 0x39];

WebSocketHandshake::Parameter::Parameter()
    : number_1_(0), number_2_(0) {
  if (randomCharacterInSecWebSocketKey[0] == '\0') {
    int i = 0;
    for (int ch = 0x21; ch <= 0x2F; ch++, i++)
      randomCharacterInSecWebSocketKey[i] = ch;
    for (int ch = 0x3A; ch <= 0x7E; ch++, i++)
      randomCharacterInSecWebSocketKey[i] = ch;
  }
}

WebSocketHandshake::Parameter::~Parameter() {}

void WebSocketHandshake::Parameter::GenerateKeys() {
  GenerateSecWebSocketKey(&number_1_, &key_1_);
  GenerateSecWebSocketKey(&number_2_, &key_2_);
  GenerateKey3();
}

static void SetChallengeNumber(uint8* buf, uint32 number) {
  uint8* p = buf + 3;
  for (int i = 0; i < 4; i++) {
    *p = (uint8)(number & 0xFF);
    --p;
    number >>= 8;
  }
}

void WebSocketHandshake::Parameter::GetExpectedResponse(uint8 *expected) const {
  uint8 challenge[kExpectedResponseSize];
  SetChallengeNumber(&challenge[0], number_1_);
  SetChallengeNumber(&challenge[4], number_2_);
  memcpy(&challenge[8], key_3_.data(), kKey3Size);
  MD5Digest digest;
  MD5Sum(challenge, kExpectedResponseSize, &digest);
  memcpy(expected, digest.a, kExpectedResponseSize);
}

/* static */
void WebSocketHandshake::Parameter::SetRandomNumberGenerator(
    uint32 (*rand)(uint32 min, uint32 max)) {
  rand_ = rand;
}

void WebSocketHandshake::Parameter::GenerateSecWebSocketKey(
    uint32* number, std::string* key) {
  uint32 space = rand_(1, 12);
  uint32 max = 4294967295U / space;
  *number = rand_(0, max);
  uint32 product = *number * space;

  std::string s = base::StringPrintf("%u", product);
  int n = rand_(1, 12);
  for (int i = 0; i < n; i++) {
    int pos = rand_(0, s.length());
    int chpos = rand_(0, sizeof(randomCharacterInSecWebSocketKey) - 1);
    s = s.substr(0, pos).append(1, randomCharacterInSecWebSocketKey[chpos]) +
        s.substr(pos);
  }
  for (uint32 i = 0; i < space; i++) {
    int pos = rand_(1, s.length() - 1);
    s = s.substr(0, pos) + " " + s.substr(pos);
  }
  *key = s;
}

void WebSocketHandshake::Parameter::GenerateKey3() {
  key_3_.clear();
  for (int i = 0; i < 8; i++) {
    key_3_.append(1, rand_(0, 255));
  }
}

}  // namespace net
