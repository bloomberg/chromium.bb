// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutEmbeddedItem_h
#define LayoutEmbeddedItem_h

#include "core/layout/LayoutEmbeddedObject.h"
#include "core/layout/api/LayoutPartItem.h"

namespace blink {

class LayoutEmbeddedItem : public LayoutPartItem {
 public:
  explicit LayoutEmbeddedItem(LayoutEmbeddedObject* layout_embedded_object)
      : LayoutPartItem(layout_embedded_object) {}

  explicit LayoutEmbeddedItem(const LayoutItem& item) : LayoutPartItem(item) {
    SECURITY_DCHECK(!item || item.IsEmbeddedObject());
  }

  explicit LayoutEmbeddedItem(std::nullptr_t) : LayoutPartItem(nullptr) {}

  LayoutEmbeddedItem() {}

  void SetPluginAvailability(
      LayoutEmbeddedObject::PluginAvailability availability) {
    ToEmbeddedObject()->SetPluginAvailability(availability);
  }

  bool ShowsUnavailablePluginIndicator() const {
    return ToEmbeddedObject()->ShowsUnavailablePluginIndicator();
  }

 private:
  LayoutEmbeddedObject* ToEmbeddedObject() {
    return ToLayoutEmbeddedObject(GetLayoutObject());
  }

  const LayoutEmbeddedObject* ToEmbeddedObject() const {
    return ToLayoutEmbeddedObject(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LayoutEmbeddedItem_h
