// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef layout_ng_list_marker_h
#define layout_ng_list_marker_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/ng/layout_ng_mixin.h"

namespace blink {

class Document;

// A LayoutObject subclass for list markers in LayoutNG.
class CORE_EXPORT LayoutNGListMarker final
    : public LayoutNGMixin<LayoutBlockFlow> {
 public:
  explicit LayoutNGListMarker(Element*);
  static LayoutNGListMarker* CreateAnonymous(Document*);

  const char* GetName() const override { return "LayoutNGListMarker"; }

 private:
  bool IsOfType(LayoutObjectType) const override;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGListMarker, IsLayoutNGListMarker());

}  // namespace blink

#endif  // layout_ng_list_marker_h
