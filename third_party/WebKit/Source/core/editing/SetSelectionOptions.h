// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SetSelectionOptions_h
#define SetSelectionOptions_h

#include "core/CoreExport.h"
#include "core/editing/TextGranularity.h"
#include "platform/wtf/Allocator.h"

namespace blink {

enum class CursorAlignOnScroll { kIfNeeded, kAlways };
enum class SetSelectionBy { kSystem = 0, kUser = 1 };

// This class represents parameters of |FrameSelection::SetSelection()|.
class CORE_EXPORT SetSelectionOptions final {
  STACK_ALLOCATED();

 public:
  class CORE_EXPORT Builder;

  SetSelectionOptions(const SetSelectionOptions&);
  SetSelectionOptions();

  CursorAlignOnScroll GetCursorAlignOnScroll() const {
    return cursor_align_on_scroll_;
  }
  TextGranularity Granularity() const { return granularity_; }
  SetSelectionBy GetSetSelectionBy() const { return set_selection_by_; }
  bool DoNotSetFocus() const { return do_not_set_focus_; }
  bool DoNotClearStrategy() const { return do_not_clear_strategy_; }
  bool ShouldCloseTyping() const { return should_close_typing_; }
  bool ShouldClearTypingStyle() const { return should_clear_typing_style_; }
  bool ShouldShowHandle() const { return should_show_handle_; }

 private:
  CursorAlignOnScroll cursor_align_on_scroll_ = CursorAlignOnScroll::kIfNeeded;
  bool do_not_set_focus_ = false;
  bool do_not_clear_strategy_ = false;
  TextGranularity granularity_ = TextGranularity::kCharacter;
  SetSelectionBy set_selection_by_ = SetSelectionBy::kSystem;
  bool should_close_typing_ = false;
  bool should_clear_typing_style_ = false;
  bool should_show_handle_ = false;
};

// This class is used for building |SelectionData| object.
class CORE_EXPORT SetSelectionOptions::Builder final {
  STACK_ALLOCATED();

 public:
  explicit Builder(const SetSelectionOptions&);
  Builder();

  SetSelectionOptions Build() const;

  Builder& SetCursorAlignOnScroll(CursorAlignOnScroll);
  Builder& SetDoNotSetFocus(bool);
  Builder& SetDoNotClearStrategy(bool);
  Builder& SetGranularity(TextGranularity);
  Builder& SetSetSelectionBy(SetSelectionBy);
  Builder& SetShouldCloseTyping(bool);
  Builder& SetShouldClearTypingStyle(bool);
  Builder& SetShouldShowHandle(bool);

 private:
  SetSelectionOptions data_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

}  // namespace blink

#endif  // SetSelectionOptions_h
