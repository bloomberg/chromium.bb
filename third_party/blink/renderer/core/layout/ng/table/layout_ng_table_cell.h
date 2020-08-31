// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CELL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CELL_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/table_constants.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell_interface.h"

namespace blink {

class CORE_EXPORT LayoutNGTableCell
    : public LayoutNGBlockFlowMixin<LayoutBlockFlow>,
      public LayoutNGTableCellInterface {
 public:
  explicit LayoutNGTableCell(Element*);

  unsigned ComputedRowSpan() const {
    if (!has_rowspan_)
      return 1;
    unsigned rowspan = ParseRowSpanFromDOM();
    if (rowspan == 0)  // rowspan == 0 means all rows.
      rowspan = kMaxRowSpan;
    return rowspan;
  }

  // LayoutBlockFlow methods start.

  void UpdateBlockLayout(bool relayout_children) override;

  // TODO(atotic) Remove "New" from name.
  // Currently,  LayoutNGTableCellLegacy is named LayoutNGTableCell for test
  // compat.
  const char* GetName() const final { return "LayoutNGTableCellNew"; }

  // LayoutBlockFlow methods end.

  // LayoutNGTableCellInterface methods start.

  const LayoutTableCell* ToLayoutTableCell() const final {
    NOTREACHED();
    return nullptr;
  }

  const LayoutNGTableCellInterface* ToLayoutNGTableCellInterface() const final {
    return this;
  }
  const LayoutObject* ToLayoutObject() const final { return this; }

  LayoutObject* ToMutableLayoutObject() final { return this; }

  LayoutNGTableInterface* TableInterface() const final;

  void ColSpanOrRowSpanChanged() final;

  Length StyleOrColLogicalWidth() const final;

  // Not used in LayoutNG.
  int IntrinsicPaddingBefore() const final { return 0; }
  // Not used in LayoutNG.
  int IntrinsicPaddingAfter() const final { return 0; }

  unsigned RowIndex() const final;

  unsigned ResolvedRowSpan() const final;

  unsigned AbsoluteColumnIndex() const final;

  unsigned ColSpan() const final;

  LayoutNGTableCellInterface* NextCellInterface() const final;

  LayoutNGTableCellInterface* PreviousCellInterface() const final;

  LayoutNGTableRowInterface* RowInterface() const final;

  LayoutNGTableSectionInterface* SectionInterface() const final;

  // LayoutNGTableCellInterface methods end.

 protected:
  bool IsOfType(LayoutObjectType type) const final {
    return type == kLayoutObjectTableCell ||
           LayoutNGBlockFlowMixin<LayoutBlockFlow>::IsOfType(type);
  }

 private:
  void UpdateColAndRowSpanFlags();

  unsigned ParseRowSpanFromDOM() const;

  unsigned ParseColSpanFromDOM() const;
  // Use ComputedRowSpan instead
  unsigned ParsedRowSpan() const {
    if (!has_rowspan_)
      return 1;
    return ParseRowSpanFromDOM();
  }

  unsigned has_col_span_ : 1;
  unsigned has_rowspan_ : 1;
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutNGTableCell> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTableCell() && !object.IsTableCellLegacy();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CELL_H_
