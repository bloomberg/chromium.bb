// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/RootScrollerController.h"

#include "core/css/StyleChangeReason.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/LocalFrameView.h"
#include "core/fullscreen/DocumentFullscreen.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/LayoutView.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/scrolling/RootScrollerUtil.h"
#include "core/page/scrolling/TopDocumentRootScrollerController.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "core/paint/compositing/PaintLayerCompositor.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

class RootFrameViewport;

namespace {

bool FillsViewport(const Element& element) {
  DCHECK(element.GetLayoutObject());
  DCHECK(element.GetLayoutObject()->IsBox());

  LayoutObject* layout_object = element.GetLayoutObject();

  // TODO(bokan): Broken for OOPIF. crbug.com/642378.
  Document& top_document = element.GetDocument().TopDocument();

  Vector<FloatQuad> quads;
  layout_object->AbsoluteQuads(quads);
  DCHECK_EQ(quads.size(), 1u);

  if (!quads[0].IsRectilinear())
    return false;

  LayoutRect bounding_box(quads[0].BoundingBox());

  return bounding_box.Location() == LayoutPoint::Zero() &&
         bounding_box.Size() == top_document.GetLayoutView()->GetLayoutSize();
}

}  // namespace

// static
RootScrollerController* RootScrollerController::Create(Document& document) {
  return new RootScrollerController(document);
}

RootScrollerController::RootScrollerController(Document& document)
    : document_(&document),
      effective_root_scroller_(&document),
      document_has_document_element_(false),
      needs_apply_properties_(false) {}

void RootScrollerController::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(root_scroller_);
  visitor->Trace(effective_root_scroller_);
  visitor->Trace(implicit_candidates_);
  visitor->Trace(implicit_root_scroller_);
}

void RootScrollerController::Set(Element* new_root_scroller) {
  if (root_scroller_ == new_root_scroller)
    return;

  root_scroller_ = new_root_scroller;
  RecomputeEffectiveRootScroller();
}

Element* RootScrollerController::Get() const {
  return root_scroller_;
}

Node& RootScrollerController::EffectiveRootScroller() const {
  DCHECK(effective_root_scroller_);
  return *effective_root_scroller_;
}

void RootScrollerController::DidUpdateLayout() {
  RecomputeEffectiveRootScroller();
}

void RootScrollerController::DidResizeFrameView() {
  DCHECK(document_);

  Page* page = document_->GetPage();
  if (document_->GetFrame() && document_->GetFrame()->IsMainFrame() && page)
    page->GlobalRootScrollerController().DidResizeViewport();

  // If the effective root scroller in this Document is a Frame, it'll match
  // its parent's frame rect. We can't rely on layout to kick it to update its
  // geometry so we do so explicitly here.
  if (EffectiveRootScroller().IsFrameOwnerElement()) {
    UpdateIFrameGeometryAndLayoutSize(
        *ToHTMLFrameOwnerElement(&EffectiveRootScroller()));
  }
}

void RootScrollerController::DidUpdateIFrameFrameView(
    HTMLFrameOwnerElement& element) {
  if (&element != root_scroller_.Get() && &element != implicit_root_scroller_)
    return;

  // Reevaluate whether the iframe should be the effective root scroller (e.g.
  // demote it if it became remote). Ensure properties are re-applied even if
  // the effective root scroller doesn't change since the FrameView might have
  // been swapped out.
  needs_apply_properties_ = true;
  RecomputeEffectiveRootScroller();
}

void RootScrollerController::RecomputeEffectiveRootScroller() {
  ProcessImplicitCandidates();

  Node* new_effective_root_scroller = document_;

  if (!DocumentFullscreen::fullscreenElement(*document_)) {
    bool root_scroller_valid =
        root_scroller_ && IsValidRootScroller(*root_scroller_);
    if (root_scroller_valid)
      new_effective_root_scroller = root_scroller_;
    else if (implicit_root_scroller_)
      new_effective_root_scroller = implicit_root_scroller_;
  }

  // TODO(bokan): This is a terrible hack but required because the viewport
  // apply scroll works on Elements rather than Nodes. If we're going from
  // !documentElement to documentElement, we can't early out even if the root
  // scroller didn't change since the global root scroller didn't have an
  // Element previously to put it's ViewportScrollCallback onto. We need this
  // to kick the global root scroller to recompute itself. We can remove this
  // if ScrollCustomization is moved to the Node rather than Element.
  bool old_has_document_element = document_has_document_element_;
  document_has_document_element_ = document_->documentElement();

  if (old_has_document_element || !document_has_document_element_) {
    if (effective_root_scroller_ == new_effective_root_scroller) {
      if (needs_apply_properties_)
        ApplyRootScrollerProperties(*effective_root_scroller_);

      return;
    }
  }

  Node* old_effective_root_scroller = effective_root_scroller_;
  effective_root_scroller_ = new_effective_root_scroller;

  ApplyRootScrollerProperties(*old_effective_root_scroller);
  ApplyRootScrollerProperties(*effective_root_scroller_);

  if (Page* page = document_->GetPage())
    page->GlobalRootScrollerController().DidChangeRootScroller();
}

bool RootScrollerController::IsValidRootScroller(const Element& element) const {
  if (!element.IsInTreeScope())
    return false;

  if (!element.GetLayoutObject())
    return false;

  // Ignore anything inside a FlowThread (multi-col, paginated, etc.).
  if (element.GetLayoutObject()->IsInsideFlowThread())
    return false;

  if (!element.GetLayoutObject()->HasOverflowClip() &&
      !element.IsFrameOwnerElement())
    return false;

  if (element.IsFrameOwnerElement()) {
    const HTMLFrameOwnerElement* frame_owner =
        ToHTMLFrameOwnerElement(&element);

    if (!frame_owner)
      return false;

    if (!frame_owner->OwnedEmbeddedContentView())
      return false;

    // TODO(bokan): Make work with OOPIF. crbug.com/642378.
    if (!frame_owner->OwnedEmbeddedContentView()->IsLocalFrameView())
      return false;
  }

  if (!FillsViewport(element))
    return false;

  return true;
}

bool RootScrollerController::IsValidImplicit(const Element& element) const {
  // Valid implicit root scroller are a subset of valid root scrollers.
  if (!IsValidRootScroller(element))
    return false;

  // Valid iframes can always be implicitly promoted.
  if (element.IsFrameOwnerElement())
    return true;

  const ComputedStyle* style = element.GetLayoutObject()->Style();
  if (!style)
    return false;

  // Regular Elements must have overflow: auto or scroll.
  return style->OverflowX() == EOverflow::kAuto ||
         style->OverflowX() == EOverflow::kScroll ||
         style->OverflowY() == EOverflow::kAuto ||
         style->OverflowY() == EOverflow::kScroll;
}

void RootScrollerController::ApplyRootScrollerProperties(Node& node) {
  DCHECK(document_->GetFrame());
  DCHECK(document_->GetFrame()->View());

  if (&node == effective_root_scroller_)
    needs_apply_properties_ = false;

  // If the node has been removed from the Document, we shouldn't be touching
  // anything related to the Frame- or Layout- hierarchies.
  if (!node.IsInTreeScope())
    return;

  if (!node.IsFrameOwnerElement())
    return;

  HTMLFrameOwnerElement* frame_owner = ToHTMLFrameOwnerElement(&node);

  // The current effective root scroller may have lost its ContentFrame. If
  // that's the case, there's nothing to be done. https://crbug.com/805317 for
  // an example of how we get here.
  if (!frame_owner->ContentFrame())
    return;

  if (frame_owner->ContentFrame()->IsLocalFrame()) {
    LocalFrameView* frame_view =
        ToLocalFrameView(frame_owner->OwnedEmbeddedContentView());

    bool is_root_scroller = &EffectiveRootScroller() == &node;

    // If we're making the Frame the root scroller, it must have a FrameView
    // by now.
    DCHECK(frame_view || !is_root_scroller);
    if (frame_view) {
      frame_view->SetLayoutSizeFixedToFrameSize(!is_root_scroller);
      UpdateIFrameGeometryAndLayoutSize(*frame_owner);
    }
  } else {
    // TODO(bokan): Make work with OOPIF. crbug.com/642378.
  }
}

void RootScrollerController::UpdateIFrameGeometryAndLayoutSize(
    HTMLFrameOwnerElement& frame_owner) const {
  DCHECK(document_->GetFrame());
  DCHECK(document_->GetFrame()->View());

  LocalFrameView* child_view =
      ToLocalFrameView(frame_owner.OwnedEmbeddedContentView());

  if (!child_view)
    return;

  // We can get here as a result of the "post layout resize" on the main frame.
  // That happens from inside LocalFrameView::PerformLayout. Calling
  // UpdateGeometry on the iframe causes it to layout which calls
  // Document::UpdateStyleAndLayout. That tries to recurse up the hierarchy,
  // reentering Layout on this Document. Thus, we avoid calling this here if
  // we're in layout; it'll get called when this Document finishes laying out.
  if (!document_->GetFrame()->View()->IsInPerformLayout())
    child_view->UpdateGeometry();

  if (&EffectiveRootScroller() == frame_owner)
    child_view->SetLayoutSize(document_->GetFrame()->View()->GetLayoutSize());
}

void RootScrollerController::ProcessImplicitCandidates() {
  if (!RuntimeEnabledFeatures::ImplicitRootScrollerEnabled())
    return;

  Element* highest_z_element = nullptr;
  bool highest_is_ambiguous = false;

  HeapHashSet<WeakMember<Element>> copy(implicit_candidates_);
  for (auto& element : copy) {
    if (!IsValidImplicit(*element)) {
      implicit_candidates_.erase(element);
      continue;
    }

    if (!highest_z_element) {
      highest_z_element = element;
    } else {
      int element_z = element->GetLayoutObject()->Style()->ZIndex();
      int highest_z = highest_z_element->GetLayoutObject()->Style()->ZIndex();

      if (element_z > highest_z) {
        highest_z_element = element;
        highest_is_ambiguous = false;
      } else if (element_z == highest_z) {
        highest_is_ambiguous = true;
      }
    }
  }

  if (highest_is_ambiguous)
    implicit_root_scroller_ = nullptr;
  else
    implicit_root_scroller_ = highest_z_element;
}

PaintLayer* RootScrollerController::RootScrollerPaintLayer() const {
  return RootScrollerUtil::PaintLayerForRootScroller(effective_root_scroller_);
}

bool RootScrollerController::ScrollsViewport(const Element& element) const {
  if (effective_root_scroller_->IsDocumentNode())
    return element == document_->documentElement();

  return element == effective_root_scroller_.Get();
}

void RootScrollerController::ElementRemoved(const Element& element) {
  if (element != effective_root_scroller_.Get())
    return;

  effective_root_scroller_ = document_;
  if (Page* page = document_->GetPage())
    page->GlobalRootScrollerController().DidChangeRootScroller();
}

void RootScrollerController::ConsiderForImplicit(Node& node) {
  DCHECK(RuntimeEnabledFeatures::ImplicitRootScrollerEnabled());

  if (!node.IsElementNode())
    return;

  if (!IsValidImplicit(ToElement(node)))
    return;

  if (document_->GetPage()->GetChromeClient().IsPopup())
    return;

  implicit_candidates_.insert(&ToElement(node));
}

}  // namespace blink
