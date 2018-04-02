// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef layout_ng_list_marker_image_h
#define layout_ng_list_marker_image_h

#include "core/CoreExport.h"
#include "core/layout/LayoutImage.h"

namespace blink {

class Document;

class CORE_EXPORT LayoutNGListMarkerImage final : public LayoutImage {
 public:
  explicit LayoutNGListMarkerImage(Element*);
  static LayoutNGListMarkerImage* CreateAnonymous(Document*);

 private:
  bool IsOfType(LayoutObjectType) const override;

  void ComputeSVGIntrinsicSizingInfoByDefaultSize(IntrinsicSizingInfo&) const;
  bool GetNestedIntrinsicSizingInfo(IntrinsicSizingInfo&) const final;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGListMarkerImage,
                                IsLayoutNGListMarkerImage());

}  // namespace blink

#endif  // layout_ng_list_marker_image_h
