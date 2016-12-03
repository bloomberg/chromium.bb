// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionGeometry.h"

#include "core/dom/Element.h"
#include "core/dom/ElementRareData.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutAPIShim.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/paint/PaintLayer.h"

namespace blink {

namespace {

bool isContainingBlockChainDescendant(LayoutObject* descendant,
                                      LayoutObject* ancestor) {
  LocalFrame* ancestorFrame = ancestor->document().frame();
  LocalFrame* descendantFrame = descendant->document().frame();

  if (ancestor->isLayoutView())
    return descendantFrame && descendantFrame->tree().top() == ancestorFrame;

  if (ancestorFrame != descendantFrame)
    return false;

  while (descendant && descendant != ancestor)
    descendant = descendant->containingBlock();
  return descendant;
}

void mapRectUpToDocument(LayoutRect& rect,
                         const LayoutObject& layoutObject,
                         const Document& document) {
  FloatQuad mappedQuad = layoutObject.localToAbsoluteQuad(
      FloatQuad(FloatRect(rect)), UseTransforms | ApplyContainerFlip);
  rect = LayoutRect(mappedQuad.boundingBox());
}

void mapRectDownToDocument(LayoutRect& rect,
                           LayoutBoxModelObject& layoutObject,
                           const Document& document) {
  FloatQuad mappedQuad = document.layoutView()->ancestorToLocalQuad(
      &layoutObject, FloatQuad(FloatRect(rect)),
      UseTransforms | ApplyContainerFlip | TraverseDocumentBoundaries);
  rect = LayoutRect(mappedQuad.boundingBox());
}

LayoutUnit computeMargin(const Length& length, LayoutUnit referenceLength) {
  if (length.type() == Percent) {
    return LayoutUnit(
        static_cast<int>(referenceLength.toFloat() * length.percent() / 100.0));
  }
  DCHECK_EQ(length.type(), Fixed);
  return LayoutUnit(length.intValue());
}

}  // namespace

IntersectionGeometry::IntersectionGeometry(
    Node* root,
    Element* target,
    const Vector<Length>& rootMargin,
    ReportRootBounds shouldReportRootBounds)
    : m_root(root),
      m_target(target),
      m_rootMargin(rootMargin),
      m_shouldReportRootBounds(shouldReportRootBounds) {
  DCHECK(m_target);
  DCHECK(rootMargin.isEmpty() || rootMargin.size() == 4);
}

IntersectionGeometry::~IntersectionGeometry() {}

Element* IntersectionGeometry::root() const {
  if (m_root && !m_root->isDocumentNode())
    return toElement(m_root);
  return nullptr;
}

LayoutObject* IntersectionGeometry::getRootLayoutObject() const {
  DCHECK(m_root);
  if (m_root->isDocumentNode()) {
    return LayoutAPIShim::layoutObjectFrom(
        toDocument(m_root)->layoutViewItem());
  }
  return toElement(m_root)->layoutObject();
}

void IntersectionGeometry::initializeGeometry() {
  initializeTargetRect();
  m_intersectionRect = m_targetRect;
  initializeRootRect();
  m_doesIntersect = true;
}

void IntersectionGeometry::initializeTargetRect() {
  LayoutObject* targetLayoutObject = m_target->layoutObject();
  DCHECK(targetLayoutObject && targetLayoutObject->isBoxModelObject());
  m_targetRect = LayoutRect(
      toLayoutBoxModelObject(targetLayoutObject)->borderBoundingBox());
}

void IntersectionGeometry::initializeRootRect() {
  LayoutObject* rootLayoutObject = getRootLayoutObject();
  if (rootLayoutObject->isLayoutView()) {
    m_rootRect = LayoutRect(
        toLayoutView(rootLayoutObject)->frameView()->visibleContentRect());
  } else if (rootLayoutObject->isBox() && rootLayoutObject->hasOverflowClip()) {
    m_rootRect = LayoutRect(toLayoutBox(rootLayoutObject)->contentBoxRect());
  } else {
    m_rootRect = LayoutRect(
        toLayoutBoxModelObject(rootLayoutObject)->borderBoundingBox());
  }
  applyRootMargin();
}

void IntersectionGeometry::applyRootMargin() {
  if (m_rootMargin.isEmpty())
    return;

  // TODO(szager): Make sure the spec is clear that left/right margins are
  // resolved against width and not height.
  LayoutUnit topMargin = computeMargin(m_rootMargin[0], m_rootRect.height());
  LayoutUnit rightMargin = computeMargin(m_rootMargin[1], m_rootRect.width());
  LayoutUnit bottomMargin = computeMargin(m_rootMargin[2], m_rootRect.height());
  LayoutUnit leftMargin = computeMargin(m_rootMargin[3], m_rootRect.width());

  m_rootRect.setX(m_rootRect.x() - leftMargin);
  m_rootRect.setWidth(m_rootRect.width() + leftMargin + rightMargin);
  m_rootRect.setY(m_rootRect.y() - topMargin);
  m_rootRect.setHeight(m_rootRect.height() + topMargin + bottomMargin);
}

void IntersectionGeometry::clipToRoot() {
  // Map and clip rect into root element coordinates.
  // TODO(szager): the writing mode flipping needs a test.
  LayoutBox* rootLayoutObject = toLayoutBox(getRootLayoutObject());
  LayoutObject* targetLayoutObject = m_target->layoutObject();

  m_doesIntersect = targetLayoutObject->mapToVisualRectInAncestorSpace(
      rootLayoutObject, m_intersectionRect, EdgeInclusive);
  if (rootLayoutObject->hasOverflowClip())
    m_intersectionRect.move(-rootLayoutObject->scrolledContentOffset());

  if (!m_doesIntersect)
    return;
  LayoutRect rootClipRect(m_rootRect);
  rootLayoutObject->flipForWritingMode(rootClipRect);
  m_doesIntersect &= m_intersectionRect.inclusiveIntersect(rootClipRect);
}

void IntersectionGeometry::mapTargetRectToTargetFrameCoordinates() {
  LayoutObject& targetLayoutObject = *m_target->layoutObject();
  Document& targetDocument = m_target->document();
  LayoutSize scrollPosition =
      LayoutSize(targetDocument.view()->getScrollOffset());
  mapRectUpToDocument(m_targetRect, targetLayoutObject, targetDocument);
  m_targetRect.move(-scrollPosition);
}

void IntersectionGeometry::mapRootRectToRootFrameCoordinates() {
  LayoutObject& rootLayoutObject = *getRootLayoutObject();
  Document& rootDocument = rootLayoutObject.document();
  LayoutSize scrollPosition =
      LayoutSize(rootDocument.view()->getScrollOffset());
  mapRectUpToDocument(m_rootRect, rootLayoutObject,
                      rootLayoutObject.document());
  m_rootRect.move(-scrollPosition);
}

void IntersectionGeometry::mapRootRectToTargetFrameCoordinates() {
  LayoutObject& rootLayoutObject = *getRootLayoutObject();
  Document& targetDocument = m_target->document();
  LayoutSize scrollPosition =
      LayoutSize(targetDocument.view()->getScrollOffset());

  if (&targetDocument == &rootLayoutObject.document()) {
    mapRectUpToDocument(m_intersectionRect, rootLayoutObject, targetDocument);
  } else {
    mapRectDownToDocument(m_intersectionRect,
                          toLayoutBoxModelObject(rootLayoutObject),
                          targetDocument);
  }

  m_intersectionRect.move(-scrollPosition);
}

void IntersectionGeometry::computeGeometry() {
  // In the first few lines here, before initializeGeometry is called, "return
  // true" effectively means "if the previous observed state was that root and
  // target were intersecting, then generate a notification indicating that they
  // are no longer intersecting."  This happens, for example, when root or
  // target is removed from the DOM tree and not reinserted before the next
  // frame is generated, or display:none is set on the root or target.
  if (!m_target->isConnected())
    return;
  Element* rootElement = root();
  if (rootElement && !rootElement->isConnected())
    return;

  LayoutObject* rootLayoutObject = getRootLayoutObject();
  if (!rootLayoutObject || !rootLayoutObject->isBoxModelObject())
    return;
  // TODO(szager): Support SVG
  LayoutObject* targetLayoutObject = m_target->layoutObject();
  if (!targetLayoutObject)
    return;
  if (!targetLayoutObject->isBoxModelObject() && !targetLayoutObject->isText())
    return;
  if (!isContainingBlockChainDescendant(targetLayoutObject, rootLayoutObject))
    return;

  initializeGeometry();

  clipToRoot();

  mapTargetRectToTargetFrameCoordinates();

  if (m_doesIntersect)
    mapRootRectToTargetFrameCoordinates();
  else
    m_intersectionRect = LayoutRect();

  // Small optimization: if we're not going to report root bounds, don't bother
  // transforming them to the frame.
  if (m_shouldReportRootBounds == ReportRootBounds::kShouldReportRootBounds)
    mapRootRectToRootFrameCoordinates();
}

DEFINE_TRACE(IntersectionGeometry) {
  visitor->trace(m_root);
  visitor->trace(m_target);
}

}  // namespace blink
