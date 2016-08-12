// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragment_base.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_text_fragment.h"

namespace blink {

NGFragmentBase::NGFragmentBase(LayoutUnit inlineSize,
                               LayoutUnit blockSize,
                               LayoutUnit inlineOverflow,
                               LayoutUnit blockOverflow,
                               NGWritingMode writingMode,
                               NGDirection direction,
                               NGFragmentType type)
    : m_inlineSize(inlineSize),
      m_blockSize(blockSize),
      m_inlineOverflow(inlineOverflow),
      m_blockOverflow(blockOverflow),
      m_type(type),
      m_writingMode(writingMode),
      m_direction(direction),
      m_hasBeenPlaced(false) {}

void NGFragmentBase::setOffset(LayoutUnit inlineOffset,
                               LayoutUnit blockOffset) {
  // setOffset should only be called once.
  DCHECK(!m_hasBeenPlaced);
  m_inlineOffset = inlineOffset;
  m_blockOffset = blockOffset;
  m_hasBeenPlaced = true;
}

DEFINE_TRACE(NGFragmentBase) {
  if (type() == FragmentText)
    static_cast<NGTextFragment*>(this)->traceAfterDispatch(visitor);
  else
    static_cast<NGFragment*>(this)->traceAfterDispatch(visitor);
}

}  // namespace blink
