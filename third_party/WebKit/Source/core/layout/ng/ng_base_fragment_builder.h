// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBaseFragmentBuilder_h
#define NGBaseFragmentBuilder_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "core/style/ComputedStyle.h"
#include "platform/text/TextDirection.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class CORE_EXPORT NGBaseFragmentBuilder {
  STACK_ALLOCATED();
 public:
  virtual ~NGBaseFragmentBuilder();

  const ComputedStyle& Style() const {
    DCHECK(style_);
    return *style_;
  }
  NGBaseFragmentBuilder& SetStyle(RefPtr<const ComputedStyle>);

  NGWritingMode WritingMode() const { return writing_mode_; }
  TextDirection Direction() const { return direction_; }

 protected:
  NGBaseFragmentBuilder(RefPtr<const ComputedStyle>,
                        NGWritingMode,
                        TextDirection);
  NGBaseFragmentBuilder(NGWritingMode, TextDirection);

 private:
  RefPtr<const ComputedStyle> style_;
  NGWritingMode writing_mode_;
  TextDirection direction_;
};

}  // namespace blink

#endif  // NGBaseFragmentBuilder
