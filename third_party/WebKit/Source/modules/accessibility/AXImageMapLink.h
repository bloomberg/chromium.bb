/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef AXImageMapLink_h
#define AXImageMapLink_h

#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLMapElement.h"
#include "modules/accessibility/AXNodeObject.h"

namespace blink {

class AXObjectCacheImpl;

class AXImageMapLink final : public AXNodeObject {
  WTF_MAKE_NONCOPYABLE(AXImageMapLink);

 private:
  explicit AXImageMapLink(HTMLAreaElement*, AXObjectCacheImpl&);

 public:
  static AXImageMapLink* Create(HTMLAreaElement*, AXObjectCacheImpl&);
  ~AXImageMapLink() override;
  virtual void Trace(blink::Visitor*);

  HTMLAreaElement* AreaElement() const { return ToHTMLAreaElement(GetNode()); }

  HTMLMapElement* MapElement() const;

  AccessibilityRole RoleValue() const override;
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

  Element* AnchorElement() const override;
  Element* ActionElement() const override;
  KURL Url() const override;
  bool IsLink() const override { return true; }
  bool IsLinked() const override { return true; }
  AXObject* ComputeParent() const override;
  void GetRelativeBounds(AXObject** out_container,
                         FloatRect& out_bounds_in_container,
                         SkMatrix44& out_container_transform) const override;

 private:
  bool IsImageMapLink() const override { return true; }
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXImageMapLink, IsImageMapLink());

}  // namespace blink

#endif  // AXImageMapLink_h
