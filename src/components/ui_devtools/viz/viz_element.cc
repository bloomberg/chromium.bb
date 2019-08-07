// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/viz/viz_element.h"

#include <algorithm>

#include "components/ui_devtools/ui_element_delegate.h"
#include "components/ui_devtools/viz/frame_sink_element.h"
#include "components/ui_devtools/viz/surface_element.h"

namespace ui_devtools {

namespace {

bool IsVizElement(const UIElement* element) {
  return element->type() == UIElementType::FRAMESINK ||
         element->type() == UIElementType::SURFACE;
}

bool CompareVizElements(const UIElement* a, const UIElement* b) {
  if (a->type() == UIElementType::FRAMESINK) {
    if (b->type() == UIElementType::SURFACE)
      return true;
    // From() checks that |b| is a FrameSinkElement.
    return FrameSinkElement::From(a) < FrameSinkElement::From(b);
  } else {
    if (b->type() == UIElementType::FRAMESINK)
      return false;
    // From() checks that |a| and |b| are SurfaceElements.
    return SurfaceElement::From(a) < SurfaceElement::From(b);
  }
}

}  // namespace

VizElement::VizElement(const UIElementType type,
                       UIElementDelegate* delegate,
                       UIElement* parent)
    : UIElement(type, delegate, parent) {}

VizElement::~VizElement() {}

void VizElement::AddToParentSorted(UIElement* parent, bool notify_delegate) {
  parent->AddOrderedChild(this, &CompareVizElements, notify_delegate);
}

void VizElement::Reparent(UIElement* new_parent) {
  if (parent() == new_parent)
    return;
  // Become a child of |new_parent| without calling OnUIElementRemoved or
  // OnUIElementAdded on |delegate_|. Instead, call OnUIElementReordered, which
  // destroys and rebuilds the DOM node in the new location.
  parent()->RemoveChild(this, false);
  AddToParentSorted(new_parent, false);
  delegate()->OnUIElementReordered(new_parent, this);
  set_parent(new_parent);
}

VizElement* VizElement::AsVizElement(UIElement* element) {
  DCHECK(IsVizElement(element));
  return static_cast<VizElement*>(element);
}

}  // namespace ui_devtools
