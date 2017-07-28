/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/PseudoElement.h"

#include "core/dom/FirstLetterPseudoElement.h"
#include "core/frame/UseCounter.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutQuote.h"
#include "core/probe/CoreProbes.h"
#include "core/style/ContentData.h"

namespace blink {

PseudoElement* PseudoElement::Create(Element* parent, PseudoId pseudo_id) {
  return new PseudoElement(parent, pseudo_id);
}

const QualifiedName& PseudoElementTagName(PseudoId pseudo_id) {
  switch (pseudo_id) {
    case kPseudoIdAfter: {
      DEFINE_STATIC_LOCAL(QualifiedName, after,
                          (g_null_atom, "<pseudo:after>", g_null_atom));
      return after;
    }
    case kPseudoIdBefore: {
      DEFINE_STATIC_LOCAL(QualifiedName, before,
                          (g_null_atom, "<pseudo:before>", g_null_atom));
      return before;
    }
    case kPseudoIdBackdrop: {
      DEFINE_STATIC_LOCAL(QualifiedName, backdrop,
                          (g_null_atom, "<pseudo:backdrop>", g_null_atom));
      return backdrop;
    }
    case kPseudoIdFirstLetter: {
      DEFINE_STATIC_LOCAL(QualifiedName, first_letter,
                          (g_null_atom, "<pseudo:first-letter>", g_null_atom));
      return first_letter;
    }
    default:
      NOTREACHED();
  }
  DEFINE_STATIC_LOCAL(QualifiedName, name,
                      (g_null_atom, "<pseudo>", g_null_atom));
  return name;
}

String PseudoElement::PseudoElementNameForEvents(PseudoId pseudo_id) {
  DEFINE_STATIC_LOCAL(const String, after, ("::after"));
  DEFINE_STATIC_LOCAL(const String, before, ("::before"));
  switch (pseudo_id) {
    case kPseudoIdAfter:
      return after;
    case kPseudoIdBefore:
      return before;
    default:
      return g_empty_string;
  }
}

PseudoElement::PseudoElement(Element* parent, PseudoId pseudo_id)
    : Element(PseudoElementTagName(pseudo_id),
              &parent->GetDocument(),
              kCreateElement),
      pseudo_id_(pseudo_id) {
  DCHECK_NE(pseudo_id, kPseudoIdNone);
  parent->GetTreeScope().AdoptIfNeeded(*this);
  SetParentOrShadowHostNode(parent);
  SetHasCustomStyleCallbacks();
  if ((pseudo_id == kPseudoIdBefore || pseudo_id == kPseudoIdAfter) &&
      parent->HasTagName(HTMLNames::inputTag)) {
    UseCounter::Count(parent->GetDocument(),
                      WebFeature::kPseudoBeforeAfterForInputElement);
  }
}

PassRefPtr<ComputedStyle> PseudoElement::CustomStyleForLayoutObject() {
  return ParentOrShadowHostElement()->PseudoStyle(
      PseudoStyleRequest(pseudo_id_));
}

void PseudoElement::Dispose() {
  DCHECK(ParentOrShadowHostElement());

  probe::pseudoElementDestroyed(this);

  DCHECK(!nextSibling());
  DCHECK(!previousSibling());

  DetachLayoutTree();
  Element* parent = ParentOrShadowHostElement();
  GetDocument().AdoptIfNeeded(*this);
  SetParentOrShadowHostNode(0);
  RemovedFrom(parent);
}

void PseudoElement::AttachLayoutTree(AttachContext& context) {
  DCHECK(!GetLayoutObject());

  Element::AttachLayoutTree(context);

  LayoutObject* layout_object = this->GetLayoutObject();
  if (!layout_object)
    return;

  ComputedStyle& style = layout_object->MutableStyleRef();
  if (style.StyleType() != kPseudoIdBefore &&
      style.StyleType() != kPseudoIdAfter)
    return;
  DCHECK(style.GetContentData());

  for (const ContentData* content = style.GetContentData(); content;
       content = content->Next()) {
    LayoutObject* child = content->CreateLayoutObject(*this, style);
    if (layout_object->IsChildAllowed(child, style)) {
      layout_object->AddChild(child);
      if (child->IsQuote())
        ToLayoutQuote(child)->AttachQuote();
    } else {
      child->Destroy();
    }
  }
}

bool PseudoElement::LayoutObjectIsNeeded(const ComputedStyle& style) {
  return PseudoElementLayoutObjectIsNeeded(&style);
}

void PseudoElement::DidRecalcStyle() {
  if (!GetLayoutObject())
    return;

  // The layoutObjects inside pseudo elements are anonymous so they don't get
  // notified of recalcStyle and must have the style propagated downward
  // manually similar to LayoutObject::propagateStyleToAnonymousChildren.
  LayoutObject* layout_object = this->GetLayoutObject();
  for (LayoutObject* child = layout_object->NextInPreOrder(layout_object);
       child; child = child->NextInPreOrder(layout_object)) {
    // We only manage the style for the generated content items.
    if (!child->IsText() && !child->IsQuote() && !child->IsImage())
      continue;

    child->SetPseudoStyle(layout_object->MutableStyle());
  }
}

// With PseudoElements the DOM tree and Layout tree can differ. When you attach
// a, first-letter for example, into the DOM we walk down the Layout
// tree to find the correct insertion point for the LayoutObject. But, this
// means if we ask for the parentOrShadowHost Node from the first-letter
// pseudo element we will get some arbitrary ancestor of the LayoutObject.
//
// For hit testing, we need the parent Node of the LayoutObject for the
// first-letter pseudo element. So, by walking up the Layout tree we know
// we will get the parent and not some other ancestor.
Node* PseudoElement::FindAssociatedNode() const {
  // The ::backdrop element is parented to the LayoutView, not to the node
  // that it's associated with. We need to make sure ::backdrop sends the
  // events to the parent node correctly.
  if (GetPseudoId() == kPseudoIdBackdrop)
    return ParentOrShadowHostNode();

  DCHECK(GetLayoutObject());
  DCHECK(GetLayoutObject()->Parent());

  // We can have any number of anonymous layout objects inserted between
  // us and our parent so make sure we skip over them.
  LayoutObject* ancestor = GetLayoutObject()->Parent();
  while (ancestor->IsAnonymous() ||
         (ancestor->GetNode() && ancestor->GetNode()->IsPseudoElement())) {
    DCHECK(ancestor->Parent());
    ancestor = ancestor->Parent();
  }
  return ancestor->GetNode();
}

}  // namespace blink
