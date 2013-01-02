// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_JS_JS_BACKEND_H_
#define SYNC_JS_JS_BACKEND_H_

// See README.js for design comments.

#include <string>

#include "sync/base/sync_export.h"

namespace syncer {

class JsArgList;
class JsEventHandler;
class JsReplyHandler;
template <typename T> class WeakHandle;

// Interface representing the backend of chrome://sync-internals.  A
// JsBackend can handle messages and can emit events to a
// JsEventHandler.
class SYNC_EXPORT_PRIVATE JsBackend {
 public:
  // Starts emitting events to the given handler, if initialized.
  virtual void SetJsEventHandler(
      const WeakHandle<JsEventHandler>& event_handler) = 0;

  // Processes the given message and replies via the given handler, if
  // initialized.
  virtual void ProcessJsMessage(
      const std::string& name, const JsArgList& args,
      const WeakHandle<JsReplyHandler>& reply_handler) = 0;

 protected:
  virtual ~JsBackend() {}
};

}  // namespace syncer

#endif  // SYNC_JS_JS_BACKEND_H_
