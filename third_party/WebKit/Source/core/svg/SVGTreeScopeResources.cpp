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

void SVGTreeScopeResources::RemoveUnreferencedResources() {
  if (resources_.IsEmpty())
    return;
  // Remove resources that are no longer referenced.
  Vector<AtomicString> to_be_removed;
  for (const auto& entry : resources_) {
    LocalSVGResource* resource = entry.value.Get();
    DCHECK(resource);
    if (resource->IsEmpty()) {
      resource->Unregister();
      to_be_removed.push_back(entry.key);
    }
  }
  resources_.RemoveAll(to_be_removed);
}

void SVGTreeScopeResources::RemoveWatchesForElement(SVGElement& element) {
  if (resources_.IsEmpty() || !element.HasPendingResources())
    return;
  // Remove the element from pending resources.
  Vector<AtomicString> to_be_removed;
  for (const auto& entry : resources_) {
    LocalSVGResource* resource = entry.value.Get();
    DCHECK(resource);
    resource->RemoveWatch(element);
    if (resource->IsEmpty()) {
      resource->Unregister();
      to_be_removed.push_back(entry.key);
    }
  }
  resources_.RemoveAll(to_be_removed);

  element.ClearHasPendingResources();
}

void SVGTreeScopeResources::Trace(blink::Visitor* visitor) {
  visitor->Trace(resources_);
  visitor->Trace(tree_scope_);
}

}  // namespace blink
