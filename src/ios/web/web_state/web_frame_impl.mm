// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/web_frame_impl.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "crypto/aead.h"
#include "crypto/random.h"
#import "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

WebFrameImpl::WebFrameImpl(const std::string& frame_id,
                           std::unique_ptr<crypto::SymmetricKey> frame_key,
                           int initial_message_id,
                           bool is_main_frame,
                           GURL security_origin,
                           web::WebState* web_state)
    : frame_id_(frame_id),
      frame_key_(std::move(frame_key)),
      next_message_id_(initial_message_id),
      is_main_frame_(is_main_frame),
      security_origin_(security_origin),
      web_state_(web_state) {
  DCHECK(frame_key_);
}

WebFrameImpl::~WebFrameImpl() = default;

WebState* WebFrameImpl::GetWebState() {
  return web_state_;
}

std::string WebFrameImpl::GetFrameId() const {
  return frame_id_;
}

bool WebFrameImpl::IsMainFrame() const {
  return is_main_frame_;
}

GURL WebFrameImpl::GetSecurityOrigin() const {
  return security_origin_;
}

bool WebFrameImpl::CallJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters) {
  int message_id = next_message_id_;
  next_message_id_++;

  base::DictionaryValue message;
  message.SetKey("messageId", base::Value(message_id));
  message.SetKey("functionName", base::Value(name));
  base::ListValue parameters_value(parameters);
  message.SetKey("parameters", std::move(parameters_value));

  std::string json;
  base::JSONWriter::Write(message, &json);

  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(&frame_key_->key());

  std::string iv;
  crypto::RandBytes(base::WriteInto(&iv, aead.NonceLength() + 1),
                    aead.NonceLength());

  std::string ciphertext;
  if (!aead.Seal(json, iv, /*additional_data=*/nullptr, &ciphertext)) {
    LOG(ERROR) << "Error sealing message for WebFrame.";
    return false;
  }

  std::string encoded_iv;
  base::Base64Encode(iv, &encoded_iv);
  std::string encoded_message;
  base::Base64Encode(ciphertext, &encoded_message);
  std::string script = base::StringPrintf(
      "__gCrWeb.frameMessaging.routeMessage('%s', '%s', '%s')",
      encoded_message.c_str(), encoded_iv.c_str(), frame_id_.c_str());
  GetWebState()->ExecuteJavaScript(base::UTF8ToUTF16(script));

  return true;
}

}  // namespace web
