// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"

namespace blink {

bool StyleRecalcChange::TraverseChildren(const Node& node) const {
  return RecalcChildren() || node.ChildNeedsStyleRecalc();
}

bool StyleRecalcChange::TraversePseudoElements(const Node& node) const {
  return UpdatePseudoElements() || node.ChildNeedsStyleRecalc();
}

bool StyleRecalcChange::TraverseChild(const Node& node) const {
  return ShouldRecalcStyleFor(node) || node.ChildNeedsStyleRecalc();
}

bool StyleRecalcChange::ShouldRecalcStyleFor(const Node& node) const {
  if (RecalcChildren())
    return true;
  if (node.NeedsStyleRecalc())
    return true;
  if (node.GetForceReattachLayoutTree())
    return true;
  if (propagate_ != kClearEnsured)
    return false;
  if (const ComputedStyle* old_style = node.GetComputedStyle())
    return old_style->IsEnsuredInDisplayNone();
  return false;
}

bool StyleRecalcChange::ShouldUpdatePseudoElement(
    const PseudoElement& pseudo_element) const {
  return UpdatePseudoElements() || pseudo_element.NeedsStyleRecalc();
}

}  // namespace blink
