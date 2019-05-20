// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_inside_list_marker.h"

#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"

namespace blink {

LayoutNGInsideListMarker::LayoutNGInsideListMarker(Element* element)
    : LayoutInline(element) {}

LayoutNGInsideListMarker* LayoutNGInsideListMarker::CreateAnonymous(
    Document* document) {
  LayoutNGInsideListMarker* object = new LayoutNGInsideListMarker(nullptr);
  object->SetDocumentForAnonymous(document);
  return object;
}

bool LayoutNGInsideListMarker::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGInsideListMarker ||
         LayoutInline::IsOfType(type);
}

PositionWithAffinity LayoutNGInsideListMarker::PositionForPoint(
    const LayoutPoint&) const {
  return CreatePositionWithAffinity(0);
}

}  // namespace blink
