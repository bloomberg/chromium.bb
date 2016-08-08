// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/NGFragment.h"

namespace blink {

NGFragment::NGFragment(LayoutUnit inlineSize, LayoutUnit blockSize)
    : m_inlineSize(inlineSize)
    , m_blockSize(blockSize)
{
}

void NGFragment::setOffset(LayoutUnit inlineOffset, LayoutUnit blockOffset)
{
    m_inlineOffset = inlineOffset;
    m_blockOffset = blockOffset;
}

} // namespace blink
