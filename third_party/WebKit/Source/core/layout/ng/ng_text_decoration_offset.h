// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ng_text_decoration_offset_h
#define ng_text_decoration_offset_h

#include "core/CoreExport.h"
#include "core/layout/TextDecorationOffsetBase.h"
#include "core/layout/api/LineLayoutItem.h"

namespace blink {

class ComputedStyle;
class NGPhysicalBoxFragment;
class NGPhysicalTextFragment;

// Class for computing the decoration offset for text fragments in LayoutNG.
class CORE_EXPORT NGTextDecorationOffset : public TextDecorationOffsetBase {
  STACK_ALLOCATED();

 public:
  NGTextDecorationOffset(const ComputedStyle& style,
                         const NGPhysicalTextFragment& text_fragment,
                         const NGPhysicalBoxFragment* decorating_box)
      : TextDecorationOffsetBase(style),
        text_fragment_(text_fragment),
        decorating_box_(decorating_box) {}
  ~NGTextDecorationOffset() = default;

  int ComputeUnderlineOffsetForUnder(float text_decoration_thickness,
                                     LineVerticalPositionType) const override;

 private:
  const NGPhysicalTextFragment& text_fragment_;
  const NGPhysicalBoxFragment* decorating_box_;
};

}  // namespace blink

#endif  // ng_text_decoration_offset_h
