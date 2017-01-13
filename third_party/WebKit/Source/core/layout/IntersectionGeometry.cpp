// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/IntersectionGeometry.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
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
  if (ancestorFrame != descendantFrame)
    return false;

  while (descendant && descendant != ancestor)
    descendant = descendant->containingBlock();
  return descendant;
}

void mapRectUpToDocument(LayoutRect& rect,
                         const LayoutObject& descendant,
                         const Document& document) {
  FloatQuad mappedQuad = descendant.localToAncestorQuad(
      FloatQuad(FloatRect(rect)), document.layoutView(),
      UseTransforms | ApplyContainerFlip);
  rect = LayoutRect(mappedQuad.boundingBox());
}

void mapRectDownToDocument(LayoutRect& rect,
                           LayoutBoxModelObject* ancestor,
                           const Document& document) {
  FloatQuad mappedQuad = document.layoutView()->ancestorToLocalQuad(
      ancestor, FloatQuad(FloatRect(rect)),
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

LayoutView* localRootView(Element& element) {
  LocalFrame* frame = element.document().frame();
  LocalFrame* frameRoot = frame ? frame->localFrameRoot() : nullptr;
  return frameRoot ? frameRoot->contentLayoutObject() : nullptr;
}

}  // namespace

IntersectionGeometry::IntersectionGeometry(Element* root,
                                           Element& target,
                                           const Vector<Length>& rootMargin,
                                           bool shouldReportRootBounds)
    : m_root(root ? root->layoutObject() : localRootView(target)),
      m_target(target.layoutObject()),
      m_rootMargin(rootMargin),
      m_doesIntersect(0),
      m_shouldReportRootBounds(shouldReportRootBounds),
      m_rootIsImplicit(!root),
      m_canComputeGeometry(initializeCanComputeGeometry(root, target)) {
  if (m_canComputeGeometry)
    initializeGeometry();
}

IntersectionGeometry::~IntersectionGeometry() {}

bool IntersectionGeometry::initializeCanComputeGeometry(Element* root,
                                                        Element& target) const {
  DCHECK(m_rootMargin.isEmpty() || m_rootMargin.size() == 4);
  if (root && !root->isConnected())
    return false;
  if (!m_root || !m_root->isBox())
    return false;
  if (!target.isConnected())
    return false;
  if (!m_target || (!m_target->isBoxModelObject() && !m_target->isText()))
    return false;
  if (root && !isContainingBlockChainDescendant(m_target, m_root))
    return false;
  return true;
}

void IntersectionGeometry::initializeGeometry() {
  initializeTargetRect();
  m_intersectionRect = m_targetRect;
  initializeRootRect();
}

void IntersectionGeometry::initializeTargetRect() {
  m_targetRect =
      LayoutRect(toLayoutBoxModelObject(target())->borderBoundingBox());
}

void IntersectionGeometry::initializeRootRect() {
  if (m_root->isLayoutView()) {
    m_rootRect =
        LayoutRect(toLayoutView(m_root)->frameView()->visibleContentRect());
    m_root->mapToVisualRectInAncestorSpace(nullptr, m_rootRect);
  } else if (m_root->isBox() && m_root->hasOverflowClip()) {
    m_rootRect = LayoutRect(toLayoutBox(m_root)->contentBoxRect());
  } else {
    m_rootRect =
        LayoutRect(toLayoutBoxModelObject(m_root)->borderBoundingBox());
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
  LayoutBox* ancestor = toLayoutBox(m_root);
  m_doesIntersect = m_target->mapToVisualRectInAncestorSpace(
      (rootIsImplicit() ? nullptr : ancestor), m_intersectionRect,
      EdgeInclusive);
  if (ancestor && ancestor->hasOverflowClip())
    m_intersectionRect.move(-ancestor->scrolledContentOffset());
  if (!m_doesIntersect)
    return;
  LayoutRect rootClipRect(m_rootRect);
  if (ancestor)
    ancestor->flipForWritingMode(rootClipRect);
  m_doesIntersect &= m_intersectionRect.inclusiveIntersect(rootClipRect);
}

void IntersectionGeometry::mapTargetRectToTargetFrameCoordinates() {
  Document& targetDocument = m_target->document();
  LayoutSize scrollPosition =
      LayoutSize(targetDocument.view()->getScrollOffset());
  mapRectUpToDocument(m_targetRect, *m_target, targetDocument);
  m_targetRect.move(-scrollPosition);
}

void IntersectionGeometry::mapRootRectToRootFrameCoordinates() {
  m_root->frameView()->mapQuadToAncestorFrameIncludingScrollOffset(
      m_rootRect, m_root,
      rootIsImplicit() ? nullptr : m_root->document().layoutView(),
      UseTransforms | ApplyContainerFlip);
}

void IntersectionGeometry::mapIntersectionRectToTargetFrameCoordinates() {
  Document& targetDocument = m_target->document();
  if (rootIsImplicit()) {
    LocalFrame* targetFrame = targetDocument.frame();
    Frame* rootFrame = targetFrame->tree().top();
    LayoutSize scrollPosition =
        LayoutSize(targetDocument.view()->getScrollOffset());
    if (targetFrame != rootFrame)
      mapRectDownToDocument(m_intersectionRect, nullptr, targetDocument);
    m_intersectionRect.move(-scrollPosition);
  } else {
    LayoutSize scrollPosition =
        LayoutSize(targetDocument.view()->getScrollOffset());
    mapRectUpToDocument(m_intersectionRect, *m_root, m_root->document());
    m_intersectionRect.move(-scrollPosition);
  }
}

void IntersectionGeometry::computeGeometry() {
  if (!canComputeGeometry())
    return;
  clipToRoot();
  mapTargetRectToTargetFrameCoordinates();
  if (m_doesIntersect)
    mapIntersectionRectToTargetFrameCoordinates();
  else
    m_intersectionRect = LayoutRect();
  // Small optimization: if we're not going to report root bounds, don't bother
  // transforming them to the frame.
  if (shouldReportRootBounds())
    mapRootRectToRootFrameCoordinates();
}

}  // namespace blink
