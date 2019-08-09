// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

void NGFragmentItemsBuilder::SetTextContent(const NGInlineNode& node) {
  const NGInlineItemsData& items_data = node.ItemsData(false);
  text_content_ = items_data.text_content;
  const NGInlineItemsData& first_line = node.ItemsData(true);
  if (&items_data != &first_line)
    first_line_text_content_ = first_line.text_content;
}

void NGFragmentItemsBuilder::AddLine(const NGPhysicalLineBoxFragment& line,
                                     ChildList& children) {
  DCHECK_EQ(items_.size(), offsets_.size());
#if DCHECK_IS_ON()
  DCHECK(!is_converted_to_physical_);
#endif

  // Reserve the capacity for (children + line box item).
  wtf_size_t capacity = items_.size() + children.size() + 1;
  items_.ReserveCapacity(capacity);
  offsets_.ReserveCapacity(capacity);

  // Add an empty item so that the start of the line can be set later.
  wtf_size_t line_start_index = items_.size();
  items_.Grow(line_start_index + 1);
  offsets_.Grow(line_start_index + 1);

  AddItems({children.begin(), children.size()});

  // All children are added. Create an item for the start of the line.
  wtf_size_t item_count = items_.size() - line_start_index;
  items_[line_start_index] = std::make_unique<NGFragmentItem>(line, item_count);
  // TODO(kojii): We probably need an end marker too for the reverse-order
  // traversals.
}

void NGFragmentItemsBuilder::AddItems(base::span<Child> children) {
  DCHECK_EQ(items_.size(), offsets_.size());

  for (auto& child : children) {
    if (const NGPhysicalTextFragment* text = child.fragment.get()) {
      DCHECK(text->TextShapeResult());
      DCHECK_EQ(text->StartOffset(), text->TextShapeResult()->StartIndex());
      DCHECK_EQ(text->EndOffset(), text->TextShapeResult()->EndIndex());
      items_.push_back(std::make_unique<NGFragmentItem>(*text));
      offsets_.push_back(child.offset);
      continue;
    }

    if (child.layout_result) {
      // Add an empty item so that the start of the box can be set later.
      wtf_size_t box_start_index = items_.size();
      items_.Grow(box_start_index + 1);
      offsets_.push_back(child.offset);
      // TODO(kojii): Add children and update children_count.
      // All children are added. Create an item for the start of the box.
      wtf_size_t item_count = items_.size() - box_start_index;
      const NGPhysicalBoxFragment& box =
          To<NGPhysicalBoxFragment>(child.layout_result->PhysicalFragment());
      items_[box_start_index] =
          std::make_unique<NGFragmentItem>(box, item_count);
      continue;
    }

    DCHECK(!child.out_of_flow_positioned_box);
  }
}

// Convert internal logical offsets to physical. Items are kept with logical
// offset until outer box size is determined.
void NGFragmentItemsBuilder::ConvertToPhysical(WritingMode writing_mode,
                                               TextDirection direction,
                                               const PhysicalSize& outer_size) {
  CHECK_EQ(items_.size(), offsets_.size());
#if DCHECK_IS_ON()
  DCHECK(!is_converted_to_physical_);
#endif

  const LogicalOffset* offset_iter = offsets_.begin();
  for (auto& item : items_) {
    item->SetOffset(offset_iter->ConvertToPhysical(writing_mode, direction,
                                                   outer_size, item->Size()));
    ++offset_iter;
  }

#if DCHECK_IS_ON()
  is_converted_to_physical_ = true;
#endif
}

void NGFragmentItemsBuilder::ToFragmentItems(WritingMode writing_mode,
                                             TextDirection direction,
                                             const PhysicalSize& outer_size,
                                             void* data) {
  ConvertToPhysical(writing_mode, direction, outer_size);
  new (data) NGFragmentItems(this);
}

}  // namespace blink
