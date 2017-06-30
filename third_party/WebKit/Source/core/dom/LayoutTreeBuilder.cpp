/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/dom/LayoutTreeBuilder.h"

#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/FirstLetterPseudoElement.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/InsertionPoint.h"
#include "core/dom/Node.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/Text.h"
#include "core/layout/LayoutFullScreen.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutView.h"
#include "core/svg/SVGElement.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

LayoutTreeBuilderForElement::LayoutTreeBuilderForElement(Element& element,
                                                         ComputedStyle* style)
    : LayoutTreeBuilder(element, nullptr), style_(style) {
  DCHECK(!element.IsActiveSlotOrActiveInsertionPoint());
  // TODO(ecobos): Move the first-letter logic inside parentLayoutObject too?
  // It's an extra (unnecessary) check for text nodes, though.
  if (element.IsFirstLetterPseudoElement()) {
    if (LayoutObject* next_layout_object =
            FirstLetterPseudoElement::FirstLetterTextLayoutObject(element))
      layout_object_parent_ = next_layout_object->Parent();
  } else {
    layout_object_parent_ =
        LayoutTreeBuilderTraversal::ParentLayoutObject(element);
  }
}

LayoutObject* LayoutTreeBuilderForElement::NextLayoutObject() const {
  DCHECK(layout_object_parent_);

  if (node_->IsInTopLayer())
    return LayoutTreeBuilderTraversal::NextInTopLayer(*node_);

  if (node_->IsFirstLetterPseudoElement())
    return FirstLetterPseudoElement::FirstLetterTextLayoutObject(*node_);

  return LayoutTreeBuilder::NextLayoutObject();
}

LayoutObject* LayoutTreeBuilderForElement::ParentLayoutObject() const {
  if (layout_object_parent_) {
    // FIXME: Guarding this by parentLayoutObject isn't quite right as the spec
    // for top layer only talks about display: none ancestors so putting a
    // <dialog> inside an <optgroup> seems like it should still work even though
    // this check will prevent it.
    if (node_->IsInTopLayer())
      return node_->GetDocument().GetLayoutView();
  }

  return layout_object_parent_;
}

DISABLE_CFI_PERF
bool LayoutTreeBuilderForElement::ShouldCreateLayoutObject() const {
  if (!layout_object_parent_)
    return false;

  LayoutObject* parent_layout_object = this->ParentLayoutObject();
  if (!parent_layout_object)
    return false;
  if (!parent_layout_object->CanHaveChildren())
    return false;
  return node_->LayoutObjectIsNeeded(Style());
}

ComputedStyle& LayoutTreeBuilderForElement::Style() const {
  if (!style_)
    style_ = node_->StyleForLayoutObject();
  return *style_;
}

DISABLE_CFI_PERF
void LayoutTreeBuilderForElement::CreateLayoutObject() {
  ComputedStyle& style = this->Style();

  LayoutObject* new_layout_object = node_->CreateLayoutObject(style);
  if (!new_layout_object)
    return;

  LayoutObject* parent_layout_object = this->ParentLayoutObject();

  if (!parent_layout_object->IsChildAllowed(new_layout_object, style)) {
    new_layout_object->Destroy();
    return;
  }

  // Make sure the LayoutObject already knows it is going to be added to a
  // LayoutFlowThread before we set the style for the first time. Otherwise code
  // using inLayoutFlowThread() in the styleWillChange and styleDidChange will
  // fail.
  new_layout_object->SetIsInsideFlowThread(
      parent_layout_object->IsInsideFlowThread());

  LayoutObject* next_layout_object = this->NextLayoutObject();
  node_->SetLayoutObject(new_layout_object);
  new_layout_object->SetStyle(
      &style);  // setStyle() can depend on layoutObject() already being set.

  if (Fullscreen::IsFullscreenElement(*node_)) {
    new_layout_object = LayoutFullScreen::WrapLayoutObject(
        new_layout_object, parent_layout_object, &node_->GetDocument());
    if (!new_layout_object)
      return;
  }

  // Note: Adding newLayoutObject instead of layoutObject(). layoutObject() may
  // be a child of newLayoutObject.
  parent_layout_object->AddChild(new_layout_object, next_layout_object);
}

void LayoutTreeBuilderForText::CreateLayoutObject() {
  ComputedStyle& style = *style_;

  DCHECK(style_ == layout_object_parent_->Style() ||
         ToElement(LayoutTreeBuilderTraversal::Parent(*node_))
             ->HasDisplayContentsStyle());

  LayoutText* new_layout_object = node_->CreateTextLayoutObject(style);
  if (!layout_object_parent_->IsChildAllowed(new_layout_object, style)) {
    new_layout_object->Destroy();
    return;
  }

  // Make sure the LayoutObject already knows it is going to be added to a
  // LayoutFlowThread before we set the style for the first time. Otherwise code
  // using inLayoutFlowThread() in the styleWillChange and styleDidChange will
  // fail.
  new_layout_object->SetIsInsideFlowThread(
      layout_object_parent_->IsInsideFlowThread());

  LayoutObject* next_layout_object = this->NextLayoutObject();
  node_->SetLayoutObject(new_layout_object);
  // Parent takes care of the animations, no need to call setAnimatableStyle.
  new_layout_object->SetStyle(&style);
  layout_object_parent_->AddChild(new_layout_object, next_layout_object);
}

}  // namespace blink
