// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlockFlowPaintInvalidator_h
#define BlockFlowPaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "wtf/Allocator.h"

namespace blink {

class LayoutBlockFlow;
struct PaintInvalidatorContext;

class BlockFlowPaintInvalidator {
    STACK_ALLOCATED();
public:
    BlockFlowPaintInvalidator(const LayoutBlockFlow& blockFlow, const PaintInvalidatorContext& context)
        : m_blockFlow(blockFlow), m_context(context) { }

    PaintInvalidationReason invalidatePaintIfNeeded();

private:
    const LayoutBlockFlow& m_blockFlow;
    const PaintInvalidatorContext& m_context;
};

} // namespace blink

#endif
