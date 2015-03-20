// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SubtreeRecorder.h"

#include "core/layout/LayoutObject.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/SubtreeDisplayItem.h"

namespace blink {

SubtreeRecorder::SubtreeRecorder(GraphicsContext* context, const LayoutObject& subtreeRoot, PaintPhase paintPhase)
    : m_displayItemList(context->displayItemList())
    , m_subtreeRoot(subtreeRoot)
    , m_paintPhase(paintPhase)
    , m_begun(false)
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

    ASSERT(m_displayItemList);
}

SubtreeRecorder::~SubtreeRecorder()
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

    if (m_begun)
        addDisplayItem(EndSubtreeDisplayItem::create(m_subtreeRoot.displayItemClient(), DisplayItem::paintPhaseToEndSubtreeType(m_paintPhase)));
}

void SubtreeRecorder::begin()
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

    addDisplayItem(BeginSubtreeDisplayItem::create(m_subtreeRoot.displayItemClient(), DisplayItem::paintPhaseToBeginSubtreeType(m_paintPhase)));
    m_begun = true;
}

void SubtreeRecorder::addDisplayItem(PassOwnPtr<DisplayItem> displayItem)
{
#ifndef NDEBUG
    displayItem->setClientDebugString(String::format("subtreeRoot: \"%p %s\"", &m_subtreeRoot,
        m_subtreeRoot.debugName().utf8().data()));
#endif
    m_displayItemList->add(displayItem);
}

} // namespace blink
