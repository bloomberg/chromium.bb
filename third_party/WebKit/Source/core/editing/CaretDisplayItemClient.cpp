/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/CaretDisplayItemClient.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/VisibleUnits.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutBlockItem.h"
#include "core/layout/api/LayoutItem.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

CaretDisplayItemClient::CaretDisplayItemClient() = default;
CaretDisplayItemClient::~CaretDisplayItemClient() = default;

static inline bool caretRendersInsideNode(const Node* node) {
  return node && !isDisplayInsideTable(node) && !editingIgnoresContent(*node);
}

LayoutBlock* CaretDisplayItemClient::caretLayoutBlock(const Node* node) {
  if (!node)
    return nullptr;

  LayoutObject* layoutObject = node->layoutObject();
  if (!layoutObject)
    return nullptr;

  // if caretNode is a block and caret is inside it then caret should be painted
  // by that block
  bool paintedByBlock =
      layoutObject->isLayoutBlock() && caretRendersInsideNode(node);
  // TODO(yoichio): This function is called at least
  // DocumentLifeCycle::LayoutClean but caretRendersInsideNode above can
  // layout. Thus |node->layoutObject()| can be changed then this is bad
  // design. We should make caret painting algorithm clean.
  CHECK_EQ(layoutObject, node->layoutObject())
      << "Layout tree should not changed";
  return paintedByBlock ? toLayoutBlock(layoutObject)
                        : layoutObject->containingBlock();
}

static LayoutRect mapCaretRectToCaretPainter(
    LayoutItem caretLayoutItem,
    LayoutBlockItem caretPainterItem,
    const LayoutRect& passedCaretRect) {
  // FIXME: This shouldn't be called on un-rooted subtrees.
  // FIXME: This should probably just use mapLocalToAncestor.
  // Compute an offset between the caretLayoutItem and the caretPainterItem.

  DCHECK(caretLayoutItem.isDescendantOf(caretPainterItem));

  LayoutRect caretRect = passedCaretRect;
  while (caretLayoutItem != caretPainterItem) {
    LayoutItem containerItem = caretLayoutItem.container();
    if (containerItem.isNull())
      return LayoutRect();
    caretRect.move(caretLayoutItem.offsetFromContainer(containerItem));
    caretLayoutItem = containerItem;
  }
  return caretRect;
}

LayoutRect CaretDisplayItemClient::computeCaretRect(
    const PositionWithAffinity& caretPosition) {
  if (caretPosition.isNull())
    return LayoutRect();

  DCHECK(caretPosition.anchorNode()->layoutObject());

  // First compute a rect local to the layoutObject at the selection start.
  LayoutObject* layoutObject;
  const LayoutRect& caretLocalRect =
      localCaretRectOfPosition(caretPosition, layoutObject);

  // Get the layoutObject that will be responsible for painting the caret
  // (which is either the layoutObject we just found, or one of its containers).
  LayoutBlockItem caretPainterItem =
      LayoutBlockItem(caretLayoutBlock(caretPosition.anchorNode()));
  LayoutRect caretLocalRectWithWritingMode = caretLocalRect;
  caretPainterItem.flipForWritingMode(caretLocalRectWithWritingMode);
  return mapCaretRectToCaretPainter(LayoutItem(layoutObject), caretPainterItem,
                                    caretLocalRectWithWritingMode);
}

void CaretDisplayItemClient::clearPreviousVisualRect(const LayoutBlock& block) {
  if (block == m_layoutBlock) {
    m_visualRect = LayoutRect();
    m_localRect = LayoutRect();
  }
  if (block == m_previousLayoutBlock)
    m_visualRectInPreviousLayoutBlock = LayoutRect();
}

void CaretDisplayItemClient::layoutBlockWillBeDestroyed(
    const LayoutBlock& block) {
  if (block == m_layoutBlock)
    m_layoutBlock = nullptr;
  if (block == m_previousLayoutBlock)
    m_previousLayoutBlock = nullptr;
}

void CaretDisplayItemClient::updateStyleAndLayoutIfNeeded(
    const PositionWithAffinity& caretPosition) {
  // This method may be called multiple times (e.g. in partial lifecycle
  // updates) before a paint invalidation. We should save m_previousLayoutBlock
  // and m_visualRectInPreviousLayoutBlock only if they have not been saved
  // since the last paint invalidation to ensure the caret painted in the
  // previous paint invalidated block will be invalidated. We don't care about
  // intermediate changes of layoutBlock because they are not painted.
  if (!m_previousLayoutBlock) {
    m_previousLayoutBlock = m_layoutBlock;
    m_visualRectInPreviousLayoutBlock = m_visualRect;
  }

  LayoutBlock* newLayoutBlock = caretLayoutBlock(caretPosition.anchorNode());
  if (newLayoutBlock != m_layoutBlock) {
    if (m_layoutBlock)
      m_layoutBlock->setMayNeedPaintInvalidation();
    m_layoutBlock = newLayoutBlock;
    m_visualRect = LayoutRect();
    if (newLayoutBlock) {
      m_needsPaintInvalidation = true;
      if (newLayoutBlock == m_previousLayoutBlock) {
        // The caret has disappeared and is reappearing in the same block,
        // since the last paint invalidation. Set m_visualRect as if the caret
        // has always been there as paint invalidation doesn't care about the
        // intermediate changes.
        m_visualRect = m_visualRectInPreviousLayoutBlock;
      }
    }
  }

  if (!newLayoutBlock) {
    m_color = Color();
    m_localRect = LayoutRect();
    return;
  }

  Color newColor;
  if (caretPosition.anchorNode()) {
    newColor = caretPosition.anchorNode()->layoutObject()->resolveColor(
        CSSPropertyCaretColor);
  }
  if (newColor != m_color) {
    m_needsPaintInvalidation = true;
    m_color = newColor;
  }

  LayoutRect newLocalRect = computeCaretRect(caretPosition);
  if (newLocalRect != m_localRect) {
    m_needsPaintInvalidation = true;
    m_localRect = newLocalRect;
  }

  if (m_needsPaintInvalidation)
    newLayoutBlock->setMayNeedPaintInvalidation();
}

void CaretDisplayItemClient::invalidatePaintIfNeeded(
    const LayoutBlock& block,
    const PaintInvalidatorContext& context) {
  if (block == m_layoutBlock) {
    invalidatePaintInCurrentLayoutBlock(context);
    return;
  }

  if (block == m_previousLayoutBlock)
    invalidatePaintInPreviousLayoutBlock(context);
}

void CaretDisplayItemClient::invalidatePaintInPreviousLayoutBlock(
    const PaintInvalidatorContext& context) {
  DCHECK(m_previousLayoutBlock);

  ObjectPaintInvalidatorWithContext objectInvalidator(*m_previousLayoutBlock,
                                                      context);
  if (!isImmediateFullPaintInvalidationReason(
          m_previousLayoutBlock->fullPaintInvalidationReason())) {
    objectInvalidator.invalidatePaintRectangleWithContext(
        m_visualRectInPreviousLayoutBlock, PaintInvalidationCaret);
  }

  context.paintingLayer->setNeedsRepaint();
  objectInvalidator.invalidateDisplayItemClient(*this, PaintInvalidationCaret);
  m_previousLayoutBlock = nullptr;
}

void CaretDisplayItemClient::invalidatePaintInCurrentLayoutBlock(
    const PaintInvalidatorContext& context) {
  DCHECK(m_layoutBlock);

  LayoutRect newVisualRect;
  if (!m_localRect.isEmpty()) {
    newVisualRect = m_localRect;
    context.mapLocalRectToVisualRectInBacking(*m_layoutBlock, newVisualRect);

    if (m_layoutBlock->usesCompositedScrolling()) {
      // The caret should use scrolling coordinate space.
      DCHECK(m_layoutBlock == context.paintInvalidationContainer);
      newVisualRect.move(LayoutSize(m_layoutBlock->scrolledContentOffset()));
    }
  }

  if (m_layoutBlock == m_previousLayoutBlock)
    m_previousLayoutBlock = nullptr;

  ObjectPaintInvalidatorWithContext objectInvalidator(*m_layoutBlock, context);
  if (!m_needsPaintInvalidation && newVisualRect == m_visualRect) {
    // The caret may change paint offset without changing visual rect, and we
    // need to invalidate the display item client if the block is doing full
    // paint invalidation.
    if (isImmediateFullPaintInvalidationReason(
            m_layoutBlock->fullPaintInvalidationReason()) ||
        // For non-SPv2, ForcedSubtreeInvalidationChecking may hint change of
        // paint offset. See ObjectPaintInvalidatorWithContext::
        // invalidatePaintIfNeededWithComputedReason().
        (!RuntimeEnabledFeatures::slimmingPaintV2Enabled() &&
         (context.forcedSubtreeInvalidationFlags &
          PaintInvalidatorContext::ForcedSubtreeInvalidationChecking))) {
      objectInvalidator.invalidateDisplayItemClient(*this,
                                                    PaintInvalidationCaret);
    }
    return;
  }

  m_needsPaintInvalidation = false;

  if (!isImmediateFullPaintInvalidationReason(
          m_layoutBlock->fullPaintInvalidationReason())) {
    objectInvalidator.fullyInvalidatePaint(PaintInvalidationCaret, m_visualRect,
                                           newVisualRect);
  }

  context.paintingLayer->setNeedsRepaint();
  objectInvalidator.invalidateDisplayItemClient(*this, PaintInvalidationCaret);

  m_visualRect = newVisualRect;
}

void CaretDisplayItemClient::paintCaret(
    GraphicsContext& context,
    const LayoutPoint& paintOffset,
    DisplayItem::Type displayItemType) const {
  if (DrawingRecorder::useCachedDrawingIfPossible(context, *this,
                                                  displayItemType))
    return;

  LayoutRect drawingRect = m_localRect;
  drawingRect.moveBy(paintOffset);

  IntRect paintRect = pixelSnappedIntRect(drawingRect);
  DrawingRecorder drawingRecorder(context, *this, displayItemType, paintRect);
  context.fillRect(paintRect, m_color);
}

String CaretDisplayItemClient::debugName() const {
  return "Caret";
}

LayoutRect CaretDisplayItemClient::visualRect() const {
  return m_visualRect;
}

}  // namespace blink
