// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPaintProperties.h"

namespace blink {

std::unique_ptr<PropertyTreeState> ObjectPaintProperties::contentsProperties(
    PropertyTreeState* localBorderBoxProperties,
    ObjectPaintProperties* paintProperties) {
  if (!localBorderBoxProperties)
    return nullptr;

  std::unique_ptr<PropertyTreeState> contents =
      WTF::makeUnique<PropertyTreeState>(*localBorderBoxProperties);

  if (paintProperties) {
    if (paintProperties->scrollTranslation())
      contents->setTransform(paintProperties->scrollTranslation());
    if (paintProperties->overflowClip())
      contents->setClip(paintProperties->overflowClip());
    else if (paintProperties->cssClip())
      contents->setClip(paintProperties->cssClip());
  }

  // TODO(chrishtr): cssClipFixedPosition needs to be handled somehow.

  return contents;
}

}  // namespace blink
