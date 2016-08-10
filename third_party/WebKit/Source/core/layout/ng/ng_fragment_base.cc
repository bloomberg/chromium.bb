// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragment_base.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_text.h"

namespace blink {

NGFragmentBase::NGFragmentBase(LayoutUnit inlineSize,
                               LayoutUnit blockSize,
                               LayoutUnit inlineOverflow,
                               LayoutUnit blockOverflow)
    : m_inlineSize(inlineSize),
      m_blockSize(blockSize),
      m_inlineOverflow(inlineOverflow),
      m_blockOverflow(blockOverflow) {}

void NGFragmentBase::setOffset(LayoutUnit inlineOffset,
                               LayoutUnit blockOffset) {
  m_inlineOffset = inlineOffset;
  m_blockOffset = blockOffset;
}

DEFINE_TRACE(NGFragmentBase) {
  if (m_isText)
    static_cast<NGText*>(this)->traceAfterDispatch(visitor);
  else
    static_cast<NGFragment*>(this)->traceAfterDispatch(visitor);
}

}  // namespace blink
