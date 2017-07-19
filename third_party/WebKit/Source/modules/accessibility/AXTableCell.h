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

#ifndef AXTableCell_h
#define AXTableCell_h

#include "modules/accessibility/AXLayoutObject.h"

namespace blink {

class AXObjectCacheImpl;

class MODULES_EXPORT AXTableCell : public AXLayoutObject {
  WTF_MAKE_NONCOPYABLE(AXTableCell);

 protected:
  AXTableCell(LayoutObject*, AXObjectCacheImpl&);

 public:
  static AXTableCell* Create(LayoutObject*, AXObjectCacheImpl&);
  ~AXTableCell() override;

  bool IsTableCell() const final;

  // fills in the start location and row span of cell
  virtual void RowIndexRange(std::pair<unsigned, unsigned>& row_range);
  // fills in the start location and column span of cell
  virtual void ColumnIndexRange(std::pair<unsigned, unsigned>& column_range);
  // In the case of cells that act as row or column headers.
  SortDirection GetSortDirection() const final;
  virtual AccessibilityRole ScanToDecideHeaderRole();

  unsigned AriaColumnIndex() const;
  unsigned AriaRowIndex() const;
  void SetARIAColIndexFromRow(int index) { aria_col_index_from_row_ = index; }
  virtual bool CanSetSelectedAttribute() const { return false; }

 protected:
  virtual AXObject* ParentTable() const;
  AccessibilityRole DetermineAccessibilityRole() final;

 private:
  bool IsTableHeaderCell() const;
  bool IsRowHeaderCell() const;
  bool IsColumnHeaderCell() const;

  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const final;

  unsigned aria_col_index_from_row_ = 0;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXTableCell, IsTableCell());

}  // namespace blink

#endif  // AXTableCell_h
