// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomLayoutChild_h
#define CustomLayoutChild_h

#include "core/css/cssom/PrepopulatedComputedStylePropertyMap.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSLayoutDefinition;
class CustomLayoutConstraintsOptions;
class CustomLayoutFragmentRequest;
class LayoutBox;

// Represents a "CSS box" for use by a web developer. This is passed into the
// web developer defined layout and intrinsicSizes functions so that they can
// perform layout on these children.
//
// The represent all inflow children, out-of-flow children (fixed/absolute) do
// not appear in the children list.
class CustomLayoutChild : public ScriptWrappable {
  WTF_MAKE_NONCOPYABLE(CustomLayoutChild);
  DEFINE_WRAPPERTYPEINFO();

 public:
  CustomLayoutChild(const CSSLayoutDefinition&, LayoutBox*);
  virtual ~CustomLayoutChild() = default;

  // LayoutChild.idl
  PrepopulatedComputedStylePropertyMap* styleMap() const { return style_map_; }
  CustomLayoutFragmentRequest* layoutNextFragment(
      const CustomLayoutConstraintsOptions&);

  LayoutBox* GetLayoutBox() const {
    DCHECK(box_);
    return box_;
  }
  void ClearLayoutBox() { box_ = nullptr; }

  // A layout child may be invalid if it has been removed from the tree (it is
  // possible for a web developer to hold onto a LayoutChild object after its
  // underlying LayoutObject has been destroyed).
  bool IsValid() const { return box_; }

  void Trace(blink::Visitor*) override;

 private:
  LayoutBox* box_;
  Member<PrepopulatedComputedStylePropertyMap> style_map_;
};

}  // namespace blink

#endif  // CustomLayoutChild_h
