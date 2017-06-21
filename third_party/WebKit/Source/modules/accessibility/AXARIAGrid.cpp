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

#include "modules/accessibility/AXARIAGrid.h"

#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/accessibility/AXTableColumn.h"
#include "modules/accessibility/AXTableRow.h"

namespace blink {

AXARIAGrid::AXARIAGrid(LayoutObject* layout_object,
                       AXObjectCacheImpl& ax_object_cache)
    : AXTable(layout_object, ax_object_cache) {}

AXARIAGrid::~AXARIAGrid() {}

AXARIAGrid* AXARIAGrid::Create(LayoutObject* layout_object,
                               AXObjectCacheImpl& ax_object_cache) {
  return new AXARIAGrid(layout_object, ax_object_cache);
}

bool AXARIAGrid::AddTableRowChild(AXObject* child,
                                  HeapHashSet<Member<AXObject>>& appended_rows,
                                  unsigned& column_count) {
  if (!child || child->RoleValue() != kRowRole)
    return false;

  if (appended_rows.Contains(child))
    return false;

  // store the maximum number of columns
  const unsigned row_cell_count = child->Children().size();
  if (row_cell_count > column_count)
    column_count = row_cell_count;

  AXTableRow* row = child->IsTableRow() ? ToAXTableRow(child) : 0;
  if (row) {
    row->SetRowIndex((int)rows_.size());
  }
  rows_.push_back(child);

  // Try adding the row if it's not ignoring accessibility,
  // otherwise add its children (the cells) as the grid's children.
  if (!child->AccessibilityIsIgnored())
    children_.push_back(child);
  else
    children_.AppendVector(child->Children());

  appended_rows.insert(child);
  return true;
}

void AXARIAGrid::AddChildren() {
  DCHECK(!IsDetached());
  DCHECK(!have_children_);

  if (!IsAXTable()) {
    AXLayoutObject::AddChildren();
    return;
  }

  have_children_ = true;
  if (!layout_object_)
    return;

  HeapVector<Member<AXObject>> children;
  for (AXObject* child = RawFirstChild(); child;
       child = child->RawNextSibling())
    children.push_back(child);
  ComputeAriaOwnsChildren(children);

  AXObjectCacheImpl& ax_cache = AxObjectCache();

  // Only add children that are actually rows.
  HeapHashSet<Member<AXObject>> appended_rows;
  unsigned column_count = 0;
  for (const auto& child : children) {
    if (!AddTableRowChild(child, appended_rows, column_count)) {
      // in case the layout tree doesn't match the expected ARIA hierarchy, look
      // at the children
      if (!child->HasChildren())
        child->AddChildren();

      // The children of this non-row will contain all non-ignored elements
      // (recursing to find them).  This allows the table to dive arbitrarily
      // deep to find the rows.
      for (const auto& child_object : child->Children())
        AddTableRowChild(child_object.Get(), appended_rows, column_count);
    }
  }

  // make the columns based on the number of columns in the first body
  for (unsigned i = 0; i < column_count; ++i) {
    AXTableColumn* column = ToAXTableColumn(ax_cache.GetOrCreate(kColumnRole));
    column->SetColumnIndex((int)i);
    column->SetParent(this);
    columns_.push_back(column);
    if (!column->AccessibilityIsIgnored())
      children_.push_back(column);
  }

  AXObject* header_container_object = HeaderContainer();
  if (header_container_object &&
      !header_container_object->AccessibilityIsIgnored())
    children_.push_back(header_container_object);
}

}  // namespace blink
