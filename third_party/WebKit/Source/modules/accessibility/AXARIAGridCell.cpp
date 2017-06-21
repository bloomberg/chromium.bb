/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "modules/accessibility/AXARIAGridCell.h"

#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/accessibility/AXTable.h"
#include "modules/accessibility/AXTableRow.h"

namespace blink {

AXARIAGridCell::AXARIAGridCell(LayoutObject* layout_object,
                               AXObjectCacheImpl& ax_object_cache)
    : AXTableCell(layout_object, ax_object_cache) {}

AXARIAGridCell::~AXARIAGridCell() {}

AXARIAGridCell* AXARIAGridCell::Create(LayoutObject* layout_object,
                                       AXObjectCacheImpl& ax_object_cache) {
  return new AXARIAGridCell(layout_object, ax_object_cache);
}

bool AXARIAGridCell::IsAriaColumnHeader() const {
  const AtomicString& role = GetAttribute(HTMLNames::roleAttr);
  return EqualIgnoringASCIICase(role, "columnheader");
}

bool AXARIAGridCell::IsAriaRowHeader() const {
  const AtomicString& role = GetAttribute(HTMLNames::roleAttr);
  return EqualIgnoringASCIICase(role, "rowheader");
}

AXObject* AXARIAGridCell::ParentTable() const {
  AXObject* parent = ParentObjectUnignored();
  if (!parent)
    return 0;

  if (parent->IsAXTable())
    return parent;

  // It could happen that we hadn't reached the parent table yet (in
  // case objects for rows were not ignoring accessibility) so for
  // that reason we need to run parentObjectUnignored once again.
  parent = parent->ParentObjectUnignored();
  if (!parent || !parent->IsAXTable())
    return 0;

  return parent;
}

void AXARIAGridCell::RowIndexRange(std::pair<unsigned, unsigned>& row_range) {
  AXObject* parent = ParentObjectUnignored();
  if (!parent)
    return;

  if (parent->IsTableRow()) {
    // We already got a table row, use its API.
    row_range.first = ToAXTableRow(parent)->RowIndex();
  } else if (parent->IsAXTable()) {
    // We reached the parent table, so we need to inspect its
    // children to determine the row index for the cell in it.
    unsigned column_count = ToAXTable(parent)->ColumnCount();
    if (!column_count)
      return;

    const auto& siblings = parent->Children();
    unsigned children_size = siblings.size();
    for (unsigned k = 0; k < children_size; ++k) {
      if (siblings[k].Get() == this) {
        row_range.first = k / column_count;
        break;
      }
    }
  }

  // as far as I can tell, grid cells cannot span rows
  row_range.second = 1;
}

void AXARIAGridCell::ColumnIndexRange(
    std::pair<unsigned, unsigned>& column_range) {
  AXObject* parent = ParentObjectUnignored();
  if (!parent)
    return;

  if (!parent->IsTableRow() && !parent->IsAXTable())
    return;

  const auto& siblings = parent->Children();
  unsigned children_size = siblings.size();
  for (unsigned k = 0; k < children_size; ++k) {
    if (siblings[k].Get() == this) {
      column_range.first = k;
      break;
    }
  }

  // as far as I can tell, grid cells cannot span columns
  column_range.second = 1;
}

AccessibilityRole AXARIAGridCell::ScanToDecideHeaderRole() {
  if (IsAriaRowHeader())
    return kRowHeaderRole;

  if (IsAriaColumnHeader())
    return kColumnHeaderRole;

  return kCellRole;
}

}  // namespace blink
