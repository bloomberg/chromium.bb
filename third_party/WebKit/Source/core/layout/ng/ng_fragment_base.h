// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragmentBase_h
#define NGFragmentBase_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class CORE_EXPORT NGFragmentBase : public GarbageCollected<NGFragmentBase> {
 public:
  enum NGFragmentType { FragmentBox = 0, FragmentText = 1 };

  NGFragmentType Type() const { return static_cast<NGFragmentType>(type_); }
  NGWritingMode WritingMode() const {
    return static_cast<NGWritingMode>(writing_mode_);
  }
  NGDirection Direction() const { return static_cast<NGDirection>(direction_); }

  // Returns the border-box size.
  LayoutUnit InlineSize() const { return size_.inline_size; }
  LayoutUnit BlockSize() const { return size_.block_size; }

  // Returns the total size, including the contents outside of the border-box.
  LayoutUnit InlineOverflow() const { return overflow_.inline_size; }
  LayoutUnit BlockOverflow() const { return overflow_.block_size; }

  // Returns the offset relative to the parent fragement's content-box.
  LayoutUnit InlineOffset() const { return offset_.inline_offset; }
  LayoutUnit BlockOffset() const { return offset_.block_offset; }

  // Should only be used by the parent fragement's layout.
  void SetOffset(LayoutUnit inline_offset, LayoutUnit block_offset);

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {}
  DECLARE_TRACE();

 protected:
  NGFragmentBase(NGLogicalSize size,
                 NGLogicalSize overflow,
                 NGWritingMode,
                 NGDirection,
                 NGFragmentType);

  NGLogicalSize size_;
  NGLogicalSize overflow_;
  NGLogicalOffset offset_;

  unsigned type_ : 1;
  unsigned writing_mode_ : 3;
  unsigned direction_ : 1;
  unsigned has_been_placed_ : 1;
};

}  // namespace blink

#endif  // NGFragmentBase_h
