// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_fragment.h"

#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_unpositioned_float.h"

namespace blink {

NGPhysicalFragment::NGPhysicalFragment(LayoutObject* layout_object,
                                       const ComputedStyle& style,
                                       NGPhysicalSize size,
                                       NGFragmentType type,
                                       RefPtr<NGBreakToken> break_token)
    : layout_object_(layout_object),
      style_(&style),
      size_(size),
      break_token_(std::move(break_token)),
      type_(type),
      is_placed_(false) {}

void NGPhysicalFragment::Destroy() const {
  switch (Type()) {
    case kFragmentBox:
      delete static_cast<const NGPhysicalBoxFragment*>(this);
      break;
    case kFragmentText:
      delete static_cast<const NGPhysicalTextFragment*>(this);
      break;
    case kFragmentLineBox:
      delete static_cast<const NGPhysicalLineBoxFragment*>(this);
      break;
    default:
      NOTREACHED();
      break;
  }
}

const ComputedStyle& NGPhysicalFragment::Style() const {
  DCHECK(style_);
  return *style_;
}

NGPixelSnappedPhysicalBoxStrut NGPhysicalFragment::BorderWidths() const {
  unsigned edges = BorderEdges();
  NGPhysicalBoxStrut box_strut(
      LayoutUnit((edges & NGBorderEdges::kTop) ? Style().BorderTopWidth()
                                               : .0f),
      LayoutUnit((edges & NGBorderEdges::kRight) ? Style().BorderRightWidth()
                                                 : .0f),
      LayoutUnit((edges & NGBorderEdges::kBottom) ? Style().BorderBottomWidth()
                                                  : .0f),
      LayoutUnit((edges & NGBorderEdges::kLeft) ? Style().BorderLeftWidth()
                                                : .0f));
  return box_strut.SnapToDevicePixels();
}

RefPtr<NGPhysicalFragment> NGPhysicalFragment::CloneWithoutOffset() const {
  switch (Type()) {
    case kFragmentBox:
      return static_cast<const NGPhysicalBoxFragment*>(this)
          ->CloneWithoutOffset();
      break;
    case kFragmentText:
      return static_cast<const NGPhysicalTextFragment*>(this)
          ->CloneWithoutOffset();
      break;
    case kFragmentLineBox:
      return static_cast<const NGPhysicalLineBoxFragment*>(this)
          ->CloneWithoutOffset();
      break;
    default:
      NOTREACHED();
      break;
  }
  return nullptr;
}

String NGPhysicalFragment::ToString() const {
  return String::Format("Type: '%d' Size: '%s' Offset: '%s' Placed: '%d'",
                        Type(), Size().ToString().Ascii().data(),
                        Offset().ToString().Ascii().data(), IsPlaced());
}

}  // namespace blink
