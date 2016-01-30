/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2009, 2010 Apple Inc. All rights reserved.
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
 */

#ifndef LayoutTable_h
#define LayoutTable_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/layout/LayoutBlock.h"
#include "core/style/CollapsedBorderValue.h"
#include "wtf/Vector.h"

namespace blink {

class LayoutTableCol;
class LayoutTableCaption;
class LayoutTableCell;
class LayoutTableSection;
class TableLayoutAlgorithm;

enum SkipEmptySectionsValue { DoNotSkipEmptySections, SkipEmptySections };

// LayoutTable is the LayoutObject associated with
// display: table or inline-table.
//
// LayoutTable is the master coordinator for determining the overall table
// structure. The reason is that LayoutTableSection children have a local
// view over what their structure is but don't account for other
// LayoutTableSection. Thus LayoutTable helps keep consistency across
// LayoutTableSection. See e.g. m_column below.
//
// LayoutTable expects only 3 types of children:
// - zero or more LayoutTableCol
// - zero or more LayoutTableCaption
// - zero or more LayoutTableSection
// This is aligned with what HTML5 expects:
// https://html.spec.whatwg.org/multipage/tables.html#the-table-element
// with one difference: we allow more than one caption as we follow what
// CSS expects (https://bugs.webkit.org/show_bug.cgi?id=69773).
// Those expectations are enforced by LayoutTable::addChild, that wraps unknown
// children into an anonymous LayoutTableSection. This is what the "generate
// missing child wrapper" step in CSS mandates in
// http://www.w3.org/TR/CSS21/tables.html#anonymous-boxes.
//
// LayoutTable assumes a pretty strict structure that is mandated by CSS:
// (note that this structure in HTML is enforced by the HTML5 Parser).
//
//                 LayoutTable
//                 |        |
//  LayoutTableSection    LayoutTableCaption
//                 |
//      LayoutTableRow
//                 |
//     LayoutTableCell
//
// This means that we have to generate some anonymous table wrappers in order to
// satisfy the structure. See again
// http://www.w3.org/TR/CSS21/tables.html#anonymous-boxes.
// The anonymous table wrappers are inserted in LayoutTable::addChild,
// LayoutTableSection::addChild, LayoutTableRow::addChild and
// LayoutObject::addChild.
//
// Note that this yields to interesting issues in the insertion code. The DOM
// code is unaware of the anonymous LayoutObjects and thus can insert
// LayoutObjects into a different part of the layout tree. An example is:
//
// <!DOCTYPE html>
// <style>
// tablerow { display: table-row; }
// tablecell { display: table-cell; border: 5px solid purple; }
// </style>
// <tablerow id="firstRow">
//     <tablecell>Short first row.</tablecell>
// </tablerow>
// <tablecell id="cell">Long second row, shows the table structure.</tablecell>
//
// The page generates a single anonymous table (LayoutTable) and table row group
// (LayoutTableSection) to wrap the <tablerow> (#firstRow) and an anonymous
// table row (LayoutTableRow) for the second <tablecell>.
// It is possible for JavaScript to insert a new element between these 2
// <tablecell> (using Node.insertBefore), requiring us to split the anonymous
// table (or the anonymous table row group) in 2. Also note that even
// though the second <tablecell> and <tablerow> are siblings in the DOM tree,
// they are not in the layout tree.
class CORE_EXPORT LayoutTable final : public LayoutBlock {
public:
    explicit LayoutTable(Element*);
    ~LayoutTable() override;

    // Per CSS 3 writing-mode: "The first and second values of the 'border-spacing' property represent spacing between columns
    // and rows respectively, not necessarily the horizontal and vertical spacing respectively".
    int hBorderSpacing() const { return m_hSpacing; }
    int vBorderSpacing() const { return m_vSpacing; }

    bool collapseBorders() const { return style()->borderCollapse(); }

    int borderStart() const override { return m_borderStart; }
    int borderEnd() const override { return m_borderEnd; }
    int borderBefore() const override;
    int borderAfter() const override;

    int borderLeft() const override
    {
        if (style()->isHorizontalWritingMode())
            return style()->isLeftToRightDirection() ? borderStart() : borderEnd();
        return style()->isFlippedBlocksWritingMode() ? borderAfter() : borderBefore();
    }

    int borderRight() const override
    {
        if (style()->isHorizontalWritingMode())
            return style()->isLeftToRightDirection() ? borderEnd() : borderStart();
        return style()->isFlippedBlocksWritingMode() ? borderBefore() : borderAfter();
    }

    int borderTop() const override
    {
        if (style()->isHorizontalWritingMode())
            return style()->isFlippedBlocksWritingMode() ? borderAfter() : borderBefore();
        return style()->isLeftToRightDirection() ? borderStart() : borderEnd();
    }

    int borderBottom() const override
    {
        if (style()->isHorizontalWritingMode())
            return style()->isFlippedBlocksWritingMode() ? borderBefore() : borderAfter();
        return style()->isLeftToRightDirection() ? borderEnd() : borderStart();
    }

    int outerBorderBefore() const;
    int outerBorderAfter() const;
    int outerBorderStart() const;
    int outerBorderEnd() const;

    int outerBorderLeft() const
    {
        if (style()->isHorizontalWritingMode())
            return style()->isLeftToRightDirection() ? outerBorderStart() : outerBorderEnd();
        return style()->isFlippedBlocksWritingMode() ? outerBorderAfter() : outerBorderBefore();
    }

    int outerBorderRight() const
    {
        if (style()->isHorizontalWritingMode())
            return style()->isLeftToRightDirection() ? outerBorderEnd() : outerBorderStart();
        return style()->isFlippedBlocksWritingMode() ? outerBorderBefore() : outerBorderAfter();
    }

    int outerBorderTop() const
    {
        if (style()->isHorizontalWritingMode())
            return style()->isFlippedBlocksWritingMode() ? outerBorderAfter() : outerBorderBefore();
        return style()->isLeftToRightDirection() ? outerBorderStart() : outerBorderEnd();
    }

    int outerBorderBottom() const
    {
        if (style()->isHorizontalWritingMode())
            return style()->isFlippedBlocksWritingMode() ? outerBorderBefore() : outerBorderAfter();
        return style()->isLeftToRightDirection() ? outerBorderEnd() : outerBorderStart();
    }

    int calcBorderStart() const;
    int calcBorderEnd() const;
    void recalcBordersInRowDirection();

    void addChild(LayoutObject* child, LayoutObject* beforeChild = nullptr) override;
    void addChildIgnoringContinuation(LayoutObject* newChild, LayoutObject* beforeChild = nullptr) override;

    struct ColumnStruct {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
        explicit ColumnStruct(unsigned initialSpan = 1)
            : span(initialSpan)
        {
        }

        unsigned span;
    };

    void forceSectionsRecalc()
    {
        setNeedsSectionRecalc();
        recalcSections();
    }

    const Vector<ColumnStruct>& columns() const { return m_columns; }
    const Vector<int>& columnPositions() const { return m_columnPos; }
    void setColumnPosition(unsigned index, int position)
    {
        // Note that if our horizontal border-spacing changed, our position will change but not
        // our column's width. In practice, horizontal border-spacing won't change often.
        m_columnLogicalWidthChanged |= m_columnPos[index] != position;
        m_columnPos[index] = position;
    }

    LayoutTableSection* header() const { return m_head; }
    LayoutTableSection* footer() const { return m_foot; }
    LayoutTableSection* firstBody() const { return m_firstBody; }

    // This function returns 0 if the table has no section.
    LayoutTableSection* topSection() const;
    LayoutTableSection* bottomSection() const;

    // This function returns 0 if the table has no non-empty sections.
    LayoutTableSection* topNonEmptySection() const;

    unsigned lastColumnIndex() const { return numEffCols() - 1; }

    void splitColumn(unsigned position, unsigned firstSpan);
    void appendColumn(unsigned span);
    unsigned numEffCols() const { return m_columns.size(); }
    unsigned spanOfEffCol(unsigned effCol) const { return m_columns[effCol].span; }

    unsigned colToEffCol(unsigned column) const
    {
        if (!m_hasCellColspanThatDeterminesTableWidth)
            return column;

        unsigned effColumn = 0;
        unsigned numColumns = numEffCols();
        for (unsigned c = 0; effColumn < numColumns && c + m_columns[effColumn].span - 1 < column; ++effColumn)
            c += m_columns[effColumn].span;
        return effColumn;
    }

    unsigned effColToCol(unsigned effCol) const
    {
        if (!m_hasCellColspanThatDeterminesTableWidth)
            return effCol;

        unsigned c = 0;
        for (unsigned i = 0; i < effCol; i++)
            c += m_columns[i].span;
        return c;
    }

    LayoutUnit borderSpacingInRowDirection() const
    {
        if (unsigned effectiveColumnCount = numEffCols())
            return static_cast<LayoutUnit>(effectiveColumnCount + 1) * hBorderSpacing();

        return LayoutUnit();
    }

    // The collapsing border model dissallows paddings on table, which is why we
    // override those functions.
    // See http://www.w3.org/TR/CSS2/tables.html#collapsing-borders.
    LayoutUnit paddingTop() const override;
    LayoutUnit paddingBottom() const override;
    LayoutUnit paddingLeft() const override;
    LayoutUnit paddingRight() const override;

    // Override paddingStart/End to return pixel values to match behavor of LayoutTableCell.
    LayoutUnit paddingEnd() const override { return LayoutUnit(static_cast<int>(LayoutBlock::paddingEnd())); }
    LayoutUnit paddingStart() const override { return LayoutUnit(static_cast<int>(LayoutBlock::paddingStart())); }

    LayoutUnit bordersPaddingAndSpacingInRowDirection() const
    {
        // 'border-spacing' only applies to separate borders (see 17.6.1 The separated borders model).
        return borderStart() + borderEnd() + (collapseBorders() ? LayoutUnit() : (paddingStart() + paddingEnd() + borderSpacingInRowDirection()));
    }

    // Return the first column or column-group.
    LayoutTableCol* firstColumn() const;

    struct ColAndColGroup {
        ColAndColGroup()
            : col(nullptr)
            , colgroup(nullptr)
            , adjoinsStartBorderOfColGroup(false)
            , adjoinsEndBorderOfColGroup(false)
        {
        }
        LayoutTableCol* col;
        LayoutTableCol* colgroup;
        bool adjoinsStartBorderOfColGroup;
        bool adjoinsEndBorderOfColGroup;
        LayoutTableCol* innermostColOrColGroup()
        {
            return col ? col : colgroup;
        }
    };
    ColAndColGroup colElement(unsigned col) const
    {
        // The common case is to not have columns, make that case fast.
        if (!m_hasColElements)
            return ColAndColGroup();
        return slowColElement(col);
    }

    bool needsSectionRecalc() const { return m_needsSectionRecalc; }
    void setNeedsSectionRecalc()
    {
        if (documentBeingDestroyed())
            return;
        m_needsSectionRecalc = true;
        setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::TableChanged);
    }

    LayoutTableSection* sectionAbove(const LayoutTableSection*, SkipEmptySectionsValue = DoNotSkipEmptySections) const;
    LayoutTableSection* sectionBelow(const LayoutTableSection*, SkipEmptySectionsValue = DoNotSkipEmptySections) const;

    LayoutTableCell* cellAbove(const LayoutTableCell*) const;
    LayoutTableCell* cellBelow(const LayoutTableCell*) const;
    LayoutTableCell* cellBefore(const LayoutTableCell*) const;
    LayoutTableCell* cellAfter(const LayoutTableCell*) const;

    typedef Vector<CollapsedBorderValue> CollapsedBorderValues;
    void invalidateCollapsedBorders();

    bool hasSections() const { return m_head || m_foot || m_firstBody; }

    void recalcSectionsIfNeeded() const
    {
        if (m_needsSectionRecalc)
            recalcSections();
    }

    static LayoutTable* createAnonymousWithParent(const LayoutObject*);
    LayoutBox* createAnonymousBoxWithSameTypeAs(const LayoutObject* parent) const override
    {
        return createAnonymousWithParent(parent);
    }

    const BorderValue& tableStartBorderAdjoiningCell(const LayoutTableCell*) const;
    const BorderValue& tableEndBorderAdjoiningCell(const LayoutTableCell*) const;

    void addCaption(const LayoutTableCaption*);
    void removeCaption(const LayoutTableCaption*);
    void addColumn(const LayoutTableCol*);
    void removeColumn(const LayoutTableCol*);

    void paintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&) const final;

    void paintMask(const PaintInfo&, const LayoutPoint&) const final;

    const CollapsedBorderValues& collapsedBorders() const
    {
        ASSERT(m_collapsedBordersValid);
        return m_collapsedBorders;
    }

    void subtractCaptionRect(LayoutRect&) const;

    const char* name() const override { return "LayoutTable"; }

protected:
    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;
    void simplifiedNormalFlowLayout() override;
    PaintInvalidationReason invalidatePaintIfNeeded(PaintInvalidationState&, const LayoutBoxModelObject& paintInvalidationContainer) override;
    void invalidatePaintOfSubtreesIfNeeded(PaintInvalidationState&) override;

private:
    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectTable || LayoutBlock::isOfType(type); }

    void paintObject(const PaintInfo&, const LayoutPoint&) const override;
    void layout() override;
    void computeIntrinsicLogicalWidths(LayoutUnit& minWidth, LayoutUnit& maxWidth) const override;
    void computePreferredLogicalWidths() override;
    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;
    int firstLineBoxBaseline() const override;
    int inlineBlockBaseline(LineDirectionMode) const override;

    ColAndColGroup slowColElement(unsigned col) const;

    void updateColumnCache() const;
    void invalidateCachedColumns();

    void updateLogicalWidth() override;

    LayoutUnit convertStyleLogicalWidthToComputedWidth(const Length& styleLogicalWidth, LayoutUnit availableWidth);
    LayoutUnit convertStyleLogicalHeightToComputedHeight(const Length& styleLogicalHeight);

    LayoutRect overflowClipRect(const LayoutPoint& location, OverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize) const override;

    void addOverflowFromChildren() override;

    void recalcSections() const;
    void layoutCaption(LayoutTableCaption&);

    void distributeExtraLogicalHeight(int extraLogicalHeight);

    void recalcCollapsedBordersIfNeeded();

    // TODO(layout-dev): All mutables in this class are lazily updated by recalcSections()
    // which is called by various getter methods (e.g. borderBefore(), borderAfter()).
    // They allow dirty layout even after DocumentLifecycle::LayoutClean which seems not proper. crbug.com/538236.

    mutable Vector<int> m_columnPos;

    // This Vector holds the columns counts over the entire table.
    //
    // To save memory at the expense of massive code complexity, the code tries
    // to coalesce columns. This means that we try to the wider column grouping
    // seen over the LayoutTableSections.
    //
    // Note that this is also a defensive pattern as <td colspan="6666666666">
    // only allocates a single entry in this Vector. This argument is weak
    // though as we cap colspans in HTMLTableCellElement.
    //
    // The following example would have 2 ColumnStruct [ 3, 2 ]:
    // <table>
    //   <tr>
    //     <td colspan="3"></td>
    //     <td colspan="2"></td>
    //   </tr>
    // </table>
    //
    // Columns can be split if we add a row with a different colspan structure.
    // See splitColumn and appendColumn for operations over |m_columns|.
    //
    // See colToEffCol for converting an absolute column index into an
    // index into |m_columns|.
    mutable Vector<ColumnStruct> m_columns;

    // The captions associated with this object.
    mutable Vector<LayoutTableCaption*> m_captions;

    mutable Vector<LayoutTableCol*> m_columnLayoutObjects;

    mutable LayoutTableSection* m_head;
    mutable LayoutTableSection* m_foot;
    mutable LayoutTableSection* m_firstBody;

    // The layout algorithm used by this table.
    //
    // CSS 2.1 defines 2 types of table layouts toggled with 'table-layout':
    // fixed (TableLayoutAlgorithmFixed) and auto (TableLayoutAlgorithmAuto).
    // See http://www.w3.org/TR/CSS21/tables.html#width-layout.
    //
    // The layout algorithm is delegated to TableLayoutAlgorithm. This enables
    // changing 'table-layout' without having to reattach the <table>.
    //
    // As the algorithm is dependent on the style, this field is nullptr before
    // the first style is applied in styleDidChange().
    OwnPtr<TableLayoutAlgorithm> m_tableLayout;

    // A sorted list of all unique border values that we want to paint.
    //
    // Collapsed borders are SUPER EXPENSIVE to compute. The reason is that we
    // need to compare a cells border against all the adjoining cells, rows,
    // row groups, column, column groups and table. Thus we cache them in this
    // field.
    CollapsedBorderValues m_collapsedBorders;
    bool m_collapsedBordersValid : 1;

    mutable bool m_hasColElements : 1;
    mutable bool m_needsSectionRecalc : 1;

    bool m_columnLogicalWidthChanged : 1;
    mutable bool m_columnLayoutObjectsValid: 1;
    mutable bool m_hasCellColspanThatDeterminesTableWidth : 1;
    bool hasCellColspanThatDeterminesTableWidth() const
    {
        for (unsigned c = 0; c < numEffCols(); c++) {
            if (m_columns[c].span > 1)
                return true;
        }
        return false;
    }

    short m_hSpacing;
    short m_vSpacing;
    int m_borderStart;
    int m_borderEnd;
};

inline LayoutTableSection* LayoutTable::topSection() const
{
    ASSERT(!needsSectionRecalc());
    if (m_head)
        return m_head;
    if (m_firstBody)
        return m_firstBody;
    return m_foot;
}

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTable, isTable());

} // namespace blink

#endif // LayoutTable_h
