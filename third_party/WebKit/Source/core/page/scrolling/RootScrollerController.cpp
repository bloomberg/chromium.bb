// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/RootScrollerController.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/StyleChangeReason.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/Page.h"
#include "core/page/scrolling/RootScrollerUtil.h"
#include "core/page/scrolling/TopDocumentRootScrollerController.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
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
         bounding_box.Size() == top_document.GetLayoutViewItem().Size();
}

}  // namespace

// static
RootScrollerController* RootScrollerController::Create(Document& document) {
  return new RootScrollerController(document);
}

RootScrollerController::RootScrollerController(Document& document)
    : document_(&document),
      effective_root_scroller_(&document),
      document_has_document_element_(false) {}

DEFINE_TRACE(RootScrollerController) {
  visitor->Trace(document_);
  visitor->Trace(root_scroller_);
  visitor->Trace(effective_root_scroller_);
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
  DCHECK(document_->Lifecycle().GetState() == DocumentLifecycle::kLayoutClean);
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

void RootScrollerController::RecomputeEffectiveRootScroller() {
  bool root_scroller_valid =
      root_scroller_ && IsValidRootScroller(*root_scroller_);

  Node* new_effective_root_scroller = document_;
  if (root_scroller_valid)
    new_effective_root_scroller = root_scroller_;

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
    if (effective_root_scroller_ == new_effective_root_scroller)
      return;
  }

  Node* old_effective_root_scroller = effective_root_scroller_;
  effective_root_scroller_ = new_effective_root_scroller;

  ApplyRootScrollerProperties(*old_effective_root_scroller);
  ApplyRootScrollerProperties(*effective_root_scroller_);

  // Document (i.e. LayoutView) gets its background style from the rootScroller
  // so we need to recalc its style. Ensure that we get back to a LayoutClean
  // state after.
  document_->SetNeedsStyleRecalc(kLocalStyleChange,
                                 StyleChangeReasonForTracing::Create(
                                     StyleChangeReason::kStyleInvalidator));
  document_->UpdateStyleAndLayout();

  if (Page* page = document_->GetPage())
    page->GlobalRootScrollerController().DidChangeRootScroller();
}

bool RootScrollerController::IsValidRootScroller(const Element& element) const {
  if (!element.IsInTreeScope())
    return false;

  if (!element.GetLayoutObject())
    return false;

  if (!element.GetLayoutObject()->HasOverflowClip() &&
      !element.IsFrameOwnerElement())
    return false;

  if (!FillsViewport(element))
    return false;

  return true;
}

void RootScrollerController::ApplyRootScrollerProperties(Node& node) const {
  DCHECK(document_->GetFrame());
  DCHECK(document_->GetFrame()->View());

  // If the node has been removed from the Document, we shouldn't be touching
  // anything related to the Frame- or Layout- hierarchies.
  if (!node.IsInTreeScope())
    return;

  if (node.IsFrameOwnerElement()) {
    HTMLFrameOwnerElement* frame_owner = ToHTMLFrameOwnerElement(&node);
    DCHECK(frame_owner->ContentFrame());

    if (frame_owner->ContentFrame()->IsLocalFrame()) {
      LocalFrameView* frame_view =
          ToLocalFrame(frame_owner->ContentFrame())->View();

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
}

void RootScrollerController::UpdateIFrameGeometryAndLayoutSize(
    HTMLFrameOwnerElement& frame_owner) const {
  DCHECK(document_->GetFrame());
  DCHECK(document_->GetFrame()->View());

  LocalFrameView* child_view =
      ToLocalFrameView(frame_owner.OwnedEmbeddedContentView());

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

  RecomputeEffectiveRootScroller();
  DCHECK(element != effective_root_scroller_.Get());
}

}  // namespace blink
