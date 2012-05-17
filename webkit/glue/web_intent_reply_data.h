// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEB_INTENT_REPLY_DATA_H_
#define WEBKIT_GLUE_WEB_INTENT_REPLY_DATA_H_

#include "base/string16.h"

namespace webkit_glue {

// Constant values use to indicate what type of reply the caller is getting from
// the web intents service page.
enum WebIntentReplyType {
  // Invalid type. Use to initialize reply types.
  WEB_INTENT_REPLY_INVALID,

  // Sent for a reply message (success).
  WEB_INTENT_REPLY_SUCCESS,

  // Sent for a failure message.
  WEB_INTENT_REPLY_FAILURE,

  // Sent if the picker is cancelled without a selection being made.
  WEB_INTENT_PICKER_CANCELLED,

  // Sent if the service contents is closed without any response being sent.
  WEB_INTENT_SERVICE_CONTENTS_CLOSED,
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEB_INTENT_REPLY_DATA_H_
