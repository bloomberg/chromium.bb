// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutCustom_h
#define LayoutCustom_h

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/custom/CSSLayoutDefinition.h"

namespace blink {

// NOTE: In the future there may be a third state "normal", this will mean that
// not everything is blockified, (e.g. root inline boxes, so that line-by-line
// layout can be performed).
enum LayoutCustomState { kUnloaded, kBlock };

// The LayoutObject for elements which have "display: layout(foo);" specified.
// https://drafts.css-houdini.org/css-layout-api/
//
// This class inherits from LayoutBlockFlow so that when a web developer's
// intrinsicSizes/layout callback fails, it will fallback onto the default
// block-flow layout algorithm.
class LayoutCustom final : public LayoutBlockFlow {
 public:
  explicit LayoutCustom(Element*);

  const char* GetName() const override { return "LayoutCustom"; }
  LayoutCustomState State() const { return state_; }

  bool CreatesNewFormattingContext() const override { return true; }

  void AddChild(LayoutObject* new_child, LayoutObject* before_child) override;
  void RemoveChild(LayoutObject* child) override;

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void UpdateBlockLayout(bool relayout_children) override;

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutCustom || LayoutBlockFlow::IsOfType(type);
  }

  bool PerformLayout(bool relayout_children, SubtreeLayoutScope*);

  LayoutCustomState state_;
  Persistent<CSSLayoutDefinition::Instance> instance_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutCustom, IsLayoutCustom());

}  // namespace blink

#endif  // LayoutCustom_h
