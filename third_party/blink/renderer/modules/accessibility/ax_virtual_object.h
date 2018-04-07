// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_VIRTUAL_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_VIRTUAL_OBJECT_H_

#include "third_party/blink/renderer/core/dom/accessible_node.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class AXObjectCacheImpl;

class MODULES_EXPORT AXVirtualObject : public AXObject {
 public:
  AXVirtualObject(AXObjectCacheImpl&, AccessibleNode*);
  ~AXVirtualObject() override;
  virtual void Trace(blink::Visitor*);

  // AXObject overrides.
  void Detach();
  AXObject* ComputeParent() const override { return parent_; }
  bool IsVirtualObject() const override { return true; }
  void AddChildren() override;
  const AtomicString& GetAOMPropertyOrARIAAttribute(
      AOMStringProperty) const override;
  bool HasAOMPropertyOrARIAAttribute(AOMBooleanProperty,
                                     bool& result) const override;
  AccessibleNode* GetAccessibleNode() const override;
  String TextAlternative(bool recursive,
                         bool in_aria_labelled_by_traversal,
                         AXObjectSet& visited,
                         AXNameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;

 private:
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

  Member<AccessibleNode> accessible_node_;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXVirtualObject, IsVirtualObject());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_VIRTUAL_OBJECT_H_
