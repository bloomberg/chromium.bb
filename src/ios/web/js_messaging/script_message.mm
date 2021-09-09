// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/js_messaging/script_message.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ScriptMessage::ScriptMessage(std::unique_ptr<base::Value> body,
                             bool is_user_interacting,
                             bool is_main_frame,
                             absl::optional<GURL> request_url)
    : body_(std::move(body)),
      is_user_interacting_(is_user_interacting),
      is_main_frame_(is_main_frame),
      request_url_(request_url) {}
ScriptMessage::~ScriptMessage() = default;

ScriptMessage::ScriptMessage(const ScriptMessage& other)
    : is_user_interacting_(other.is_user_interacting_),
      is_main_frame_(other.is_main_frame_),
      request_url_(other.request_url_) {
  if (other.body_) {
    body_ = other.body_->CreateDeepCopy();
  }
}

}  // namespace web
