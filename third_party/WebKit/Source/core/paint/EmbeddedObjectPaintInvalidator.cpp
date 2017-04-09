// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/EmbeddedObjectPaintInvalidator.h"

#include "core/layout/LayoutEmbeddedObject.h"
#include "core/paint/BoxPaintInvalidator.h"
#include "core/plugins/PluginView.h"

namespace blink {

PaintInvalidationReason
EmbeddedObjectPaintInvalidator::InvalidatePaintIfNeeded() {
  PaintInvalidationReason reason =
      BoxPaintInvalidator(embedded_object_, context_).InvalidatePaintIfNeeded();

  PluginView* plugin = embedded_object_.Plugin();
  if (plugin)
    plugin->InvalidatePaintIfNeeded();

  return reason;
}

}  // namespace blink
