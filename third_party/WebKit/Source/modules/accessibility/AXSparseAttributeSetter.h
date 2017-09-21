// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AXSparseAttributeSetter_h
#define AXSparseAttributeSetter_h

#include "core/dom/AccessibleNode.h"
#include "core/dom/AccessibleNodeList.h"
#include "modules/accessibility/AXObject.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class AXSparseAttributeSetter {
  USING_FAST_MALLOC(AXSparseAttributeSetter);

 public:
  virtual void Run(const AXObject&,
                   AXSparseAttributeClient&,
                   const AtomicString& value) = 0;
};

using AXSparseAttributeSetterMap =
    HashMap<QualifiedName, AXSparseAttributeSetter*>;

// A map from attribute name to a AXSparseAttributeSetter that
// calls AXSparseAttributeClient when that attribute's value
// changes.
//
// That way we only need to iterate over the list of attributes once,
// rather than calling getAttribute() once for each possible obscure
// accessibility attribute.
AXSparseAttributeSetterMap& GetSparseAttributeSetterMap();

// An implementation of AOMPropertyClient that calls
// AXSparseAttributeClient for an AOM property.
class AXSparseAttributeAOMPropertyClient : public AOMPropertyClient {
 public:
  AXSparseAttributeAOMPropertyClient(
      AXObjectCacheImpl& ax_object_cache,
      AXSparseAttributeClient& sparse_attribute_client)
      : ax_object_cache_(ax_object_cache),
        sparse_attribute_client_(sparse_attribute_client) {}

  void AddStringProperty(AOMStringProperty, const String& value) override;
  void AddBooleanProperty(AOMBooleanProperty, bool value) override;
  void AddIntProperty(AOMIntProperty, int32_t value) override;
  void AddUIntProperty(AOMUIntProperty, uint32_t value) override;
  void AddFloatProperty(AOMFloatProperty, float value) override;
  void AddRelationProperty(AOMRelationProperty,
                           const AccessibleNode& value) override;
  void AddRelationListProperty(AOMRelationListProperty,
                               const AccessibleNodeList& relations) override;

 private:
  Persistent<AXObjectCacheImpl> ax_object_cache_;
  AXSparseAttributeClient& sparse_attribute_client_;
};

}  // namespace blink

#endif  // AXSparseAttributeSetter_h
