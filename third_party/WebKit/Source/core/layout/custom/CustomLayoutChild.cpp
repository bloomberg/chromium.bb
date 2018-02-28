// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/custom/CustomLayoutChild.h"

#include "core/css/cssom/PrepopulatedComputedStylePropertyMap.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/custom/CSSLayoutDefinition.h"

namespace blink {

CustomLayoutChild::CustomLayoutChild(const CSSLayoutDefinition& definition,
                                     LayoutBox* box)
    : box_(box),
      style_map_(new PrepopulatedComputedStylePropertyMap(
          box->GetDocument(),
          box->StyleRef(),
          box->GetNode(),
          definition.ChildNativeInvalidationProperties(),
          definition.ChildCustomInvalidationProperties())) {}

void CustomLayoutChild::Trace(blink::Visitor* visitor) {
  visitor->Trace(style_map_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
