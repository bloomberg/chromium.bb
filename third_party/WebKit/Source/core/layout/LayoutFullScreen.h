/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef LayoutFullScreen_h
#define LayoutFullScreen_h

#include "core/CoreExport.h"
#include "core/layout/LayoutFlexibleBox.h"

namespace blink {

class LayoutBlockFlow;

class CORE_EXPORT LayoutFullScreen final : public LayoutFlexibleBox {
 public:
  static LayoutFullScreen* CreateAnonymous(Document*);

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutFullScreen ||
           LayoutFlexibleBox::IsOfType(type);
  }
  const char* GetName() const override { return "LayoutFullScreen"; }

  void ResetPlaceholder() { placeholder_ = nullptr; }
  LayoutBlockFlow* Placeholder() { return placeholder_; }
  void CreatePlaceholder(scoped_refptr<ComputedStyle>,
                         const LayoutRect& frame_rect);

  static LayoutObject* WrapLayoutObject(LayoutObject*,
                                        LayoutObject*,
                                        Document*);
  void UnwrapLayoutObject();

  void UpdateStyle();
  void UpdateStyle(LayoutObject* parent);
  bool AnonymousHasStylePropagationOverride() override { return true; }

  // Must call setStyleWithWritingModeOfParent() instead.
  void SetStyle(scoped_refptr<ComputedStyle>) = delete;

 private:
  LayoutFullScreen();
  void WillBeDestroyed() override;

 protected:
  LayoutBlockFlow* placeholder_;
  ItemPosition SelfAlignmentNormalBehavior(
      const LayoutBox* child = nullptr) const override {
    DCHECK(!child);
    return kItemPositionCenter;
  }
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutFullScreen, IsLayoutFullScreen());

}  // namespace blink

#endif
