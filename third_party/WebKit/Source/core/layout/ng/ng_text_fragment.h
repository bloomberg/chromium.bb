// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGTextFragment_h
#define NGTextFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_fragment_base.h"
#include "core/layout/ng/ng_layout_input_text.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT NGTextFragment final : public NGFragmentBase {
 public:
  NGTextFragment(NGLogicalSize size,
                 NGLogicalSize overflow,
                 NGWritingMode writingMode,
                 NGDirection direction)
      : NGFragmentBase(size, overflow, writingMode, direction, FragmentText) {}

  String text() const;

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    visitor->trace(text_list_);
    NGFragmentBase::traceAfterDispatch(visitor);
  }

 private:
  Member<NGLayoutInputText> text_list_;
  unsigned start_offset_;
  unsigned end_offset_;
};

}  // namespace blink

#endif  // NGTextFragment_h
