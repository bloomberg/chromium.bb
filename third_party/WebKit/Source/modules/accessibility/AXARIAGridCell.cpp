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

bool AXARIAGridCell::RowIndexRange(
    std::pair<unsigned, unsigned>& row_range) const {
  AXObject* parent = ParentObjectUnignored();
  if (!parent)
    return false;

  // Use native table semantics if this is ARIA overlayed on an HTML table.
  if (AXTableCell::RowIndexRange(row_range))
    return true;

  if (parent->IsTableRow()) {
    // We already got a table row, use its API.
    row_range.first = ToAXTableRow(parent)->RowIndex();
  } else if (parent->IsAXTable()) {
    // We reached the parent table, so we need to inspect its
    // children to determine the row index for the cell in it.
    unsigned column_count = ToAXTable(parent)->ColumnCount();
    if (!column_count)
      return false;

    const auto& siblings = parent->Children();
    unsigned children_size = siblings.size();
    for (unsigned k = 0; k < children_size; ++k) {
      if (siblings[k].Get() == this) {
        row_range.first = k / column_count;
        break;
      }
    }
  }

  // TODO should aria-rowspan be checked here? We also support it another way.
  row_range.second = 1;
  return true;
}

bool AXARIAGridCell::ColumnIndexRange(
    std::pair<unsigned, unsigned>& column_range) const {
  AXObject* parent = ParentObjectUnignored();
  if (!parent)
    return false;

  // Use native table semantics if this is ARIA overlayed on an HTML table.
  if (AXTableCell::ColumnIndexRange(column_range))
    return true;

  if (!parent->IsTableRow() && !parent->IsAXTable())
    return false;

  const auto& siblings = parent->Children();
  unsigned children_size = siblings.size();
  for (unsigned k = 0; k < children_size; ++k) {
    if (siblings[k].Get() == this) {
      column_range.first = k;
      break;
    }
  }

  // TODO should aria-colspan be checked here? We also support it another way.
  column_range.second = 1;
  return true;
}

AccessibilityRole AXARIAGridCell::ScanToDecideHeaderRole() {
  if (IsAriaRowHeader())
    return kRowHeaderRole;

  if (IsAriaColumnHeader())
    return kColumnHeaderRole;

  return AXTableCell::ScanToDecideHeaderRole();
}

AXRestriction AXARIAGridCell::Restriction() const {
  const AXRestriction cell_restriction = AXLayoutObject::Restriction();
  // Specified gridcell restriction or local ARIA markup takes precedence.
  if (cell_restriction != kNone || HasAttribute(HTMLNames::aria_readonlyAttr) ||
      HasAttribute(HTMLNames::aria_disabledAttr))
    return cell_restriction;

  // Gridcell does not have it's own ARIA input restriction, so
  // fall back on parent grid's readonly state.
  // See ARIA specification regarding grid/treegrid and readonly.
  const AXObject* container = ParentTable();
  const bool is_in_readonly_grid = container &&
                                   (container->RoleValue() == kGridRole ||
                                    container->RoleValue() == kTreeGridRole) &&
                                   container->Restriction() == kReadOnly;
  return is_in_readonly_grid ? kReadOnly : kNone;
}

}  // namespace blink
