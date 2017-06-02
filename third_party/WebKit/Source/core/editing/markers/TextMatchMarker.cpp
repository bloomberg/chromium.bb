// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/TextMatchMarker.h"

namespace blink {

TextMatchMarker::TextMatchMarker(unsigned start_offset,
                                 unsigned end_offset,
                                 MatchStatus status)
    : DocumentMarker(start_offset, end_offset), match_status_(status) {}

DocumentMarker::MarkerType TextMatchMarker::GetType() const {
  return DocumentMarker::kTextMatch;
}

bool TextMatchMarker::IsActiveMatch() const {
  return match_status_ == MatchStatus::kActive;
}

void TextMatchMarker::SetIsActiveMatch(bool active) {
  match_status_ = active ? MatchStatus::kActive : MatchStatus::kInactive;
}

bool TextMatchMarker::IsRendered() const {
  return layout_status_ == LayoutStatus::kValidNotNull;
}

bool TextMatchMarker::Contains(const LayoutPoint& point) const {
  DCHECK_EQ(layout_status_, LayoutStatus::kValidNotNull);
  return rendered_rect_.Contains(point);
}

void TextMatchMarker::SetRenderedRect(const LayoutRect& rect) {
  if (layout_status_ == LayoutStatus::kValidNotNull && rect == rendered_rect_)
    return;
  layout_status_ = LayoutStatus::kValidNotNull;
  rendered_rect_ = rect;
}

const LayoutRect& TextMatchMarker::RenderedRect() const {
  DCHECK_EQ(layout_status_, LayoutStatus::kValidNotNull);
  return rendered_rect_;
}

void TextMatchMarker::NullifyRenderedRect() {
  layout_status_ = LayoutStatus::kValidNull;
  // Now |rendered_rect_| can not be accessed until |SetRenderedRect| is
  // called.
}

void TextMatchMarker::Invalidate() {
  layout_status_ = LayoutStatus::kInvalid;
}

bool TextMatchMarker::IsValid() const {
  return layout_status_ != LayoutStatus::kInvalid;
}

}  // namespace blink
