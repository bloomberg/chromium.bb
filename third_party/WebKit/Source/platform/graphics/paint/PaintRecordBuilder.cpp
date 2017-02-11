// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintRecordBuilder.h"

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "wtf/PtrUtil.h"

namespace blink {

PaintRecordBuilder::PaintRecordBuilder(const FloatRect& bounds,
                                       SkMetaData* metaData,
                                       GraphicsContext* containingContext,
                                       PaintController* paintController)
    : m_paintController(nullptr), m_bounds(bounds) {
  GraphicsContext::DisabledMode disabledMode = GraphicsContext::NothingDisabled;
  if (containingContext && containingContext->contextDisabled())
    disabledMode = GraphicsContext::FullyDisabled;

  if (paintController) {
    m_paintController = paintController;
  } else {
    m_paintControllerPtr = PaintController::create();
    m_paintController = m_paintControllerPtr.get();

    // Content painted with a new paint controller in SPv2 will have an
    // independent property tree set.
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      PaintChunk::Id id(*this, DisplayItem::kSVGImage);
      PropertyTreeState state(TransformPaintPropertyNode::root(),
                              ClipPaintPropertyNode::root(),
                              EffectPaintPropertyNode::root());
      m_paintController->updateCurrentPaintChunkProperties(&id, state);
    }
  }
#if DCHECK_IS_ON()
  m_paintController->setUsage(PaintController::ForPaintRecordBuilder);
#endif

  m_context = WTF::wrapUnique(
      new GraphicsContext(*m_paintController, disabledMode, metaData));

  if (containingContext) {
    m_context->setDeviceScaleFactor(containingContext->deviceScaleFactor());
    m_context->setPrinting(containingContext->printing());
  }
}

PaintRecordBuilder::~PaintRecordBuilder() {
#if DCHECK_IS_ON()
  m_paintController->setUsage(PaintController::ForNormalUsage);
#endif
}

sk_sp<PaintRecord> PaintRecordBuilder::endRecording() {
  m_context->beginRecording(m_bounds);
  m_paintController->commitNewDisplayItems();
  m_paintController->paintArtifact().replay(*m_context);
  return m_context->endRecording();
}

}  // namespace blink
