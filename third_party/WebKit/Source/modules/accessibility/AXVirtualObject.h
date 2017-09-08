// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AXVirtualObject_h
#define AXVirtualObject_h

#include "core/dom/AccessibleNode.h"
#include "modules/ModulesExport.h"
#include "modules/accessibility/AXObject.h"

namespace blink {

class AXObjectCacheImpl;

class MODULES_EXPORT AXVirtualObject : public AXObject {
 public:
  AXVirtualObject(AXObjectCacheImpl&, AccessibleNode*);
  ~AXVirtualObject() override;
  DECLARE_VIRTUAL_TRACE();

  // AXObject overrides.
  void Detach();
  AXObject* ComputeParent() const override { return parent_; }
  bool IsVirtualObject() const override { return true; }
  void AddChildren() override;
  const AtomicString& GetAOMPropertyOrARIAAttribute(
      AOMStringProperty) const override;

 private:
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

  Member<AccessibleNode> accessible_node_;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXVirtualObject, IsVirtualObject());

}  // namespace blink

#endif  // AXVirtualObject_h
