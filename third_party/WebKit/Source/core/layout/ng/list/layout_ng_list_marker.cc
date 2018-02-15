// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/list/layout_ng_list_marker.h"

namespace blink {

LayoutNGListMarker::LayoutNGListMarker(Element* element)
    : LayoutNGMixin<LayoutBlockFlow>(element) {}

LayoutNGListMarker* LayoutNGListMarker::CreateAnonymous(Document* document) {
  LayoutNGListMarker* object = new LayoutNGListMarker(nullptr);
  object->SetDocumentForAnonymous(document);
  return object;
}

bool LayoutNGListMarker::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGListMarker ||
         LayoutNGMixin<LayoutBlockFlow>::IsOfType(type);
}

}  // namespace blink
