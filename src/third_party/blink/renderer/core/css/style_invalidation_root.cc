// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_invalidation_root.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"

namespace blink {

Element* StyleInvalidationRoot::RootElement() const {
  Node* root_node = GetRootNode();
  DCHECK(root_node);
  if (auto* shadow_root = DynamicTo<ShadowRoot>(root_node))
    return &shadow_root->host();
  if (root_node->IsDocumentNode())
    return root_node->GetDocument().documentElement();
  return ToElement(root_node);
}

#if DCHECK_IS_ON()
ContainerNode* StyleInvalidationRoot::Parent(const Node& node) const {
  return node.ParentOrShadowHostNode();
}

bool StyleInvalidationRoot::IsChildDirty(const ContainerNode& node) const {
  return node.ChildNeedsStyleInvalidation();
}
#endif  // DCHECK_IS_ON()

bool StyleInvalidationRoot::IsDirty(const Node& node) const {
  return node.NeedsStyleInvalidation();
}

void StyleInvalidationRoot::ClearChildDirtyForAncestors(
    ContainerNode& parent) const {
  for (Node* ancestor = &parent; ancestor;
       ancestor = ancestor->ParentOrShadowHostNode()) {
    ancestor->ClearChildNeedsStyleInvalidation();
    DCHECK(!ancestor->NeedsStyleInvalidation());
  }
}

}  // namespace blink
