// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragmentBase_h
#define NGFragmentBase_h

#include "core/CoreExport.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class CORE_EXPORT NGFragmentBase : public GarbageCollected<NGFragmentBase> {
 public:
  // Returns the border-box size.
  LayoutUnit inlineSize() const { return m_inlineSize; }
  LayoutUnit blockSize() const { return m_blockSize; }

  // Returns the total size, including the contents outside of the border-box.
  LayoutUnit inlineOverflow() const { return m_inlineOverflow; }
  LayoutUnit blockOverflow() const { return m_blockOverflow; }

  // Returns the offset relative to the parent fragement's content-box.
  LayoutUnit inlineOffset() const { return m_inlineOffset; }
  LayoutUnit blockOffset() const { return m_blockOffset; }

  // Should only be used by the parent fragement's layout.
  void setOffset(LayoutUnit inlineOffset, LayoutUnit blockOffset);

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {}
  DECLARE_TRACE();

 protected:
  NGFragmentBase(LayoutUnit inlineSize,
                 LayoutUnit blockSize,
                 LayoutUnit inlineOverflow,
                 LayoutUnit blockOverflow);

  LayoutUnit m_inlineSize;
  LayoutUnit m_blockSize;
  LayoutUnit m_inlineOverflow;
  LayoutUnit m_blockOverflow;
  LayoutUnit m_inlineOffset;
  LayoutUnit m_blockOffset;
  bool m_isText;
};

}  // namespace blink

#endif  // NGFragmentBase_h
