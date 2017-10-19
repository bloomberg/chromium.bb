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

#ifndef AXSpinButton_h
#define AXSpinButton_h

#include "core/html/forms/SpinButtonElement.h"
#include "modules/accessibility/AXMockObject.h"

namespace blink {

class AXObjectCacheImpl;

class AXSpinButton final : public AXMockObject {
  WTF_MAKE_NONCOPYABLE(AXSpinButton);

 public:
  static AXSpinButton* Create(AXObjectCacheImpl&);
  ~AXSpinButton() override;
  virtual void Trace(blink::Visitor*);

  void SetSpinButtonElement(SpinButtonElement* spin_button) {
    spin_button_element_ = spin_button;
  }
  void Step(int amount);

 private:
  explicit AXSpinButton(AXObjectCacheImpl&);

  AccessibilityRole RoleValue() const override;
  bool IsSpinButton() const override { return true; }
  bool IsNativeSpinButton() const override { return true; }
  void AddChildren() override;
  LayoutObject* LayoutObjectForRelativeBounds() const override;
  void Detach() override;
  void DetachFromParent() override;

  Member<SpinButtonElement> spin_button_element_;
};

class AXSpinButtonPart final : public AXMockObject {
 public:
  static AXSpinButtonPart* Create(AXObjectCacheImpl&);
  ~AXSpinButtonPart() override {}
  void SetIsIncrementor(bool value) { is_incrementor_ = value; }

 private:
  explicit AXSpinButtonPart(AXObjectCacheImpl&);
  bool is_incrementor_ : 1;

  bool OnNativeClickAction() override;
  AccessibilityRole RoleValue() const override { return kButtonRole; }
  bool IsSpinButtonPart() const override { return true; }
  void GetRelativeBounds(AXObject** out_container,
                         FloatRect& out_bounds_in_container,
                         SkMatrix44& out_container_transform) const override;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXSpinButton, IsNativeSpinButton());
DEFINE_AX_OBJECT_TYPE_CASTS(AXSpinButtonPart, IsSpinButtonPart());

}  // namespace blink

#endif  // AXSpinButton_h
