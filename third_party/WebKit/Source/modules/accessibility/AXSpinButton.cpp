/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "modules/accessibility/AXSpinButton.h"

#include "SkMatrix44.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

AXSpinButton* AXSpinButton::Create(AXObjectCacheImpl& ax_object_cache) {
  return new AXSpinButton(ax_object_cache);
}

AXSpinButton::AXSpinButton(AXObjectCacheImpl& ax_object_cache)
    : AXMockObject(ax_object_cache), spin_button_element_(nullptr) {}

AXSpinButton::~AXSpinButton() {
  DCHECK(!spin_button_element_);
}

DEFINE_TRACE(AXSpinButton) {
  visitor->Trace(spin_button_element_);
  AXMockObject::Trace(visitor);
}

LayoutObject* AXSpinButton::LayoutObjectForRelativeBounds() const {
  if (!spin_button_element_ || !spin_button_element_->GetLayoutObject())
    return nullptr;

  return spin_button_element_->GetLayoutObject();
}

void AXSpinButton::Detach() {
  AXObject::Detach();
  spin_button_element_ = nullptr;
}

void AXSpinButton::DetachFromParent() {
  AXObject::DetachFromParent();
  spin_button_element_ = nullptr;
}

AccessibilityRole AXSpinButton::RoleValue() const {
  return spin_button_element_ ? kSpinButtonRole : kUnknownRole;
}

void AXSpinButton::AddChildren() {
  DCHECK(!IsDetached());
  have_children_ = true;

  AXSpinButtonPart* incrementor =
      ToAXSpinButtonPart(AxObjectCache().GetOrCreate(kSpinButtonPartRole));
  incrementor->SetIsIncrementor(true);
  incrementor->SetParent(this);
  children_.push_back(incrementor);

  AXSpinButtonPart* decrementor =
      ToAXSpinButtonPart(AxObjectCache().GetOrCreate(kSpinButtonPartRole));
  decrementor->SetIsIncrementor(false);
  decrementor->SetParent(this);
  children_.push_back(decrementor);
}

void AXSpinButton::Step(int amount) {
  DCHECK(spin_button_element_);
  if (!spin_button_element_)
    return;

  spin_button_element_->Step(amount);
}

// AXSpinButtonPart

AXSpinButtonPart::AXSpinButtonPart(AXObjectCacheImpl& ax_object_cache)
    : AXMockObject(ax_object_cache), is_incrementor_(false) {}

AXSpinButtonPart* AXSpinButtonPart::Create(AXObjectCacheImpl& ax_object_cache) {
  return new AXSpinButtonPart(ax_object_cache);
}

void AXSpinButtonPart::GetRelativeBounds(
    AXObject** out_container,
    FloatRect& out_bounds_in_container,
    SkMatrix44& out_container_transform) const {
  *out_container = nullptr;
  out_bounds_in_container = FloatRect();
  out_container_transform.setIdentity();

  if (!ParentObject())
    return;

  // FIXME: This logic should exist in the layout tree or elsewhere, but there
  // is no relationship that exists that can be queried.
  ParentObject()->GetRelativeBounds(out_container, out_bounds_in_container,
                                    out_container_transform);
  out_bounds_in_container = FloatRect(0, 0, out_bounds_in_container.Width(),
                                      out_bounds_in_container.Height());
  if (is_incrementor_) {
    out_bounds_in_container.SetHeight(out_bounds_in_container.Height() / 2);
  } else {
    out_bounds_in_container.SetY(out_bounds_in_container.Y() +
                                 out_bounds_in_container.Height() / 2);
    out_bounds_in_container.SetHeight(out_bounds_in_container.Height() / 2);
  }
  *out_container = ParentObject();
}

bool AXSpinButtonPart::Press() {
  if (!parent_ || !parent_->IsSpinButton())
    return false;

  AXSpinButton* spin_button = ToAXSpinButton(ParentObject());
  if (is_incrementor_)
    spin_button->Step(1);
  else
    spin_button->Step(-1);

  return true;
}

}  // namespace blink
