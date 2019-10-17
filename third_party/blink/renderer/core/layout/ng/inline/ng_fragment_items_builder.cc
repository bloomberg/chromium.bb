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

void NGFragmentItemsBuilder::SetCurrentLine(
    const NGPhysicalLineBoxFragment& line,
    ChildList&& children) {
#if DCHECK_IS_ON()
  current_line_fragment_ = &line;
#endif
  current_line_ = std::move(children);
}

void NGFragmentItemsBuilder::AddLine(const NGPhysicalLineBoxFragment& line,
                                     const LogicalOffset& offset) {
  DCHECK_EQ(items_.size(), offsets_.size());
#if DCHECK_IS_ON()
  DCHECK(!is_converted_to_physical_);
  DCHECK_EQ(current_line_fragment_, &line);
#endif

  // Reserve the capacity for (children + line box item).
  wtf_size_t size_before = items_.size();
  wtf_size_t capacity = size_before + current_line_.size() + 1;
  items_.ReserveCapacity(capacity);
  offsets_.ReserveCapacity(capacity);

  // Add an empty item so that the start of the line can be set later.
  wtf_size_t line_start_index = items_.size();
  items_.Grow(line_start_index + 1);
  offsets_.Grow(line_start_index + 1);

  AddItems(current_line_.begin(), current_line_.end());

  // All children are added. Create an item for the start of the line.
  wtf_size_t item_count = items_.size() - line_start_index;
  items_[line_start_index] = std::make_unique<NGFragmentItem>(line, item_count);
  // TODO(kojii): We probably need an end marker too for the reverse-order
  // traversals.

  for (unsigned i = size_before; i < offsets_.size(); ++i)
    offsets_[i] += offset;

  current_line_.clear();
#if DCHECK_IS_ON()
  current_line_fragment_ = nullptr;
#endif
}

void NGFragmentItemsBuilder::AddItems(Child* child_begin, Child* child_end) {
  DCHECK_EQ(items_.size(), offsets_.size());

  for (Child* child_iter = child_begin; child_iter != child_end;) {
    Child& child = *child_iter;
    if (const NGPhysicalTextFragment* text = child.fragment.get()) {
      items_.push_back(std::make_unique<NGFragmentItem>(*text));
      offsets_.push_back(child.offset);
      ++child_iter;
      continue;
    }

    if (child.layout_result) {
      // Create an item if this box has no inline children.
      const NGPhysicalBoxFragment& box =
          To<NGPhysicalBoxFragment>(child.layout_result->PhysicalFragment());
      if (child.children_count <= 1) {
        // Compute |has_floating_descendants_| to optimize tree traversal in
        // paint.
        if (!has_floating_descendants_ && box.IsFloating())
          has_floating_descendants_ = true;

        items_.push_back(std::make_unique<NGFragmentItem>(box, 1));
        offsets_.push_back(child.offset);
        ++child_iter;
        continue;
      }
      DCHECK(!box.IsFloating());

      // Children of inline boxes are flattened and added to |items_|, with the
      // count of descendant items to preserve the tree structure.
      //
      // Add an empty item so that the start of the box can be set later.
      wtf_size_t box_start_index = items_.size();
      items_.Grow(box_start_index + 1);
      offsets_.push_back(child.offset);

      // Add all children, including their desendants, skipping this item.
      CHECK_GE(child.children_count, 1u);  // 0 will loop infinitely.
      Child* end_child_iter = child_iter + child.children_count;
      CHECK_LE(end_child_iter - child_begin, child_end - child_begin);
      AddItems(child_iter + 1, end_child_iter);
      child_iter = end_child_iter;

      // All children are added. Compute how many items are actually added. The
      // number of items added maybe different from |child.children_count|.
      wtf_size_t item_count = items_.size() - box_start_index;

      // Create an item for the start of the box.
      items_[box_start_index] =
          std::make_unique<NGFragmentItem>(box, item_count);
      continue;
    }

    // OOF children should have been added to their parent box fragments.
    // TODO(kojii): Consider handling them in NGFragmentItem too.
    DCHECK(!child.out_of_flow_positioned_box);
    ++child_iter;
  }
}

void NGFragmentItemsBuilder::AddListMarker(
    const NGPhysicalBoxFragment& marker_fragment,
    const LogicalOffset& offset) {
  items_.push_back(std::make_unique<NGFragmentItem>(marker_fragment, 1));
  offsets_.push_back(offset);
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
