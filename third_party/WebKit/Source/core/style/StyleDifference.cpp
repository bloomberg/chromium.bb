// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StyleDifference.h"

namespace blink {

std::ostream& operator<<(std::ostream& out, const StyleDifference& diff) {
  out << "StyleDifference{layoutType=";

  switch (diff.m_layoutType) {
    case StyleDifference::NoLayout:
      out << "NoLayout";
      break;
    case StyleDifference::PositionedMovement:
      out << "PositionedMovement";
      break;
    case StyleDifference::FullLayout:
      out << "FullLayout";
      break;
    default:
      NOTREACHED();
      break;
  }

  out << ", paintInvalidationType=";
  switch (diff.m_paintInvalidationType) {
    case StyleDifference::NoPaintInvalidation:
      out << "NoPaintInvalidation";
      break;
    case StyleDifference::PaintInvalidationObject:
      out << "PaintInvalidationObject";
      break;
    case StyleDifference::PaintInvalidationSubtree:
      out << "PaintInvalidationSubtree";
      break;
    default:
      NOTREACHED();
      break;
  }

  out << ", recomputeOverflow=" << diff.m_recomputeOverflow;
  out << ", visualRectUpdate=" << diff.m_visualRectUpdate;

  out << ", propertySpecificDifferences=";
  int diffCount = 0;
  for (int i = 0; i < StyleDifference::kPropertyDifferenceCount; i++) {
    unsigned bitTest = 1 << i;
    if (diff.m_propertySpecificDifferences & bitTest) {
      if (diffCount++ > 0)
        out << "|";
      switch (bitTest) {
        case StyleDifference::TransformChanged:
          out << "TransformChanged";
          break;
        case StyleDifference::OpacityChanged:
          out << "OpacityChanged";
          break;
        case StyleDifference::ZIndexChanged:
          out << "ZIndexChanged";
          break;
        case StyleDifference::FilterChanged:
          out << "FilterChanged";
          break;
        case StyleDifference::BackdropFilterChanged:
          out << "BackdropFilterChanged";
          break;
        case StyleDifference::CSSClipChanged:
          out << "CSSClipChanged";
          break;
        case StyleDifference::TextDecorationOrColorChanged:
          out << "TextDecorationOrColorChanged";
          break;
        default:
          NOTREACHED();
          break;
      }
    }
  }

  out << ", scrollAnchorDisablingPropertyChanged="
      << diff.m_scrollAnchorDisablingPropertyChanged;

  return out << "}";
}

}  // namespace blink
