/*
 * Copyright (C) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/accessibility/AXInlineTextBox.h"

#include "core/dom/Range.h"
#include "core/layout/LayoutText.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "platform/LayoutUnit.h"

namespace blink {

using namespace HTMLNames;

AXInlineTextBox::AXInlineTextBox(
    PassRefPtr<AbstractInlineTextBox> inline_text_box,
    AXObjectCacheImpl& ax_object_cache)
    : AXObject(ax_object_cache), inline_text_box_(std::move(inline_text_box)) {}

AXInlineTextBox* AXInlineTextBox::Create(
    PassRefPtr<AbstractInlineTextBox> inline_text_box,
    AXObjectCacheImpl& ax_object_cache) {
  return new AXInlineTextBox(std::move(inline_text_box), ax_object_cache);
}

void AXInlineTextBox::Init() {}

void AXInlineTextBox::Detach() {
  AXObject::Detach();
  inline_text_box_ = nullptr;
}

void AXInlineTextBox::GetRelativeBounds(
    AXObject** out_container,
    FloatRect& out_bounds_in_container,
    SkMatrix44& out_container_transform) const {
  *out_container = nullptr;
  out_bounds_in_container = FloatRect();
  out_container_transform.setIdentity();

  if (!inline_text_box_ || !ParentObject() ||
      !ParentObject()->GetLayoutObject())
    return;

  *out_container = ParentObject();
  out_bounds_in_container = FloatRect(inline_text_box_->LocalBounds());

  // Subtract the local bounding box of the parent because they're
  // both in the same coordinate system.
  LayoutObject* parent_layout_object = ParentObject()->GetLayoutObject();
  FloatRect parent_bounding_box =
      parent_layout_object->LocalBoundingBoxRectForAccessibility();
  out_bounds_in_container.MoveBy(-parent_bounding_box.Location());
}

bool AXInlineTextBox::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  AXObject* parent = ParentObject();
  if (!parent)
    return false;

  if (!parent->AccessibilityIsIgnored())
    return false;

  if (ignored_reasons)
    parent->ComputeAccessibilityIsIgnored(ignored_reasons);

  return true;
}

void AXInlineTextBox::TextCharacterOffsets(Vector<int>& offsets) const {
  if (!inline_text_box_)
    return;

  unsigned len = inline_text_box_->Len();
  Vector<float> widths;
  inline_text_box_->CharacterWidths(widths);
  DCHECK_EQ(widths.size(), len);
  offsets.resize(len);

  float width_so_far = 0;
  for (unsigned i = 0; i < len; i++) {
    width_so_far += widths[i];
    offsets[i] = roundf(width_so_far);
  }
}

void AXInlineTextBox::GetWordBoundaries(Vector<AXRange>& words) const {
  if (!inline_text_box_)
    return;

  Vector<AbstractInlineTextBox::WordBoundaries> word_boundaries;
  inline_text_box_->GetWordBoundaries(word_boundaries);
  words.resize(word_boundaries.size());
  for (unsigned i = 0; i < word_boundaries.size(); i++)
    words[i] =
        AXRange(word_boundaries[i].start_index, word_boundaries[i].end_index);
}

String AXInlineTextBox::GetName(AXNameFrom& name_from,
                                AXObject::AXObjectVector* name_objects) const {
  if (!inline_text_box_)
    return String();

  name_from = kAXNameFromContents;
  return inline_text_box_->GetText();
}

AXObject* AXInlineTextBox::ComputeParent() const {
  DCHECK(!IsDetached());
  if (!inline_text_box_ || !ax_object_cache_)
    return 0;

  LineLayoutText line_layout_text = inline_text_box_->GetLineLayoutItem();
  return ax_object_cache_->GetOrCreate(
      LineLayoutAPIShim::LayoutObjectFrom(line_layout_text));
}

// In addition to LTR and RTL direction, edit fields also support
// top to bottom and bottom to top via the CSS writing-mode property.
AccessibilityTextDirection AXInlineTextBox::GetTextDirection() const {
  if (!inline_text_box_)
    return AXObject::GetTextDirection();

  switch (inline_text_box_->GetDirection()) {
    case AbstractInlineTextBox::kLeftToRight:
      return kAccessibilityTextDirectionLTR;
    case AbstractInlineTextBox::kRightToLeft:
      return kAccessibilityTextDirectionRTL;
    case AbstractInlineTextBox::kTopToBottom:
      return kAccessibilityTextDirectionTTB;
    case AbstractInlineTextBox::kBottomToTop:
      return kAccessibilityTextDirectionBTT;
  }

  return AXObject::GetTextDirection();
}

AXObject* AXInlineTextBox::NextOnLine() const {
  RefPtr<AbstractInlineTextBox> next_on_line = inline_text_box_->NextOnLine();
  if (next_on_line)
    return ax_object_cache_->GetOrCreate(next_on_line.Get());

  if (!inline_text_box_->IsLast())
    return 0;

  return ParentObject()->NextOnLine();
}

AXObject* AXInlineTextBox::PreviousOnLine() const {
  RefPtr<AbstractInlineTextBox> previous_on_line =
      inline_text_box_->PreviousOnLine();
  if (previous_on_line)
    return ax_object_cache_->GetOrCreate(previous_on_line.Get());

  if (!inline_text_box_->IsFirst())
    return 0;

  return ParentObject()->PreviousOnLine();
}

}  // namespace blink
