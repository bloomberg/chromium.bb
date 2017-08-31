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

#ifndef AXMenuList_h
#define AXMenuList_h

#include "modules/accessibility/AXLayoutObject.h"

namespace blink {

class AXObjectCacheImpl;
class LayoutMenuList;

class AXMenuList final : public AXLayoutObject {
  WTF_MAKE_NONCOPYABLE(AXMenuList);

 public:
  static AXMenuList* Create(LayoutMenuList* layout_object, AXObjectCacheImpl&);

  bool IsCollapsed() const override;
  AccessibilityExpanded IsExpanded() const final;
  bool OnNativeClickAction() override;
  void ClearChildren() override;

  void DidUpdateActiveOption(int option_index);
  void DidShowPopup();
  void DidHidePopup();

 private:
  friend class AXMenuListOption;

  AXMenuList(LayoutMenuList*, AXObjectCacheImpl&);

  bool IsMenuList() const override { return true; }
  AccessibilityRole DetermineAccessibilityRole() final;

  void AddChildren() override;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXMenuList, IsMenuList());

}  // namespace blink

#endif  // AXMenuList_h
