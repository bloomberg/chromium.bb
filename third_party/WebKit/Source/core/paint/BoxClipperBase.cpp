// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxClipperBase.h"

#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

DISABLE_CFI_PERF
void BoxClipperBase::InitializeScopedClipProperty(
    const FragmentData* fragment,
    const DisplayItemClient& client,
    const PaintInfo& paint_info) {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV175Enabled());

  if (!fragment)
    return;
  const auto* properties = fragment->PaintProperties();
  if (!properties || !properties->OverflowClip())
    return;

  scoped_clip_property_.emplace(paint_info.context.GetPaintController(),
                                properties->OverflowClip(), client,
                                paint_info.DisplayItemTypeForClipping());
}

}  // namespace blink
