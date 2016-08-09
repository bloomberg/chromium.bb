// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGText_h
#define NGText_h

#include "core/CoreExport.h"
#include "platform/LayoutUnit.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT NGText final {
public:
    NGText(LayoutUnit inlineSize, LayoutUnit blockSize);
    ~NGText() { }

    LayoutUnit inlineSize() const { return m_inlineSize; }
    LayoutUnit blockSize() const { return m_blockSize; }

    LayoutUnit inlineOffset() const { return m_inlineOffset; }
    LayoutUnit blockOffset() const { return m_blockOffset; }

    const String text() const { return String(); }

    void setOffset(LayoutUnit inlineOffset, LayoutUnit blockOffset);

private:
    LayoutUnit m_inlineSize;
    LayoutUnit m_blockSize;

    LayoutUnit m_inlineOffset;
    LayoutUnit m_blockOffset;
};

} // namespace blink

#endif // NGText_h
