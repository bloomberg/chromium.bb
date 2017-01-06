// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPaintProperties.h"

namespace blink {

PropertyTreeState ObjectPaintProperties::contentsProperties() const {
  PropertyTreeState properties = *localBorderBoxProperties();

  if (scrollTranslation())
    properties.setTransform(scrollTranslation());

  if (scroll())
    properties.setScroll(scroll());

  if (overflowClip())
    properties.setClip(overflowClip());
  else if (cssClip())
    properties.setClip(cssClip());

  // TODO(chrishtr): cssClipFixedPosition needs to be handled somehow.

  return properties;
}

}  // namespace blink
