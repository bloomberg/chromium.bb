// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushSubscriptionOptions_h
#define PushSubscriptionOptions_h

#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class DOMArrayBuffer;
class ExceptionState;
class PushSubscriptionOptionsInit;
struct WebPushSubscriptionOptions;

class PushSubscriptionOptions final
    : public GarbageCollected<PushSubscriptionOptions>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Converts developer-provided dictionary to WebPushSubscriptionOptions.
  // Throws if applicationServerKey is invalid.
  MODULES_EXPORT static WebPushSubscriptionOptions ToWeb(
      const PushSubscriptionOptionsInit&,
      ExceptionState&);

  static PushSubscriptionOptions* Create(
      const WebPushSubscriptionOptions& options) {
    return new PushSubscriptionOptions(options);
  }

  bool userVisibleOnly() const { return user_visible_only_; }

  // Mutable by web developer. See https://github.com/w3c/push-api/issues/198.
  DOMArrayBuffer* applicationServerKey() const {
    return application_server_key_;
  }

  void Trace(blink::Visitor*);

 private:
  explicit PushSubscriptionOptions(const WebPushSubscriptionOptions&);

  bool user_visible_only_;
  Member<DOMArrayBuffer> application_server_key_;
};

}  // namespace blink

#endif  // PushSubscriptionOptions_h
