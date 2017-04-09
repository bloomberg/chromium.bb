/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GraphicsLayerDebugInfo_h
#define GraphicsLayerDebugInfo_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/CompositingReasons.h"
#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/graphics/SquashingDisallowedReasons.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"

#include <memory>

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace blink {

class GraphicsLayerDebugInfo final {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(GraphicsLayerDebugInfo);

 public:
  GraphicsLayerDebugInfo();
  ~GraphicsLayerDebugInfo();

  std::unique_ptr<base::trace_event::TracedValue> AsTracedValue() const;

  CompositingReasons GetCompositingReasons() const {
    return compositing_reasons_;
  }
  void SetCompositingReasons(CompositingReasons reasons) {
    compositing_reasons_ = reasons;
  }

  SquashingDisallowedReasons GetSquashingDisallowedReasons() const {
    return squashing_disallowed_reasons_;
  }
  void SetSquashingDisallowedReasons(SquashingDisallowedReasons reasons) {
    squashing_disallowed_reasons_ = reasons;
  }
  void SetOwnerNodeId(int id) { owner_node_id_ = id; }

  void AppendAnnotatedInvalidateRect(const FloatRect&, PaintInvalidationReason);
  void ClearAnnotatedInvalidateRects();

  uint32_t GetMainThreadScrollingReasons() const {
    return main_thread_scrolling_reasons_;
  }
  void SetMainThreadScrollingReasons(uint32_t reasons) {
    main_thread_scrolling_reasons_ = reasons;
  }

 private:
  void AppendAnnotatedInvalidateRects(base::trace_event::TracedValue*) const;
  void AppendCompositingReasons(base::trace_event::TracedValue*) const;
  void AppendSquashingDisallowedReasons(base::trace_event::TracedValue*) const;
  void AppendOwnerNodeId(base::trace_event::TracedValue*) const;
  void AppendMainThreadScrollingReasons(base::trace_event::TracedValue*) const;

  struct AnnotatedInvalidationRect {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    FloatRect rect;
    PaintInvalidationReason reason;
  };

  CompositingReasons compositing_reasons_;
  SquashingDisallowedReasons squashing_disallowed_reasons_;
  int owner_node_id_;
  Vector<AnnotatedInvalidationRect> invalidations_;
  Vector<AnnotatedInvalidationRect> previous_invalidations_;
  uint32_t main_thread_scrolling_reasons_;
};

}  // namespace blink

#endif
