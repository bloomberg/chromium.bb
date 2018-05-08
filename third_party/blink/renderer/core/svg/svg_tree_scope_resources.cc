// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_tree_scope_resources.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

SVGTreeScopeResources::SVGTreeScopeResources(TreeScope* tree_scope)
    : tree_scope_(tree_scope) {}

SVGTreeScopeResources::~SVGTreeScopeResources() = default;

LocalSVGResource* SVGTreeScopeResources::ResourceForId(const AtomicString& id) {
  if (id.IsEmpty())
    return nullptr;
  auto& entry = resources_.insert(id, nullptr).stored_value->value;
  if (!entry)
    entry = new LocalSVGResource(*tree_scope_, id);
  return entry;
}

LocalSVGResource* SVGTreeScopeResources::ExistingResourceForId(
    const AtomicString& id) const {
  if (id.IsEmpty())
    return nullptr;
  return resources_.at(id);
}

void SVGTreeScopeResources::UnregisterResource(LocalSVGResource* resource) {
  for (const auto& entry : resources_) {
    if (resource == entry.value) {
      resources_.erase(entry.key);
      return;
    }
  }
}

void SVGTreeScopeResources::Trace(blink::Visitor* visitor) {
  visitor->Trace(resources_);
  visitor->Trace(tree_scope_);
}

}  // namespace blink
