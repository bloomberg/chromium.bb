// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFragment_h
#define NGFragment_h

#include "core/CoreExport.h"
#include "platform/LayoutUnit.h"

namespace blink {

class CORE_EXPORT NGFragment final {
public:
    NGFragment(LayoutUnit inlineSize, LayoutUnit blockSize);
    ~NGFragment() { }

    LayoutUnit inlineSize() const { return m_inlineSize; }
    LayoutUnit blockSize() const { return m_blockSize; }

    LayoutUnit inlineOffset() const { return m_inlineOffset; }
    LayoutUnit blockOffset() const { return m_blockOffset; }

    void setOffset(LayoutUnit inlineOffset, LayoutUnit blockOffset);

private:
    LayoutUnit m_inlineSize;
    LayoutUnit m_blockSize;

    LayoutUnit m_inlineOffset;
    LayoutUnit m_blockOffset;
};

} // namespace blink

#endif // NGFragment_h
