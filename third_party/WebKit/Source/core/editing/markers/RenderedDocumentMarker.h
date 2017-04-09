/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#ifndef RenderedDocumentMarker_h
#define RenderedDocumentMarker_h

#include "core/editing/markers/DocumentMarker.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

class RenderedDocumentMarker final : public DocumentMarker {
 private:
  enum class State { kInvalid, kValidNull, kValidNotNull };

 public:
  static RenderedDocumentMarker* Create(const DocumentMarker& marker) {
    return new RenderedDocumentMarker(marker);
  }

  bool IsRendered() const { return state_ == State::kValidNotNull; }
  bool Contains(const LayoutPoint& point) const {
    DCHECK_EQ(state_, State::kValidNotNull);
    return rendered_rect_.Contains(point);
  }
  void SetRenderedRect(const LayoutRect& rect) {
    if (state_ == State::kValidNotNull && rect == rendered_rect_)
      return;
    state_ = State::kValidNotNull;
    rendered_rect_ = rect;
  }

  const LayoutRect& RenderedRect() const {
    DCHECK_EQ(state_, State::kValidNotNull);
    return rendered_rect_;
  }

  void NullifyRenderedRect() {
    state_ = State::kValidNull;
    // Now |m_renderedRect| can not be accessed until |setRenderedRect| is
    // called.
  }

  void Invalidate() { state_ = State::kInvalid; }
  bool IsValid() const { return state_ != State::kInvalid; }

 private:
  explicit RenderedDocumentMarker(const DocumentMarker& marker)
      : DocumentMarker(marker), state_(State::kInvalid) {}

  LayoutRect rendered_rect_;
  State state_;
};

DEFINE_TYPE_CASTS(RenderedDocumentMarker, DocumentMarker, marker, true, true);

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::RenderedDocumentMarker);

#endif
