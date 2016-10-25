/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#include "core/layout/LayoutFieldset.h"

#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/html/HTMLLegendElement.h"
#include "core/paint/FieldsetPainter.h"

using namespace std;

namespace blink {

using namespace HTMLNames;

namespace {

void setInnerBlockPadding(bool isHorizontalWritingMode,
                          const LayoutObject* innerBlock,
                          const LayoutUnit& padding) {
  if (isHorizontalWritingMode)
    innerBlock->mutableStyleRef().setPaddingTop(Length(padding, Fixed));
  else
    innerBlock->mutableStyleRef().setPaddingLeft(Length(padding, Fixed));
}

void resetInnerBlockPadding(bool isHorizontalWritingMode,
                            const LayoutObject* innerBlock) {
  if (isHorizontalWritingMode)
    innerBlock->mutableStyleRef().setPaddingTop(Length(0, Fixed));
  else
    innerBlock->mutableStyleRef().setPaddingLeft(Length(0, Fixed));
}

}  // namespace

LayoutFieldset::LayoutFieldset(Element* element)
    : LayoutFlexibleBox(element), m_innerBlock(nullptr) {}

int LayoutFieldset::baselinePosition(FontBaseline baseline,
                                     bool firstLine,
                                     LineDirectionMode direction,
                                     LinePositionMode position) const {
  return LayoutBlock::baselinePosition(baseline, firstLine, direction,
                                       position);
}

void LayoutFieldset::computeIntrinsicLogicalWidths(
    LayoutUnit& minLogicalWidth,
    LayoutUnit& maxLogicalWidth) const {
  for (LayoutBox* child = firstChildBox(); child;
       child = child->nextSiblingBox()) {
    if (child->isOutOfFlowPositioned())
      continue;

    LayoutUnit margin = marginIntrinsicLogicalWidthForChild(*child);

    LayoutUnit minPreferredLogicalWidth;
    LayoutUnit maxPreferredLogicalWidth;
    computeChildPreferredLogicalWidths(*child, minPreferredLogicalWidth,
                                       maxPreferredLogicalWidth);
    DCHECK_GE(minPreferredLogicalWidth, LayoutUnit());
    DCHECK_GE(maxPreferredLogicalWidth, LayoutUnit());
    minPreferredLogicalWidth += margin;
    maxPreferredLogicalWidth += margin;
    minLogicalWidth = std::max(minPreferredLogicalWidth, minLogicalWidth);
    maxLogicalWidth = std::max(maxPreferredLogicalWidth, maxLogicalWidth);
  }

  maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

  // Due to negative margins, it is possible that we calculated a negative
  // intrinsic width. Make sure that we never return a negative width.
  minLogicalWidth = std::max(LayoutUnit(), minLogicalWidth);
  maxLogicalWidth = std::max(LayoutUnit(), maxLogicalWidth);

  LayoutUnit scrollbarWidth(scrollbarLogicalWidth());
  maxLogicalWidth += scrollbarWidth;
  minLogicalWidth += scrollbarWidth;
}

void LayoutFieldset::setLogicalLeftForChild(LayoutBox& child,
                                            LayoutUnit logicalLeft) {
  if (isHorizontalWritingMode()) {
    child.setX(logicalLeft);
  } else {
    child.setY(logicalLeft);
  }
}

void LayoutFieldset::setLogicalTopForChild(LayoutBox& child,
                                           LayoutUnit logicalTop) {
  if (isHorizontalWritingMode()) {
    child.setY(logicalTop);
  } else {
    child.setX(logicalTop);
  }
}

LayoutObject* LayoutFieldset::layoutSpecialExcludedChild(bool relayoutChildren,
                                                         SubtreeLayoutScope&) {
  LayoutBox* legend = findInFlowLegend();
  if (legend) {
    LayoutRect oldLegendFrameRect = legend->frameRect();

    if (relayoutChildren)
      legend->setNeedsLayoutAndFullPaintInvalidation(
          LayoutInvalidationReason::FieldsetChanged);
    legend->layoutIfNeeded();

    LayoutUnit logicalLeft;
    if (style()->isLeftToRightDirection()) {
      switch (legend->style()->textAlign()) {
        case CENTER:
          logicalLeft = (logicalWidth() - logicalWidthForChild(*legend)) / 2;
          break;
        case RIGHT:
          logicalLeft = logicalWidth() - borderEnd() - paddingEnd() -
                        logicalWidthForChild(*legend);
          break;
        default:
          logicalLeft =
              borderStart() + paddingStart() + marginStartForChild(*legend);
          break;
      }
    } else {
      switch (legend->style()->textAlign()) {
        case LEFT:
          logicalLeft = borderStart() + paddingStart();
          break;
        case CENTER: {
          // Make sure that the extra pixel goes to the end side in RTL (since
          // it went to the end side in LTR).
          LayoutUnit centeredWidth =
              logicalWidth() - logicalWidthForChild(*legend);
          logicalLeft = centeredWidth - centeredWidth / 2;
          break;
        }
        default:
          logicalLeft = logicalWidth() - borderStart() - paddingStart() -
                        marginStartForChild(*legend) -
                        logicalWidthForChild(*legend);
          break;
      }
    }

    setLogicalLeftForChild(*legend, logicalLeft);

    LayoutUnit fieldsetBorderBefore = LayoutUnit(borderBefore());
    LayoutUnit legendLogicalHeight = logicalHeightForChild(*legend);

    LayoutUnit legendLogicalTop;
    LayoutUnit collapsedLegendExtent;
    LayoutUnit innerBlockPadding;

    if (legendLogicalHeight < fieldsetBorderBefore) {
      // Center legend in fieldset border
      legendLogicalTop = (fieldsetBorderBefore - legendLogicalHeight) / 2;
    }

    // Calculate how much legend + bottom margin sticks below the fieldset
    // border
    innerBlockPadding = (legendLogicalTop + legendLogicalHeight +
                         marginAfterForChild(*legend) - fieldsetBorderBefore)
                            .clampNegativeToZero();

    if (legendLogicalTop < marginBeforeForChild(*legend)) {
      // legend margin pushes everything down
      innerBlockPadding += marginBeforeForChild(*legend) - legendLogicalTop;
      legendLogicalTop = marginBeforeForChild(*legend);
    }

    collapsedLegendExtent =
        std::max(fieldsetBorderBefore, marginBeforeForChild(*legend) +
                                           legendLogicalHeight +
                                           marginAfterForChild(*legend));

    if (m_innerBlock)
      setInnerBlockPadding(isHorizontalWritingMode(), m_innerBlock,
                           innerBlockPadding);
    setLogicalTopForChild(*legend, legendLogicalTop);
    setLogicalHeight(paddingBefore() + collapsedLegendExtent);

    if (legend->frameRect() != oldLegendFrameRect) {
      // We need to invalidate the fieldset border if the legend's frame
      // changed.
      setShouldDoFullPaintInvalidation();
      if (m_innerBlock)
        m_innerBlock->setNeedsLayout(LayoutInvalidationReason::FieldsetChanged,
                                     MarkOnlyThis);
    }
  }
  return legend;
}

LayoutBox* LayoutFieldset::findInFlowLegend() const {
  for (LayoutObject* legend = firstChild(); legend;
       legend = legend->nextSibling()) {
    if (legend->isFloatingOrOutOfFlowPositioned())
      continue;

    if (isHTMLLegendElement(legend->node()))
      return toLayoutBox(legend);
  }
  return nullptr;
}

void LayoutFieldset::paintBoxDecorationBackground(
    const PaintInfo& paintInfo,
    const LayoutPoint& paintOffset) const {
  FieldsetPainter(*this).paintBoxDecorationBackground(paintInfo, paintOffset);
}

void LayoutFieldset::paintMask(const PaintInfo& paintInfo,
                               const LayoutPoint& paintOffset) const {
  FieldsetPainter(*this).paintMask(paintInfo, paintOffset);
}

void LayoutFieldset::updateAnonymousChildStyle(
    const LayoutObject& child,
    ComputedStyle& childStyle) const {
  childStyle.setFlexShrink(1.0f);
  childStyle.setFlexGrow(1.0f);
  // min-width: 0; is needed for correct shrinking.
  childStyle.setMinWidth(Length(0, Fixed));
  childStyle.setFlexDirection(style()->flexDirection());
  childStyle.setJustifyContent(style()->justifyContent());
  childStyle.setFlexWrap(style()->flexWrap());
  childStyle.setAlignItems(style()->alignItems());
  childStyle.setAlignContent(style()->alignContent());
  // Let anonymous block to be the 1st for correct layout positioning.
  childStyle.setOrder(1);
}

void LayoutFieldset::addChild(LayoutObject* newChild,
                              LayoutObject* beforeChild) {
  if (!m_innerBlock)
    createInnerBlock();

  if (isHTMLLegendElement(newChild->node())) {
    // Let legend block to be the 2nd for correct layout positioning.
    newChild->mutableStyle()->setOrder(2);
    LayoutFlexibleBox::addChild(newChild, m_innerBlock);
  } else {
    if (beforeChild && isHTMLLegendElement(beforeChild->node())) {
      m_innerBlock->addChild(newChild);
    } else {
      m_innerBlock->addChild(newChild, beforeChild);
    }
    if (AXObjectCache* cache = document().existingAXObjectCache())
      cache->childrenChanged(this);
  }
}

void LayoutFieldset::createInnerBlock() {
  if (m_innerBlock) {
    DCHECK(firstChild() == m_innerBlock);
    return;
  }
  m_innerBlock = createAnonymousBlock(style()->display());
  LayoutFlexibleBox::addChild(m_innerBlock);
}

void LayoutFieldset::removeChild(LayoutObject* oldChild) {
  if (isHTMLLegendElement(oldChild->node())) {
    LayoutFlexibleBox::removeChild(oldChild);
    if (m_innerBlock) {
      resetInnerBlockPadding(isHorizontalWritingMode(), m_innerBlock);
      m_innerBlock->setNeedsLayout(LayoutInvalidationReason::FieldsetChanged,
                                   MarkOnlyThis);
    }
    setShouldDoFullPaintInvalidation();
  } else if (oldChild == m_innerBlock) {
    LayoutFlexibleBox::removeChild(oldChild);
    m_innerBlock = nullptr;
  } else if (oldChild->parent() == this) {
    LayoutFlexibleBox::removeChild(oldChild);
  } else if (m_innerBlock) {
    m_innerBlock->removeChild(oldChild);
  }
}

}  // namespace blink
