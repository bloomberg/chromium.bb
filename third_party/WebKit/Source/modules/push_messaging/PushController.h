// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushController_h
#define PushController_h

#include "core/frame/LocalFrame.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace blink {

class WebPushClient;

class PushController final : public GarbageCollected<PushController>,
                             public Supplement<LocalFrame> {
  USING_GARBAGE_COLLECTED_MIXIN(PushController);
  WTF_MAKE_NONCOPYABLE(PushController);

 public:
  PushController(LocalFrame&, WebPushClient*);

  static const char* SupplementName();
  static PushController* From(LocalFrame* frame) {
    return static_cast<PushController*>(
        Supplement<LocalFrame>::From(frame, SupplementName()));
  }
  static WebPushClient& ClientFrom(LocalFrame*);

  DEFINE_INLINE_VIRTUAL_TRACE() { Supplement<LocalFrame>::Trace(visitor); }

 private:
  WebPushClient* Client() const { return client_; }

  WebPushClient* client_;
};

MODULES_EXPORT void ProvidePushControllerTo(LocalFrame&, WebPushClient*);

}  // namespace blink

#endif  // PushController_h
