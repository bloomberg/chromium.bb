/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/accessibility/AXTableCell.h"

#include "core/dom/AccessibleNode.h"
#include "core/layout/LayoutTableCell.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/accessibility/AXTableRow.h"

namespace blink {

using namespace HTMLNames;

AXTableCell::AXTableCell(LayoutObject* layout_object,
                         AXObjectCacheImpl& ax_object_cache)
    : AXLayoutObject(layout_object, ax_object_cache) {}

AXTableCell::~AXTableCell() {}

AXTableCell* AXTableCell::Create(LayoutObject* layout_object,
                                 AXObjectCacheImpl& ax_object_cache) {
  return new AXTableCell(layout_object, ax_object_cache);
}

bool AXTableCell::IsTableHeaderCell() const {
  return GetNode() && GetNode()->HasTagName(thTag);
}

bool AXTableCell::IsRowHeaderCell() const {
  const AtomicString& scope = GetAttribute(scopeAttr);
  return EqualIgnoringASCIICase(scope, "row") ||
         EqualIgnoringASCIICase(scope, "rowgroup");
}

bool AXTableCell::IsColumnHeaderCell() const {
  const AtomicString& scope = GetAttribute(scopeAttr);
  return EqualIgnoringASCIICase(scope, "col") ||
         EqualIgnoringASCIICase(scope, "colgroup");
}

bool AXTableCell::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  AXObjectInclusion decision = DefaultObjectInclusion(ignored_reasons);
  if (decision == kIncludeObject)
    return false;
  if (decision == kIgnoreObject)
    return true;

  if (!IsTableCell())
    return AXLayoutObject::ComputeAccessibilityIsIgnored(ignored_reasons);

  return false;
}

AXObject* AXTableCell::ParentTable() const {
  if (!layout_object_ || !layout_object_->IsTableCell())
    return 0;

  // If the document no longer exists, we might not have an axObjectCache.
  if (IsDetached())
    return 0;

  // Do not use getOrCreate. parentTable() can be called while the layout tree
  // is being modified by javascript, and creating a table element may try to
  // access the layout tree while in a bad state.  By using only get() implies
  // that the AXTable must be created before AXTableCells. This should always be
  // the case when AT clients access a table.
  // https://bugs.webkit.org/show_bug.cgi?id=42652
  return AxObjectCache().Get(ToLayoutTableCell(layout_object_)->Table());
}

bool AXTableCell::IsTableCell() const {
  AXObject* parent = ParentObjectUnignored();
  if (!parent || !parent->IsTableRow())
    return false;

  return true;
}

unsigned AXTableCell::AriaColumnIndex() const {
  uint32_t col_index;
  if (HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kColIndex, col_index) &&
      col_index >= 1) {
    return col_index;
  }

  AXObject* parent = ParentObjectUnignored();
  if (!parent || !parent->IsTableRow())
    return 0;

  return aria_col_index_from_row_;
}

unsigned AXTableCell::AriaRowIndex() const {
  uint32_t row_index;
  if (HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kRowIndex, row_index) &&
      row_index >= 1) {
    return row_index;
  }

  AXObject* parent = ParentObjectUnignored();
  if (!parent || !parent->IsTableRow())
    return 0;

  return ToAXTableRow(parent)->AriaRowIndex();
}

static AccessibilityRole DecideRoleFromSibling(LayoutTableCell* sibling_cell) {
  if (!sibling_cell)
    return kCellRole;

  if (Node* sibling_node = sibling_cell->GetNode()) {
    if (sibling_node->HasTagName(thTag))
      return kColumnHeaderRole;
    if (sibling_node->HasTagName(tdTag))
      return kRowHeaderRole;
  }

  return kCellRole;
}

AccessibilityRole AXTableCell::ScanToDecideHeaderRole() {
  if (!IsTableHeaderCell())
    return kCellRole;

  // Check scope attribute first.
  if (IsRowHeaderCell())
    return kRowHeaderRole;

  if (IsColumnHeaderCell())
    return kColumnHeaderRole;

  // Check the previous cell and the next cell on the same row.
  LayoutTableCell* layout_cell = ToLayoutTableCell(layout_object_);
  AccessibilityRole header_role = kCellRole;

  // if header is preceded by header cells on the same row, then it is a
  // column header. If it is preceded by other cells then it's a row header.
  if ((header_role = DecideRoleFromSibling(layout_cell->PreviousCell())) !=
      kCellRole)
    return header_role;

  // if header is followed by header cells on the same row, then it is a
  // column header. If it is followed by other cells then it's a row header.
  if ((header_role = DecideRoleFromSibling(layout_cell->NextCell())) !=
      kCellRole)
    return header_role;

  // If there are no other cells on that row, then it is a column header.
  return kColumnHeaderRole;
}

AccessibilityRole AXTableCell::DetermineAccessibilityRole() {
  if (!IsTableCell())
    return AXLayoutObject::DetermineAccessibilityRole();

  return ScanToDecideHeaderRole();
}

void AXTableCell::RowIndexRange(std::pair<unsigned, unsigned>& row_range) {
  if (!layout_object_ || !layout_object_->IsTableCell())
    return;

  LayoutTableCell* layout_cell = ToLayoutTableCell(layout_object_);
  row_range.first = layout_cell->RowIndex();
  row_range.second = layout_cell->RowSpan();

  // Since our table might have multiple sections, we have to offset our row
  // appropriately.
  LayoutTableSection* section = layout_cell->Section();
  LayoutTable* table = layout_cell->Table();
  if (!table || !section)
    return;

  LayoutTableSection* table_section = table->TopSection();
  unsigned row_offset = 0;
  while (table_section) {
    if (table_section == section)
      break;
    row_offset += table_section->NumRows();
    table_section = table->SectionBelow(table_section, kSkipEmptySections);
  }

  row_range.first += row_offset;
}

void AXTableCell::ColumnIndexRange(
    std::pair<unsigned, unsigned>& column_range) {
  if (!layout_object_ || !layout_object_->IsTableCell())
    return;

  LayoutTableCell* cell = ToLayoutTableCell(layout_object_);
  column_range.first = cell->Table()->AbsoluteColumnToEffectiveColumn(
      cell->AbsoluteColumnIndex());
  column_range.second = cell->Table()->AbsoluteColumnToEffectiveColumn(
                            cell->AbsoluteColumnIndex() + cell->ColSpan()) -
                        column_range.first;
}

SortDirection AXTableCell::GetSortDirection() const {
  if (RoleValue() != kRowHeaderRole && RoleValue() != kColumnHeaderRole)
    return kSortDirectionUndefined;

  const AtomicString& aria_sort =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kSort);
  if (aria_sort.IsEmpty())
    return kSortDirectionUndefined;
  if (EqualIgnoringASCIICase(aria_sort, "none"))
    return kSortDirectionNone;
  if (EqualIgnoringASCIICase(aria_sort, "ascending"))
    return kSortDirectionAscending;
  if (EqualIgnoringASCIICase(aria_sort, "descending"))
    return kSortDirectionDescending;
  if (EqualIgnoringASCIICase(aria_sort, "other"))
    return kSortDirectionOther;
  return kSortDirectionUndefined;
}

}  // namespace blink
