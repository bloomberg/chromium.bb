// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class FenceEvent;
class LocalDOMWindow;
class ScriptState;

// Fence is a collection of fencedframe-related APIs that are visible
// at `window.fence`, only inside fencedframes.
class CORE_EXPORT Fence final : public ScriptWrappable,
                                public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit Fence(LocalDOMWindow& window);

  // Sends a beacon with the data in `event` to the registered reporting URL.
  void reportEvent(ScriptState* script_state,
                   const FenceEvent* event,
                   ExceptionState& exception_state);

  void Trace(Visitor*) const override;

 private:
  void AddConsoleMessage(const String& message);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCE_API_H_
