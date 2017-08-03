// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/SetSelectionData.h"

namespace blink {

SetSelectionData::SetSelectionData(const SetSelectionData& other) = default;
SetSelectionData::SetSelectionData() = default;
SetSelectionData::Builder::Builder() = default;

SetSelectionData::Builder::Builder(const SetSelectionData& data) {
  data_ = data;
}

SetSelectionData SetSelectionData::Builder::Build() const {
  return data_;
}

SetSelectionData::Builder& SetSelectionData::Builder::SetCursorAlignOnScroll(
    CursorAlignOnScroll align) {
  data_.cursor_align_on_scroll_ = align;
  return *this;
}

SetSelectionData::Builder& SetSelectionData::Builder::SetDoNotSetFocus(
    bool new_value) {
  data_.do_not_set_focus_ = new_value;
  return *this;
}

SetSelectionData::Builder& SetSelectionData::Builder::SetDoNotClearStrategy(
    bool new_value) {
  data_.do_not_clear_strategy_ = new_value;
  return *this;
}

SetSelectionData::Builder& SetSelectionData::Builder::SetGranularity(
    TextGranularity new_value) {
  data_.granularity_ = new_value;
  return *this;
}

SetSelectionData::Builder& SetSelectionData::Builder::SetShouldCloseTyping(
    bool new_value) {
  data_.should_close_typing_ = new_value;
  return *this;
}

SetSelectionData::Builder& SetSelectionData::Builder::SetShouldClearTypingStyle(
    bool new_value) {
  data_.should_clear_typing_style_ = new_value;
  return *this;
}

SetSelectionData::Builder& SetSelectionData::Builder::SetShouldShowHandle(
    bool new_value) {
  data_.should_show_handle_ = new_value;
  return *this;
}

SetSelectionData::Builder& SetSelectionData::Builder::SetSetSelectionBy(
    SetSelectionBy new_value) {
  data_.set_selection_by_ = new_value;
  return *this;
}

}  // namespace blink
