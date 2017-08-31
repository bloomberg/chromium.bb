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

#ifndef AXSlider_h
#define AXSlider_h

#include "modules/accessibility/AXLayoutObject.h"
#include "modules/accessibility/AXMockObject.h"

namespace blink {

class AXObjectCacheImpl;
class HTMLInputElement;

class AXSlider : public AXLayoutObject {
  WTF_MAKE_NONCOPYABLE(AXSlider);

 public:
  static AXSlider* Create(LayoutObject*, AXObjectCacheImpl&);
  ~AXSlider() override {}

 protected:
  AXSlider(LayoutObject*, AXObjectCacheImpl&);

 private:
  HTMLInputElement* GetInputElement() const;
  AXObject* ElementAccessibilityHitTest(const IntPoint&) const final;

  AccessibilityRole DetermineAccessibilityRole() final;
  bool IsSlider() const final { return true; }
  bool IsControl() const final { return true; }

  void AddChildren() final;

  bool OnNativeSetValueAction(const String&) final;
  AccessibilityOrientation Orientation() const final;
};

class AXSliderThumb final : public AXMockObject {
  WTF_MAKE_NONCOPYABLE(AXSliderThumb);

 public:
  static AXSliderThumb* Create(AXObjectCacheImpl&);
  ~AXSliderThumb() override {}

  AccessibilityRole RoleValue() const override { return kSliderThumbRole; }

 private:
  explicit AXSliderThumb(AXObjectCacheImpl&);

  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
  LayoutObject* LayoutObjectForRelativeBounds() const override;
};

}  // namespace blink

#endif  // AXSlider_h
