// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutItem_h
#define LineLayoutItem_h

#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutObjectInlines.h"

#include "platform/LayoutUnit.h"
#include "wtf/Allocator.h"

namespace blink {

class ComputedStyle;
class Document;
class HitTestRequest;
class HitTestLocation;
class LayoutObject;
class LineLayoutBox;
class LineLayoutBoxModel;
class LineLayoutAPIShim;

enum HitTestFilter;

class LineLayoutItem {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    explicit LineLayoutItem(LayoutObject* layoutObject)
        : m_layoutObject(layoutObject)
    {
    }

    LineLayoutItem(std::nullptr_t)
        : m_layoutObject(0)
    {
    }

    LineLayoutItem() : m_layoutObject(0) { }

    // TODO(pilgrim): Remove this. It's only here to make things compile before
    // switching all of core/layout/line to using the API.
    // https://crbug.com/499321
    operator LayoutObject*() const { return m_layoutObject; }

    bool isEqual(const LayoutObject* layoutObject) const
    {
        return m_layoutObject == layoutObject;
    }

    String debugName() const
    {
        return m_layoutObject->debugName();
    }

    bool needsLayout() const
    {
        return m_layoutObject->needsLayout();
    }

    Node* node() const
    {
        return m_layoutObject->node();
    }

    Node* nonPseudoNode() const
    {
        return m_layoutObject->nonPseudoNode();
    }

    LineLayoutItem parent() const
    {
        return LineLayoutItem(m_layoutObject->parent());
    }

    // Implemented in LineLayoutBox.h
    // Intentionally returns a LineLayoutBox to avoid exposing LayoutBlock
    // to the line layout code.
    LineLayoutBox containingBlock() const;

    // Implemented in LineLayoutBoxModel.h
    // Intentionally returns a LineLayoutBoxModel to avoid exposing LayoutBoxModelObject
    // to the line layout code.
    LineLayoutBoxModel enclosingBoxModelObject() const;

    LineLayoutItem container() const
    {
        return LineLayoutItem(m_layoutObject->container());
    }

    bool isDescendantOf(const LineLayoutItem item) const
    {
        return m_layoutObject->isDescendantOf(item);
    }

    void updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
    {
        return m_layoutObject->updateHitTestResult(result, point);
    }

    LineLayoutItem nextSibling() const
    {
        return LineLayoutItem(m_layoutObject->nextSibling());
    }

    LineLayoutItem previousSibling() const
    {
        return LineLayoutItem(m_layoutObject->previousSibling());
    }

    LineLayoutItem slowFirstChild() const
    {
        return LineLayoutItem(m_layoutObject->slowFirstChild());
    }

    LineLayoutItem slowLastChild() const
    {
        return LineLayoutItem(m_layoutObject->slowLastChild());
    }

    const ComputedStyle* style() const
    {
        return m_layoutObject->style();
    }

    const ComputedStyle& styleRef() const
    {
        return m_layoutObject->styleRef();
    }

    const ComputedStyle* style(bool firstLine) const
    {
        return m_layoutObject->style(firstLine);
    }

    const ComputedStyle& styleRef(bool firstLine) const
    {
        return m_layoutObject->styleRef(firstLine);
    }

    Document& document() const
    {
        return m_layoutObject->document();
    }

    bool preservesNewline() const
    {
        return m_layoutObject->preservesNewline();
    }

    unsigned length() const
    {
        return m_layoutObject->length();
    }

    void dirtyLinesFromChangedChild(LineLayoutItem item) const
    {
        m_layoutObject->dirtyLinesFromChangedChild(item.layoutObject());
    }

    bool ancestorLineBoxDirty() const
    {
        return m_layoutObject->ancestorLineBoxDirty();
    }

    bool isFloatingOrOutOfFlowPositioned() const
    {
        return m_layoutObject->isFloatingOrOutOfFlowPositioned();
    }

    bool isFloating() const
    {
        return m_layoutObject->isFloating();
    }

    bool isOutOfFlowPositioned() const
    {
        return m_layoutObject->isOutOfFlowPositioned();
    }

    bool isBox() const
    {
        return m_layoutObject->isBox();
    }

    bool isBoxModelObject() const
    {
        return m_layoutObject->isBoxModelObject();
    }

    bool isBR() const
    {
        return m_layoutObject->isBR();
    }

    bool isCombineText() const
    {
        return m_layoutObject->isCombineText();
    }

    bool isHorizontalWritingMode() const
    {
        return m_layoutObject->isHorizontalWritingMode();
    }

    bool isImage() const
    {
        return m_layoutObject->isImage();
    }

    bool isInline() const
    {
        return m_layoutObject->isInline();
    }

    bool isInlineBlockOrInlineTable() const
    {
        return m_layoutObject->isInlineBlockOrInlineTable();
    }

    bool isInlineElementContinuation() const
    {
        return m_layoutObject->isInlineElementContinuation();
    }

    bool isLayoutBlock() const
    {
        return m_layoutObject->isLayoutBlock();
    }

    bool isLayoutBlockFlow() const
    {
        return m_layoutObject->isLayoutBlockFlow();
    }

    bool isLayoutInline() const
    {
        return m_layoutObject->isLayoutInline();
    }

    bool isListMarker() const
    {
        return m_layoutObject->isListMarker();
    }

    bool isAtomicInlineLevel() const
    {
        return m_layoutObject->isAtomicInlineLevel();
    }

    bool isRubyRun() const
    {
        return m_layoutObject->isRubyRun();
    }

    bool isRubyBase() const
    {
        return m_layoutObject->isRubyBase();
    }

    bool isSVGInlineText() const
    {
        return m_layoutObject->isSVGInlineText();
    }

    bool isTableCell() const
    {
        return m_layoutObject->isTableCell();
    }

    bool isText() const
    {
        return m_layoutObject->isText();
    }

    bool hasLayer() const
    {
        return m_layoutObject->hasLayer();
    }

    bool selfNeedsLayout() const
    {
        return m_layoutObject->selfNeedsLayout();
    }

    void setAncestorLineBoxDirty() const
    {
        m_layoutObject->setAncestorLineBoxDirty();
    }

    int caretMinOffset() const
    {
        return m_layoutObject->caretMinOffset();
    }

    int caretMaxOffset() const
    {
        return m_layoutObject->caretMaxOffset();
    }

    bool hasFlippedBlocksWritingMode() const
    {
        return m_layoutObject->hasFlippedBlocksWritingMode();
    }

    bool visibleToHitTestRequest(const HitTestRequest& request) const
    {
        return m_layoutObject->visibleToHitTestRequest(request);
    }

    bool hitTest(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestFilter filter = HitTestAll)
    {
        return m_layoutObject->hitTest(result, locationInContainer, accumulatedOffset, filter);
    }

    SelectionState selectionState() const
    {
        return m_layoutObject->selectionState();
    }

    Color selectionBackgroundColor() const
    {
        return m_layoutObject->selectionBackgroundColor();
    }

    Color resolveColor(const ComputedStyle& styleToUse, int colorProperty)
    {
        return m_layoutObject->resolveColor(styleToUse, colorProperty);
    }

    bool isInFlowPositioned() const
    {
        return m_layoutObject->isInFlowPositioned();
    }

    PositionWithAffinity positionForPoint(const LayoutPoint& point)
    {
        return m_layoutObject->positionForPoint(point);
    }

    PositionWithAffinity createPositionWithAffinity(int offset, TextAffinity affinity)
    {
        return m_layoutObject->createPositionWithAffinity(offset, affinity);
    }

    LineLayoutItem previousInPreOrder(const LayoutObject* stayWithin) const
    {
        return LineLayoutItem(m_layoutObject->previousInPreOrder(stayWithin));
    }

    FloatQuad localToAbsoluteQuad(const FloatQuad& quad, MapCoordinatesFlags mode = 0, bool* wasFixed = nullptr) const
    {
        return m_layoutObject->localToAbsoluteQuad(quad, mode, wasFixed);
    }

    int previousOffset(int current) const
    {
        return m_layoutObject->previousOffset(current);
    }

    int nextOffset(int current) const
    {
        return m_layoutObject->nextOffset(current);
    }

#ifndef NDEBUG

    const char* name() const
    {
        return m_layoutObject->name();
    }

    // Intentionally returns a void* to avoid exposing LayoutObject* to the line
    // layout code.
    void* debugPointer() const
    {
        return m_layoutObject;
    }

#endif

protected:
    LayoutObject* layoutObject() { return m_layoutObject; }
    const LayoutObject* layoutObject() const { return m_layoutObject; }

private:
    LayoutObject* m_layoutObject;

    friend class LineLayoutAPIShim;
};

} // namespace blink

#endif // LineLayoutItem_h
