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

#ifndef AXMenuListOption_h
#define AXMenuListOption_h

#include "core/html/HTMLOptionElement.h"
#include "modules/accessibility/AXMockObject.h"

namespace blink {

class AXObjectCacheImpl;

class AXMenuListOption final : public AXMockObject {
  WTF_MAKE_NONCOPYABLE(AXMenuListOption);

 public:
  static AXMenuListOption* Create(HTMLOptionElement* element,
                                  AXObjectCacheImpl& ax_object_cache) {
    return new AXMenuListOption(element, ax_object_cache);
  }
  ~AXMenuListOption() override;

 private:
  AXMenuListOption(HTMLOptionElement*, AXObjectCacheImpl&);
  DECLARE_VIRTUAL_TRACE();

  bool IsMenuListOption() const override { return true; }

  Node* GetNode() const override { return element_; }
  void Detach() override;
  bool IsDetached() const override { return !element_; }
  LocalFrameView* DocumentFrameView() const override;
  AccessibilityRole RoleValue() const override;
  bool CanHaveChildren() const override { return false; }
  AXObject* ComputeParent() const override;

  Element* ActionElement() const override;
  bool IsVisible() const override;
  bool IsOffScreen() const override;
  bool IsSelected() const override;
  void SetSelected(bool) override;

  void GetRelativeBounds(AXObject** out_container,
                         FloatRect& out_bounds_in_container,
                         SkMatrix44& out_container_transform) const override;
  String TextAlternative(bool recursive,
                         bool in_aria_labelled_by_traversal,
                         AXObjectSet& visited,
                         AXNameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
  HTMLSelectElement* ParentSelectNode() const;

  Member<HTMLOptionElement> element_;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXMenuListOption, IsMenuListOption());

}  // namespace blink

#endif  // AXMenuListOption_h
