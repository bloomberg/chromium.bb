// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_SECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_SECTION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section_interface.h"

namespace blink {

// NOTE:
// Every child of LayoutNGTableSection must be LayoutNGTableRow.
class CORE_EXPORT LayoutNGTableSection : public LayoutNGMixin<LayoutBlock>,
                                         public LayoutNGTableSectionInterface {
 public:
  explicit LayoutNGTableSection(Element*);

  bool IsEmpty() const;

  // LayoutBlock methods start.

  void UpdateBlockLayout(bool relayout_children) override { NOTREACHED(); }

  const char* GetName() const override { return "LayoutNGTableSection"; }

  bool AllowsOverflowClip() const override { return false; }

  bool BackgroundIsKnownToBeOpaqueInRect(const PhysicalRect&) const override {
    return false;
  }

  // LayoutBlock methods end.

  // LayoutNGTableSectionInterface methods start.

  const LayoutTableSection* ToLayoutTableSection() const final {
    DCHECK(false);
    return nullptr;
  }
  const LayoutNGTableSectionInterface* ToLayoutNGTableSectionInterface()
      const final {
    return this;
  }
  LayoutNGTableSectionInterface* ToLayoutNGTableSectionInterface() {
    return this;
  }
  const LayoutObject* ToLayoutObject() const final { return this; }

  LayoutObject* ToMutableLayoutObject() final { return this; }

  LayoutNGTableInterface* TableInterface() const final;

  void SetNeedsCellRecalc() final;

  bool IsRepeatingHeaderGroup() const final {
    // Used in printing, not used in LayoutNG
    return false;
  }

  bool IsRepeatingFooterGroup() const final {
    // Used in printing, not used in LayoutNG
    return false;
  }

  unsigned NumRows() const final;

  unsigned NumCols(unsigned) const final;

  unsigned NumEffectiveColumns() const final;

  LayoutNGTableRowInterface* FirstRowInterface() const final;

  LayoutNGTableRowInterface* LastRowInterface() const final;

  // Called by ax_layout_object.cc.
  const LayoutNGTableCellInterface* PrimaryCellInterfaceAt(
      unsigned row,
      unsigned column) const final;

  // LayoutNGTableSectionInterface methods end.

 protected:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectTableSection ||
           LayoutNGMixin<LayoutBlock>::IsOfType(type);
  }
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutNGTableSection> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTableSection() && object.IsLayoutNGObject();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_SECTION_H_
