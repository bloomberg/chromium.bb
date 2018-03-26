// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/custom/CustomLayoutChild.h"

#include "core/css/cssom/PrepopulatedComputedStylePropertyMap.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/custom/CSSLayoutDefinition.h"
#include "core/layout/custom/CustomLayoutFragmentRequest.h"

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

CustomLayoutFragmentRequest* CustomLayoutChild::layoutNextFragment(
    const CustomLayoutConstraintsOptions& options) {
  return new CustomLayoutFragmentRequest(this, options);
}

void CustomLayoutChild::Trace(blink::Visitor* visitor) {
  visitor->Trace(style_map_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
