// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragment_h
#define NGFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_fragment_base.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class CORE_EXPORT NGFragment final : public NGFragmentBase {
 public:
  NGFragment(LayoutUnit inlineSize,
             LayoutUnit blockSize,
             LayoutUnit inlineOverflow,
             LayoutUnit blockOverflow,
             NGWritingMode writingMode,
             NGDirection direction)
      : NGFragmentBase(inlineSize,
                       blockSize,
                       inlineOverflow,
                       blockOverflow,
                       writingMode,
                       direction,
                       FragmentBox) {}

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    visitor->trace(m_children);
    NGFragmentBase::traceAfterDispatch(visitor);
  }

  void swapChildren(HeapVector<Member<const NGFragmentBase>>& children) {
    m_children.swap(children);
  }

 private:
  HeapVector<Member<const NGFragmentBase>> m_children;
};

}  // namespace blink

#endif  // NGFragment_h
