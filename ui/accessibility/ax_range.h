// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_RANGE_H_
#define UI_ACCESSIBILITY_AX_RANGE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

// A range of ax positions.
//
// In order to avoid any confusion regarding whether a deep or a shallow copy is
// being performed, this class can be moved but not copied.
template <class AXPositionType>
class AXRange {
 public:
  AXRange()
      : anchor_(AXPositionType::CreateNullPosition()),
        focus_(AXPositionType::CreateNullPosition()) {}

  AXRange(std::unique_ptr<AXPositionType> anchor,
          std::unique_ptr<AXPositionType> focus) {
    if (anchor) {
      anchor_ = std::move(anchor);
    } else {
      anchor_ = AXPositionType::CreateNullPosition();
    }
    if (focus) {
      focus_ = std::move(focus);
    } else {
      focus = AXPositionType::CreateNullPosition();
    }
  }

  AXRange(const AXRange& other) = delete;

  AXRange(AXRange&& other) : AXRange() {
    anchor_.swap(other.anchor_);
    focus_.swap(other.focus_);
  }

  AXRange& operator=(const AXRange& other) = delete;

  AXRange& operator=(const AXRange&& other) {
    if (this != other) {
      anchor_ = AXPositionType::CreateNullPosition();
      focus_ = AXPositionType::CreateNullPosition();
      anchor_.swap(other.anchor_);
      focus_.swap(other.focus_);
    }
    return *this;
  }

  virtual ~AXRange() {}

  bool IsNull() const {
    return !anchor_ || !focus_ || anchor_->IsNullPosition() ||
           focus_->IsNullPosition();
  }

  AXPositionType* anchor() const {
    DCHECK(anchor_);
    return anchor_.get();
  }

  AXPositionType* focus() const {
    DCHECK(focus_);
    return focus_.get();
  }

  base::string16 GetText() const {
    base::string16 text;
    if (IsNull())
      return text;

    std::unique_ptr<AXPositionType> start, end;
    if (*anchor_ < *focus_) {
      start = anchor_->AsLeafTextPosition();
      end = focus_->AsLeafTextPosition();
    } else {
      start = focus_->AsLeafTextPosition();
      end = anchor_->AsLeafTextPosition();
    }

    int start_offset = start->text_offset();
    DCHECK_GE(start_offset, 0);
    int end_offset = end->text_offset();
    DCHECK_GE(end_offset, 0);

    do {
      text += start->GetInnerText();
      start = start->CreateNextTextAnchorPosition();
    } while (!start->IsNullPosition() && *start <= *end);

    if (static_cast<size_t>(start_offset) > text.length())
      return base::string16();

    text = text.substr(start_offset, base::string16::npos);
    size_t text_length = text.length() - end->GetInnerText().length() +
                         static_cast<size_t>(end_offset);
    return text.substr(0, text_length);
  }

  // Returns a vector of AXRanges that span from the start of an anchor
  // to the end of an anchor, all of which are in between anchor_ and focus_
  // endpoints of this range.
  std::vector<AXRange<AXPositionType>> GetAnchors() {
    DCHECK(*anchor_ <= *focus_);

    std::vector<AXRange<AXPositionType>> anchors;
    auto range_start = anchor_->Clone();
    auto range_end = focus_->Clone();

    // Non-null degenerate ranges span no content, but they do have a single
    // anchor.
    auto current_anchor_start = range_start->AsLeafTextPosition();
    if (!current_anchor_start->IsNullPosition() && *range_start == *range_end) {
      anchors.emplace_back(AXRange(current_anchor_start->Clone(),
                                   current_anchor_start->Clone()));
      return anchors;
    }

    // If start and end are in the same anchor, use end instead of
    // CreatePositionAtEndOfAnchor to ensure this doesn't return a range that
    // spans past end.
    auto current_anchor_end =
        current_anchor_start->CreatePositionAtEndOfAnchor();
    const auto end = range_end->AsLeafTextPosition();
    if (*current_anchor_end > *end &&
        end->GetAnchor() == current_anchor_start->GetAnchor())
      current_anchor_end = end->Clone();

    while (!current_anchor_start->IsNullPosition() &&
           !current_anchor_end->IsNullPosition() &&
           *current_anchor_start < *current_anchor_end) {
      if (current_anchor_start->GetAnchor() ==
          current_anchor_end->GetAnchor()) {
        anchors.emplace_back(AXRange(current_anchor_start->Clone(),
                                     current_anchor_end->Clone()));
      }

      if (*current_anchor_end >= *end)
        break;

      if (current_anchor_end->CreateNextTextAnchorPosition()
              ->IsNullPosition()) {
        current_anchor_start = current_anchor_start->CreateNextAnchorPosition()
                                   ->AsLeafTextPosition();
      } else {
        current_anchor_start =
            current_anchor_end->CreateNextTextAnchorPosition();
      }

      current_anchor_end = current_anchor_start->CreatePositionAtEndOfAnchor();

      // If CreatePositionAtEndOfAnchor goes past the end anchor, use the end
      // anchor instead.
      if (*current_anchor_end > *end &&
          end->GetAnchor() == current_anchor_start->GetAnchor())
        current_anchor_end = end->Clone();
    }

    return anchors;
  }

  // Appends rects in screen coordinates of all text nodes that span between
  // anchor_ and focus_. Rects outside of the viewport are skipped.
  std::vector<gfx::Rect> GetScreenRects() const {
    DCHECK(*anchor_ <= *focus_);
    std::vector<gfx::Rect> rectangles;
    auto current_line_start = anchor_->Clone();
    auto range_end = focus_->Clone();

    while (*current_line_start < *range_end) {
      auto current_line_end = current_line_start->CreateNextLineEndPosition(
          ui::AXBoundaryBehavior::CrossBoundary);

      if (*current_line_end > *range_end)
        current_line_end = range_end->Clone();

      DCHECK(current_line_end->GetAnchor() == current_line_start->GetAnchor());
      int length_of_current_line =
          current_line_end->text_offset() - current_line_start->text_offset();

      if (current_line_start->GetAnchor()->data().role ==
          ax::mojom::Role::kInlineTextBox) {
        current_line_start = current_line_start->CreateParentPosition();
      }

      AXTreeID current_tree_id = current_line_start->tree_id();
      AXTreeManager* manager =
          AXTreeManagerMap::GetInstance().GetManager(current_tree_id);
      AXNode* current_anchor = current_line_start->GetAnchor();
      AXPlatformNodeDelegate* current_anchor_delegate =
          manager->GetDelegate(current_tree_id, current_anchor->id());

      gfx::Rect current_rect = current_anchor_delegate->GetScreenBoundsForRange(
          current_line_start->text_offset(), length_of_current_line,
          /*clipped*/ true);

      // Only add rects that are within the current viewport. The 'clipped'
      // parameter for GetScreenBoundsForRange will return an empty rect in
      // that case.
      if (!current_rect.IsEmpty())
        rectangles.emplace_back(current_rect);

      current_line_start = current_line_end->CreateNextLineStartPosition(
          ui::AXBoundaryBehavior::CrossBoundary);
    }

    return rectangles;
  }

 private:
  std::unique_ptr<AXPositionType> anchor_;
  std::unique_ptr<AXPositionType> focus_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_RANGE_H_
