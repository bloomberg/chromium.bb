// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DisplayItemPropertyTreeBuilder.h"

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DisplayItemClipTree.h"
#include "platform/graphics/paint/DisplayItemTransformTree.h"
#include "platform/graphics/paint/ScrollDisplayItem.h"
#include "platform/graphics/paint/Transform3DDisplayItem.h"
#include "platform/graphics/paint/TransformDisplayItem.h"

namespace blink {

DisplayItemPropertyTreeBuilder::DisplayItemPropertyTreeBuilder()
    : m_transformTree(adoptPtr(new DisplayItemTransformTree))
    , m_clipTree(adoptPtr(new DisplayItemClipTree))
    , m_rangeBeginIndex(0)
    , m_currentIndex(0)
{
    m_stateStack.append(BuilderState());
}

DisplayItemPropertyTreeBuilder::~DisplayItemPropertyTreeBuilder()
{
}

PassOwnPtr<DisplayItemTransformTree> DisplayItemPropertyTreeBuilder::releaseTransformTree()
{
    ASSERT(m_stateStack.size() == 1);
    ASSERT(currentState().ignoredBeginCount == 0);
    finishRange();
    return m_transformTree.release();
}

PassOwnPtr<DisplayItemClipTree> DisplayItemPropertyTreeBuilder::releaseClipTree()
{
    ASSERT(m_stateStack.size() == 1);
    ASSERT(currentState().ignoredBeginCount == 0);
    finishRange();
    return m_clipTree.release();
}

Vector<DisplayItemPropertyTreeBuilder::RangeRecord> DisplayItemPropertyTreeBuilder::releaseRangeRecords()
{
    Vector<RangeRecord> output;
    output.swap(m_rangeRecords);
    return output;
}

void DisplayItemPropertyTreeBuilder::processDisplayItem(const DisplayItem& displayItem)
{
    if (displayItem.isBegin())
        processBeginItem(displayItem);
    else if (displayItem.isEnd())
        processEndItem(displayItem);
    m_currentIndex++;
}

namespace {

enum TransformKind { NotATransform = 0, Only2DTranslation, RequiresTransformNode };
enum ClipKind { NotAClip = 0, RequiresClipNode };

struct BeginDisplayItemClassification {
    // |transformOrigin| is irrelevant unless a non-translation matrix is
    // provided.
    TransformKind transformKind = NotATransform;
    TransformationMatrix matrix;
    FloatPoint3D transformOrigin;

    ClipKind clipKind = NotAClip;
    FloatRect clipRect;
};

// Classifies a begin display item based on how it affects the property trees.
static BeginDisplayItemClassification classifyBeginItem(const DisplayItem& beginDisplayItem)
{
    ASSERT(beginDisplayItem.isBegin());

    BeginDisplayItemClassification result;
    DisplayItem::Type type = beginDisplayItem.type();
    if (DisplayItem::isTransform3DType(type)) {
        const auto& begin3D = static_cast<const BeginTransform3DDisplayItem&>(beginDisplayItem);
        result.matrix = begin3D.transform();
        result.transformOrigin = begin3D.transformOrigin();
        result.transformKind = result.matrix.isIdentityOr2DTranslation() ? Only2DTranslation : RequiresTransformNode;
    } else if (type == DisplayItem::BeginTransform) {
        const auto& begin2D = static_cast<const BeginTransformDisplayItem&>(beginDisplayItem);
        result.matrix = begin2D.transform();
        result.transformKind = begin2D.transform().isIdentityOrTranslation() ? Only2DTranslation : RequiresTransformNode;
    } else if (DisplayItem::isScrollType(type)) {
        const auto& beginScroll = static_cast<const BeginScrollDisplayItem&>(beginDisplayItem);
        const IntSize& offset = beginScroll.currentOffset();
        result.matrix.translate(-offset.width(), -offset.height());
        result.transformKind = Only2DTranslation;
    }
    return result;
}

} // namespace

void DisplayItemPropertyTreeBuilder::processBeginItem(const DisplayItem& displayItem)
{
    BeginDisplayItemClassification classification = classifyBeginItem(displayItem);

    if (!classification.transformKind && !classification.clipKind) {
        currentState().ignoredBeginCount++;
        return;
    }

    finishRange();
    BuilderState newState(currentState());
    newState.ignoredBeginCount = 0;

    switch (classification.transformKind) {
    case NotATransform:
        break;
    case Only2DTranslation:
        // Adjust the offset associated with the current transform node.
        newState.offset += classification.matrix.to2DTranslation();
        break;
    case RequiresTransformNode:
        // Emit a transform node.
        newState.transformNode = m_transformTree->createNewNode(
            newState.transformNode, classification.matrix, classification.transformOrigin);
        newState.offset = FloatSize();
        break;
    }

    switch (classification.clipKind) {
    case NotAClip:
        break;
    case RequiresClipNode:
        // Emit a clip node.
        FloatRect adjustedClipRect = classification.clipRect;
        adjustedClipRect.move(newState.offset);
        newState.clipNode = m_clipTree->createNewNode(
            newState.clipNode, newState.transformNode, adjustedClipRect);
        break;
    }

    m_stateStack.append(newState);
}

void DisplayItemPropertyTreeBuilder::processEndItem(const DisplayItem& displayItem)
{
    if (currentState().ignoredBeginCount) {
        // Ignored this end display item.
        currentState().ignoredBeginCount--;
    } else {
        // We've closed the scope of some property.
        finishRange();
        m_stateStack.removeLast();
        ASSERT(!m_stateStack.isEmpty());
    }
}

void DisplayItemPropertyTreeBuilder::finishRange()
{
    // Don't emit an empty range record.
    if (m_rangeBeginIndex < m_currentIndex) {
        const auto& current = currentState();
        RangeRecord rangeRecord;
        rangeRecord.displayListBeginIndex = m_rangeBeginIndex;
        rangeRecord.displayListEndIndex = m_currentIndex;
        rangeRecord.transformNodeIndex = current.transformNode;
        rangeRecord.offset = current.offset;
        rangeRecord.clipNodeIndex = current.clipNode;
        m_rangeRecords.append(rangeRecord);
    }

    // The current display item is a boundary.
    // The earliest the next range could begin is the next one.
    m_rangeBeginIndex = m_currentIndex + 1;
}

} // namespace blink
