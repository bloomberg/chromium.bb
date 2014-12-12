// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/TransformRecorder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/TransformDisplayItem.h"

namespace blink {

TransformRecorder::TransformRecorder(GraphicsContext& context, DisplayItemClient client, const AffineTransform& transform)
    : m_context(context)
    , m_client(client)
{
    m_skipRecordingForIdentityTransform = transform.isIdentity();

    if (m_skipRecordingForIdentityTransform)
        return;

    OwnPtr<BeginTransformDisplayItem> beginTransform = BeginTransformDisplayItem::create(m_client, transform);

    // We need to apply the display item so the GraphicsContext has the correct matrix pushed (for callers of ctm(), etc.).
    beginTransform->replay(&m_context);

    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context.displayItemList());
        m_context.displayItemList()->add(beginTransform.release());
    }
}

TransformRecorder::~TransformRecorder()
{
    if (m_skipRecordingForIdentityTransform)
        return;

    OwnPtr<EndTransformDisplayItem> endTransform = EndTransformDisplayItem::create(m_client);

    // We need to undo the effect of beginTransform so GraphicsContext has the correct matrix again.
    endTransform->replay(&m_context);

    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context.displayItemList());
        m_context.displayItemList()->add(endTransform.release());
    }
}

} // namespace blink
