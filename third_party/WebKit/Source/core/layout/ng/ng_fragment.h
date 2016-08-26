// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragment_h
#define NGFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_fragment_base.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_units.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class CORE_EXPORT NGFragment final : public NGFragmentBase {
 public:
  // This modified the passed-in children vector.
  NGFragment(NGLogicalSize size,
             NGLogicalSize overflow,
             NGWritingMode writingMode,
             NGDirection direction,
             HeapVector<Member<const NGFragmentBase>>& children)
      : NGFragmentBase(size, overflow, writingMode, direction, FragmentBox) {
    children_.swap(children);
  }

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    visitor->trace(children_);
    NGFragmentBase::traceAfterDispatch(visitor);
  }

 private:
  HeapVector<Member<const NGFragmentBase>> children_;
};

}  // namespace blink

#endif  // NGFragment_h
