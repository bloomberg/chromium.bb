/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AXMenuListPopup_h
#define AXMenuListPopup_h

#include "modules/accessibility/AXMockObject.h"

namespace blink {

class AXObjectCacheImpl;
class AXMenuListOption;
class HTMLElement;

class AXMenuListPopup final : public AXMockObject {
  WTF_MAKE_NONCOPYABLE(AXMenuListPopup);

 public:
  static AXMenuListPopup* Create(AXObjectCacheImpl& ax_object_cache) {
    return new AXMenuListPopup(ax_object_cache);
  }

  bool IsEnabled() const override;
  bool IsOffScreen() const override;

  void DidUpdateActiveOption(int option_index);
  void DidShow();
  void DidHide();
  AXObject* ActiveDescendant() final;

 private:
  explicit AXMenuListPopup(AXObjectCacheImpl&);

  bool IsMenuListPopup() const override { return true; }

  AccessibilityRole RoleValue() const override { return kMenuListPopupRole; }

  bool IsVisible() const override;
  bool Press() override;
  void AddChildren() override;
  void UpdateChildrenIfNecessary() override;
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

  AXMenuListOption* MenuListOptionAXObject(HTMLElement*) const;
  int GetSelectedIndex() const;

  // Note that this may be -1 if nothing is selected.
  int active_index_;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXMenuListPopup, IsMenuListPopup());

}  // namespace blink

#endif  // AXMenuListPopup_h
