// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragment.h"

namespace blink {

NGFragment::NGFragment(LayoutUnit inlineSize,
                       LayoutUnit blockSize,
                       LayoutUnit inlineOverflow,
                       LayoutUnit blockOverflow)
    : m_inlineSize(inlineSize),
      m_blockSize(blockSize),
      m_inlineOverflow(inlineOverflow),
      m_blockOverflow(blockOverflow) {}

void NGFragment::setOffset(LayoutUnit inlineOffset, LayoutUnit blockOffset) {
  m_inlineOffset = inlineOffset;
  m_blockOffset = blockOffset;
}

DEFINE_TRACE(NGFragment) {
  visitor->trace(m_children);
}

}  // namespace blink
