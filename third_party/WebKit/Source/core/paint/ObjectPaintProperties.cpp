// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPaintProperties.h"

namespace blink {

ObjectPaintProperties::PropertyTreeStateWithOffset
ObjectPaintProperties::contentsProperties() const {
  ObjectPaintProperties::PropertyTreeStateWithOffset propertiesWithOffset =
      *localBorderBoxProperties();

  if (scrollTranslation())
    propertiesWithOffset.propertyTreeState.setTransform(scrollTranslation());

  if (scroll())
    propertiesWithOffset.propertyTreeState.setScroll(scroll());

  if (overflowClip())
    propertiesWithOffset.propertyTreeState.setClip(overflowClip());
  else if (cssClip())
    propertiesWithOffset.propertyTreeState.setClip(cssClip());

  // TODO(chrishtr): cssClipFixedPosition needs to be handled somehow.

  return propertiesWithOffset;
}

}  // namespace blink
