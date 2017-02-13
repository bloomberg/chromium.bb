// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ToV8.h"

#include "bindings/core/v8/WindowProxy.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/Frame.h"

namespace blink {

v8::Local<v8::Value> ToV8(DOMWindow* window,
                          v8::Local<v8::Object> creationContext,
                          v8::Isolate* isolate) {
  // Notice that we explicitly ignore creationContext because the DOMWindow
  // has its own creationContext.

  if (UNLIKELY(!window))
    return v8::Null(isolate);
  // Initializes environment of a frame, and return the global object
  // of the frame.
  Frame* frame = window->frame();
  if (!frame)
    return v8Undefined();

  return frame->windowProxy(DOMWrapperWorld::current(isolate))
      ->globalIfNotDetached();
}

v8::Local<v8::Value> ToV8(EventTarget* impl,
                          v8::Local<v8::Object> creationContext,
                          v8::Isolate* isolate) {
  if (UNLIKELY(!impl))
    return v8::Null(isolate);

  if (impl->interfaceName() == EventTargetNames::DOMWindow)
    return ToV8(static_cast<DOMWindow*>(impl), creationContext, isolate);
  return ToV8(static_cast<ScriptWrappable*>(impl), creationContext, isolate);
}

}  // namespace blink
