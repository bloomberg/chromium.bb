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
class LayoutNGListItem;

// A LayoutObject subclass for list markers in LayoutNG.
class CORE_EXPORT LayoutNGListMarker final
    : public LayoutNGMixin<LayoutBlockFlow> {
 public:
  explicit LayoutNGListMarker(Element*);
  static LayoutNGListMarker* CreateAnonymous(Document*);

  // True if the LayoutObject is a list marker wrapper for block content.
  //
  // Because a list marker in LayoutNG is an inline block, and because CSS
  // defines all children of a box must be either inline level or block level,
  // when the content of an list item is block level, the list marker is wrapped
  // in an anonymous block box. This function determines such an anonymous box.
  static bool IsListMarkerWrapperForBlockContent(const LayoutObject&);

  void WillCollectInlines() override;

  bool IsContentImage() const;

  const char* GetName() const override { return "LayoutNGListMarker"; }

 private:
  bool IsOfType(LayoutObjectType) const override;

  LayoutNGListItem* ListItem() const;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGListMarker, IsLayoutNGListMarker());

}  // namespace blink

#endif  // layout_ng_list_marker_h
