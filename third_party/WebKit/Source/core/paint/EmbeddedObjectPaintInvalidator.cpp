// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/EmbeddedObjectPaintInvalidator.h"

#include "core/layout/LayoutEmbeddedObject.h"
#include "core/paint/BoxPaintInvalidator.h"
#include "core/plugins/PluginView.h"

namespace blink {

PaintInvalidationReason
EmbeddedObjectPaintInvalidator::invalidatePaintIfNeeded() {
  PaintInvalidationReason reason =
      BoxPaintInvalidator(m_embeddedObject, m_context)
          .invalidatePaintIfNeeded();

  FrameViewBase* frameViewBase = m_embeddedObject.widget();
  if (frameViewBase && frameViewBase->isPluginView())
    toPluginView(frameViewBase)->invalidatePaintIfNeeded();

  return reason;
}

}  // namespace blink
