// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/accessibility/AXVirtualObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

AXVirtualObject::AXVirtualObject(AXObjectCacheImpl& axObjectCache,
                                 AccessibleNode* accessible_node)
    : AXObject(axObjectCache), accessible_node_(accessible_node) {
  LOG(ERROR) << "AXVirtualObject ctor";
}

AXVirtualObject::~AXVirtualObject() {}

void AXVirtualObject::Detach() {
  AXObject::Detach();

  accessible_node_ = nullptr;
}

bool AXVirtualObject::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignoredReasons) const {
  return AccessibilityIsIgnoredByDefault(ignoredReasons);
}

void AXVirtualObject::AddChildren() {
  if (!accessible_node_)
    return;

  for (const auto& child : accessible_node_->GetChildren())
    children_.push_back(AxObjectCache().GetOrCreate(child));
}

const AtomicString& AXVirtualObject::GetAOMPropertyOrARIAAttribute(
    AOMStringProperty property) const {
  if (!accessible_node_)
    return g_null_atom;

  return accessible_node_->GetProperty(property);
}

DEFINE_TRACE(AXVirtualObject) {
  visitor->Trace(accessible_node_);
  AXObject::Trace(visitor);
}

}  // namespace blink
