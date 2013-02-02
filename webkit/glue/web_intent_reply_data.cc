// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/web_intent_reply_data.h"

namespace webkit_glue {
namespace {

const int64 kUnknownFileSize = -1;

}

WebIntentReply::WebIntentReply()
    : type(WEB_INTENT_REPLY_INVALID), data_file_size(kUnknownFileSize) {}

WebIntentReply::WebIntentReply(
    WebIntentReplyType response_type, string16 response_data)
    : type(response_type),
      data(response_data),
      data_file_size(kUnknownFileSize) {}

WebIntentReply::WebIntentReply(
    WebIntentReplyType response_type,
    base::FilePath response_data_file,
    int64 response_data_file_length)
    : type(response_type),
      data_file(response_data_file),
      data_file_size(response_data_file_length) {}

bool WebIntentReply::operator==(const WebIntentReply& other) const {
  return type == other.type &&
      data == other.data &&
      data_file == other.data_file &&
      data_file_size == other.data_file_size;
}

}  // namespace webkit_glue
