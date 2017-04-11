// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushError_h
#define PushError_h

#include "core/dom/DOMException.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "public/platform/modules/push_messaging/WebPushError.h"

namespace blink {

class ScriptPromiseResolver;

class PushError {
  STATIC_ONLY(PushError);

 public:
  // For CallbackPromiseAdapter.
  using WebType = const WebPushError&;
  static DOMException* Take(ScriptPromiseResolver*,
                            const WebPushError& web_error);
};

}  // namespace blink

#endif  // PushError_h
