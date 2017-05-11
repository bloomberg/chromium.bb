// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/TouchActionUtil.h"

#include "core/dom/Node.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"

namespace blink {
namespace TouchActionUtil {

namespace {

// touch-action applies to all elements with both width AND height properties.
// According to the CSS Box Model Spec
// (http://dev.w3.org/csswg/css-box/#the-width-and-height-properties)
// width applies to all elements but non-replaced inline elements, table rows,
// and row groups and height applies to all elements but non-replaced inline
// elements, table columns, and column groups.
bool SupportsTouchAction(const LayoutObject& object) {
  if (object.IsInline() && !object.IsAtomicInlineLevel())
    return false;
  if (object.IsTableRow() || object.IsLayoutTableCol())
    return false;

  return true;
}

const Node* ParentNodeAcrossFrames(const Node* cur_node) {
  Node* parent_node = FlatTreeTraversal::Parent(*cur_node);
  if (parent_node)
    return parent_node;

  if (cur_node->IsDocumentNode()) {
    const Document* doc = ToDocument(cur_node);
    return doc->LocalOwner();
  }

  return nullptr;
}

}  // namespace

TouchAction ComputeEffectiveTouchAction(const Node& node) {
  // Start by permitting all actions, then walk the elements supporting
  // touch-action from the target node up to root document, exclude any
  // prohibited actions at or below the element that supports them.
  // I.e. pan-related actions are considered up to the nearest scroller,
  // and zoom related actions are considered up to the root.
  TouchAction effective_touch_action = TouchAction::kTouchActionAuto;
  TouchAction handled_touch_actions = TouchAction::kTouchActionNone;
  for (const Node* cur_node = &node; cur_node;
       cur_node = ParentNodeAcrossFrames(cur_node)) {
    if (LayoutObject* layout_object = cur_node->GetLayoutObject()) {
      if (SupportsTouchAction(*layout_object)) {
        TouchAction action = layout_object->Style()->GetTouchAction();
        action |= handled_touch_actions;
        effective_touch_action &= action;
        if (effective_touch_action == TouchAction::kTouchActionNone)
          break;
      }

      // If we've reached an ancestor that supports panning, stop allowing
      // panning to be disabled.
      if ((layout_object->IsBox() &&
           ToLayoutBox(layout_object)->ScrollsOverflow()) ||
          layout_object->IsLayoutView())
        handled_touch_actions |= TouchAction::kTouchActionPan;
    }
  }
  return effective_touch_action;
}

}  // namespace TouchActionUtil
}  // namespace blink
