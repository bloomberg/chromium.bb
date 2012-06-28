// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_JS_JS_REPLY_HANDLER_H_
#define SYNC_JS_JS_REPLY_HANDLER_H_
#pragma once

// See README.js for design comments.

#include <string>

namespace syncer {

class JsArgList;

// An interface for objects that handle Javascript message replies
// (e.g., WebUIs).
class JsReplyHandler {
 public:
  virtual void HandleJsReply(
      const std::string& name, const JsArgList& args) = 0;

 protected:
  virtual ~JsReplyHandler() {}
};

}  // namespace syncer

#endif  // SYNC_JS_JS_REPLY_HANDLER_H_
