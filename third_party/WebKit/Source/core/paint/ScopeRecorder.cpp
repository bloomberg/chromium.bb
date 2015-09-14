// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ScopeRecorder.h"

#include "core/layout/LayoutObject.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

ScopeRecorder::ScopeRecorder(GraphicsContext& context)
    : m_displayItemList(context.displayItemList())
{
    ASSERT(m_displayItemList);
    m_displayItemList->beginScope();
}

ScopeRecorder::~ScopeRecorder()
{
    m_displayItemList->endScope();
}

} // namespace blink
