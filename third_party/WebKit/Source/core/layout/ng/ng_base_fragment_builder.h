// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBaseFragmentBuilder_h
#define NGBaseFragmentBuilder_h

#include "base/memory/scoped_refptr.h"
#include "core/CoreExport.h"
#include "core/style/ComputedStyle.h"
#include "platform/text/TextDirection.h"
#include "platform/text/WritingMode.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CORE_EXPORT NGBaseFragmentBuilder {
  STACK_ALLOCATED();
 public:
  virtual ~NGBaseFragmentBuilder();

  const ComputedStyle& Style() const {
    DCHECK(style_);
    return *style_;
  }
  NGBaseFragmentBuilder& SetStyle(scoped_refptr<const ComputedStyle>);

  WritingMode GetWritingMode() const { return writing_mode_; }
  TextDirection Direction() const { return direction_; }

 protected:
  NGBaseFragmentBuilder(scoped_refptr<const ComputedStyle>,
                        WritingMode,
                        TextDirection);
  NGBaseFragmentBuilder(WritingMode, TextDirection);

 private:
  scoped_refptr<const ComputedStyle> style_;
  WritingMode writing_mode_;
  TextDirection direction_;
};

}  // namespace blink

#endif  // NGBaseFragmentBuilder
