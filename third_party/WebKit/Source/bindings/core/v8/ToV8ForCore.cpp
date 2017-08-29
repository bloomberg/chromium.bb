// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ToV8ForCore.h"

#include "bindings/core/v8/WindowProxy.h"
#include "core/dom/events/EventTarget.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/Frame.h"
#include "platform/bindings/RuntimeCallStats.h"

namespace blink {

v8::Local<v8::Value> ToV8(DOMWindow* window,
                          v8::Local<v8::Object> creation_context,
                          v8::Isolate* isolate) {
  RUNTIME_CALL_TIMER_SCOPE(isolate,
                           RuntimeCallStats::CounterId::kToV8DOMWindow);

  // Notice that we explicitly ignore creationContext because the DOMWindow
  // has its own creationContext.

  if (UNLIKELY(!window))
    return v8::Null(isolate);

  // TODO(yukishiino): Get understanding of why it's possible to initialize
  // the context after the frame is detached.  And then, remove the following
  // lines.  See also https://crbug.com/712638 .
  Frame* frame = window->GetFrame();
  if (!frame)
    return v8::Local<v8::Object>();

  // TODO(yukishiino): Make this function always return the non-empty handle
  // even if the frame is detached because the global proxy must always exist
  // per spec.
  return frame->GetWindowProxy(DOMWrapperWorld::Current(isolate))
      ->GlobalProxyIfNotDetached();
}

v8::Local<v8::Value> ToV8(EventTarget* impl,
                          v8::Local<v8::Object> creation_context,
                          v8::Isolate* isolate) {
  if (UNLIKELY(!impl))
    return v8::Null(isolate);

  if (impl->InterfaceName() == EventTargetNames::DOMWindow)
    return ToV8(static_cast<DOMWindow*>(impl), creation_context, isolate);
  return ToV8(static_cast<ScriptWrappable*>(impl), creation_context, isolate);
}

}  // namespace blink
