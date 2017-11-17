// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/layout_ng_table_cell.h"

#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_length_utils.h"

namespace blink {

LayoutNGTableCell::LayoutNGTableCell(Element* element)
    : LayoutNGMixin<LayoutTableCell>(element) {}

void LayoutNGTableCell::UpdateBlockLayout(bool relayout_children) {
  LayoutAnalyzer::BlockScope analyzer(*this);

  SetOverrideLogicalContentWidth(
      (LogicalWidth() - BorderAndPaddingLogicalWidth()).ClampNegativeToZero());

  scoped_refptr<NGConstraintSpace> constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);

  scoped_refptr<NGLayoutResult> result =
      NGBlockNode(this).Layout(*constraint_space);

  for (NGOutOfFlowPositionedDescendant descendant :
       result->OutOfFlowPositionedDescendants())
    descendant.node.UseOldOutOfFlowPositioning();

  NGPhysicalBoxFragment* fragment =
      ToNGPhysicalBoxFragment(result->PhysicalFragment().get());

  const LayoutBox* section = LocationContainer();
  NGPhysicalOffset physical_offset;
  if (section) {
    NGPhysicalSize section_size(section->Size().Width(),
                                section->Size().Height());
    NGLogicalOffset logical_offset(LogicalLeft() + section->Location().X(),
                                   LogicalTop() + section->Location().Y());
    physical_offset = logical_offset.ConvertToPhysical(
        constraint_space->GetWritingMode(), constraint_space->Direction(),
        section_size, fragment->Size());
  }
  fragment->SetOffset(physical_offset);
}

}  // namespace blink
