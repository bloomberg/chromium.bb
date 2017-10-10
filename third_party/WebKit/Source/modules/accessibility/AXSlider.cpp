/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "modules/accessibility/AXSlider.h"

#include "core/dom/ShadowRoot.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/layout/LayoutObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

using namespace HTMLNames;

AXSlider::AXSlider(LayoutObject* layout_object,
                   AXObjectCacheImpl& ax_object_cache)
    : AXLayoutObject(layout_object, ax_object_cache) {}

AXSlider* AXSlider::Create(LayoutObject* layout_object,
                           AXObjectCacheImpl& ax_object_cache) {
  return new AXSlider(layout_object, ax_object_cache);
}

AccessibilityRole AXSlider::DetermineAccessibilityRole() {
  if ((aria_role_ = DetermineAriaRoleAttribute()) != kUnknownRole)
    return aria_role_;

  return kSliderRole;
}

AccessibilityOrientation AXSlider::Orientation() const {
  // Default to horizontal in the unknown case.
  if (!layout_object_)
    return kAccessibilityOrientationHorizontal;

  const ComputedStyle* style = layout_object_->Style();
  if (!style)
    return kAccessibilityOrientationHorizontal;

  ControlPart style_appearance = style->Appearance();
  switch (style_appearance) {
    case kSliderThumbHorizontalPart:
    case kSliderHorizontalPart:
    case kMediaSliderPart:
      return kAccessibilityOrientationHorizontal;

    case kSliderThumbVerticalPart:
    case kSliderVerticalPart:
    case kMediaVolumeSliderPart:
      return kAccessibilityOrientationVertical;

    default:
      return kAccessibilityOrientationHorizontal;
  }
}

void AXSlider::AddChildren() {
  DCHECK(!IsDetached());
  DCHECK(!have_children_);

  have_children_ = true;

  AXObjectCacheImpl& cache = AxObjectCache();

  AXSliderThumb* thumb =
      static_cast<AXSliderThumb*>(cache.GetOrCreate(kSliderThumbRole));
  thumb->SetParent(this);

  // Before actually adding the value indicator to the hierarchy,
  // allow the platform to make a final decision about it.
  if (thumb->AccessibilityIsIgnored())
    cache.Remove(thumb->AxObjectID());
  else
    children_.push_back(thumb);
}

AXObject* AXSlider::ElementAccessibilityHitTest(const IntPoint& point) const {
  if (children_.size()) {
    DCHECK(children_.size() == 1);
    if (children_[0]->GetBoundsInFrameCoordinates().Contains(point))
      return children_[0].Get();
  }

  return AxObjectCache().GetOrCreate(layout_object_);
}

bool AXSlider::OnNativeSetValueAction(const String& value) {
  HTMLInputElement* input = GetInputElement();

  if (input->value() == value)
    return false;

  input->setValue(value, kDispatchInputAndChangeEvent);

  // Fire change event manually, as LayoutSlider::setValueForPosition does.
  input->DispatchFormControlChangeEvent();
  return true;
}

HTMLInputElement* AXSlider::GetInputElement() const {
  return ToHTMLInputElement(layout_object_->GetNode());
}

AXSliderThumb::AXSliderThumb(AXObjectCacheImpl& ax_object_cache)
    : AXMockObject(ax_object_cache) {}

AXSliderThumb* AXSliderThumb::Create(AXObjectCacheImpl& ax_object_cache) {
  return new AXSliderThumb(ax_object_cache);
}

LayoutObject* AXSliderThumb::LayoutObjectForRelativeBounds() const {
  if (!parent_)
    return nullptr;

  LayoutObject* slider_layout_object = parent_->GetLayoutObject();
  if (!slider_layout_object || !slider_layout_object->IsSlider())
    return nullptr;
  Element* thumb_element =
      ToElement(slider_layout_object->GetNode())
          ->UserAgentShadowRoot()
          ->getElementById(ShadowElementNames::SliderThumb());
  DCHECK(thumb_element);
  return thumb_element->GetLayoutObject();
}

bool AXSliderThumb::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  return AccessibilityIsIgnoredByDefault(ignored_reasons);
}

}  // namespace blink
