// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragment_h
#define NGFragment_h

#include "core/CoreExport.h"
#include "platform/LayoutUnit.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class CORE_EXPORT NGFragment final : public GarbageCollected<NGFragment> {
public:
    NGFragment(LayoutUnit inlineSize, LayoutUnit blockSize,
        LayoutUnit inlineOverflow, LayoutUnit blockOverflow);

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

    DECLARE_TRACE();

private:
    LayoutUnit m_inlineSize;
    LayoutUnit m_blockSize;

    LayoutUnit m_inlineOverflow;
    LayoutUnit m_blockOverflow;

    LayoutUnit m_inlineOffset;
    LayoutUnit m_blockOffset;

    HeapVector<Member<const NGFragment>> m_children;
};

} // namespace blink

#endif // NGFragment_h
