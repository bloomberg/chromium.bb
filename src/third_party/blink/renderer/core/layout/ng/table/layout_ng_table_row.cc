// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_row.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"

namespace blink {

LayoutNGTableRow::LayoutNGTableRow(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {}

bool LayoutNGTableRow::IsEmpty() const {
  NOT_DESTROYED();
  return !FirstChild();
}

LayoutNGTable* LayoutNGTableRow::Table() const {
  NOT_DESTROYED();
  if (LayoutObject* section = Parent()) {
    if (LayoutObject* table = section->Parent())
      return To<LayoutNGTable>(table);
  }
  return nullptr;
}

void LayoutNGTableRow::AddChild(LayoutObject* child,
                                LayoutObject* before_child) {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table())
    table->TableGridStructureChanged();

  if (!child->IsTableCell()) {
    LayoutObject* last = before_child;
    if (!last)
      last = LastCell();
    if (last && last->IsAnonymous() && last->IsTableCell() &&
        !last->IsBeforeOrAfterContent()) {
      LayoutBlockFlow* last_cell = To<LayoutBlockFlow>(last);
      if (before_child == last_cell)
        before_child = last_cell->FirstChild();
      last_cell->AddChild(child, before_child);
      return;
    }

    if (before_child && !before_child->IsAnonymous() &&
        before_child->Parent() == this) {
      LayoutObject* cell = before_child->PreviousSibling();
      if (cell && cell->IsTableCell() && cell->IsAnonymous()) {
        cell->AddChild(child);
        return;
      }
    }

    // If before_child is inside an anonymous cell, insert into the cell.
    if (last && !last->IsTableCell() && last->Parent() &&
        last->Parent()->IsAnonymous() &&
        !last->Parent()->IsBeforeOrAfterContent()) {
      last->Parent()->AddChild(child, before_child);
      return;
    }

    LayoutBlockFlow* cell =
        LayoutObjectFactory::CreateAnonymousTableCellWithParent(*this);
    AddChild(cell, before_child);
    cell->AddChild(child);
    return;
  }

  if (before_child && before_child->Parent() != this)
    before_child = SplitAnonymousBoxesAroundChild(before_child);

  DCHECK(!before_child || before_child->IsTableCell());
  LayoutNGMixin<LayoutBlock>::AddChild(child, before_child);
}

void LayoutNGTableRow::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table())
    table->TableGridStructureChanged();
  LayoutNGMixin<LayoutBlock>::RemoveChild(child);
}

void LayoutNGTableRow::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table())
    table->TableGridStructureChanged();
  LayoutNGMixin<LayoutBlock>::WillBeRemovedFromTree();
}

void LayoutNGTableRow::StyleDidChange(StyleDifference diff,
                                      const ComputedStyle* old_style) {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table()) {
    if ((old_style && !old_style->BorderVisuallyEqual(StyleRef())) ||
        (old_style && old_style->GetWritingDirection() !=
                          StyleRef().GetWritingDirection()) ||
        (diff.TextDecorationOrColorChanged() &&
         StyleRef().HasBorderColorReferencingCurrentColor())) {
      table->GridBordersChanged();
    }
  }
  LayoutNGMixin<LayoutBlock>::StyleDidChange(diff, old_style);
}

LayoutBox* LayoutNGTableRow::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  NOT_DESTROYED();
  return LayoutObjectFactory::CreateAnonymousTableRowWithParent(*parent);
}

LayoutBlock* LayoutNGTableRow::StickyContainer() const {
  NOT_DESTROYED();
  return Table();
}

#if DCHECK_IS_ON()
void LayoutNGTableRow::AddVisualOverflowFromBlockChildren() {
  NOT_DESTROYED();
  // This is computed in |NGPhysicalBoxFragment::ComputeSelfInkOverflow| and
  // that we should not reach here.
  NOTREACHED();
}
#endif

PositionWithAffinity LayoutNGTableRow::PositionForPoint(
    const PhysicalOffset& offset) const {
  NOT_DESTROYED();
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
  // LayoutBlock::PositionForPoint is wrong for rows.
  return LayoutBox::PositionForPoint(offset);
}

unsigned LayoutNGTableRow::RowIndex() const {
  NOT_DESTROYED();
  unsigned index = 0;
  for (LayoutObject* child = Parent()->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child == this)
      return index;
    ++index;
  }
  NOTREACHED();
  return 0;
}

LayoutNGTableCell* LayoutNGTableRow::LastCell() const {
  NOT_DESTROYED();
  return To<LayoutNGTableCell>(LastChild());
}

LayoutNGTableSectionInterface* LayoutNGTableRow::SectionInterface() const {
  NOT_DESTROYED();
  return To<LayoutNGTableSection>(Parent());
}

LayoutNGTableRowInterface* LayoutNGTableRow::PreviousRowInterface() const {
  NOT_DESTROYED();
  return ToInterface<LayoutNGTableRowInterface>(PreviousSibling());
}

LayoutNGTableRowInterface* LayoutNGTableRow::NextRowInterface() const {
  NOT_DESTROYED();
  return ToInterface<LayoutNGTableRowInterface>(NextSibling());
}

LayoutNGTableCellInterface* LayoutNGTableRow::FirstCellInterface() const {
  NOT_DESTROYED();
  return ToInterface<LayoutNGTableCellInterface>(FirstChild());
}

LayoutNGTableCellInterface* LayoutNGTableRow::LastCellInterface() const {
  NOT_DESTROYED();
  return ToInterface<LayoutNGTableCellInterface>(LastChild());
}

}  // namespace blink
