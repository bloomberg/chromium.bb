/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StyleGridData_h
#define StyleGridData_h

#include "core/style/ComputedStyleConstants.h"
#include "core/style/GridArea.h"
#include "core/style/GridTrackSize.h"
#include "core/style/NamedGridLinesMap.h"
#include "core/style/OrderedNamedGridLines.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Vector.h"

namespace blink {

class StyleGridData : public RefCounted<StyleGridData> {
 public:
  static PassRefPtr<StyleGridData> Create() {
    return AdoptRef(new StyleGridData);
  }
  PassRefPtr<StyleGridData> Copy() const {
    return AdoptRef(new StyleGridData(*this));
  }

  bool operator==(const StyleGridData& o) const {
    return grid_template_columns_ == o.grid_template_columns_ &&
           grid_template_rows_ == o.grid_template_rows_ &&
           grid_auto_flow_ == o.grid_auto_flow_ &&
           grid_auto_rows_ == o.grid_auto_rows_ &&
           grid_auto_columns_ == o.grid_auto_columns_ &&
           named_grid_column_lines_ == o.named_grid_column_lines_ &&
           named_grid_row_lines_ == o.named_grid_row_lines_ &&
           ordered_named_grid_column_lines_ ==
               o.ordered_named_grid_column_lines_ &&
           ordered_named_grid_row_lines_ == o.ordered_named_grid_row_lines_ &&
           auto_repeat_named_grid_column_lines_ ==
               o.auto_repeat_named_grid_column_lines_ &&
           auto_repeat_named_grid_row_lines_ ==
               o.auto_repeat_named_grid_row_lines_ &&
           auto_repeat_ordered_named_grid_column_lines_ ==
               o.auto_repeat_ordered_named_grid_column_lines_ &&
           auto_repeat_ordered_named_grid_row_lines_ ==
               o.auto_repeat_ordered_named_grid_row_lines_ &&
           named_grid_area_ == o.named_grid_area_ &&
           named_grid_area_ == o.named_grid_area_ &&
           named_grid_area_row_count_ == o.named_grid_area_row_count_ &&
           named_grid_area_column_count_ == o.named_grid_area_column_count_ &&
           grid_column_gap_ == o.grid_column_gap_ &&
           grid_row_gap_ == o.grid_row_gap_ &&
           grid_auto_repeat_columns_ == o.grid_auto_repeat_columns_ &&
           grid_auto_repeat_rows_ == o.grid_auto_repeat_rows_ &&
           auto_repeat_columns_insertion_point_ ==
               o.auto_repeat_columns_insertion_point_ &&
           auto_repeat_rows_insertion_point_ ==
               o.auto_repeat_rows_insertion_point_ &&
           auto_repeat_columns_type_ == o.auto_repeat_columns_type_ &&
           auto_repeat_rows_type_ == o.auto_repeat_rows_type_;
  }

  bool operator!=(const StyleGridData& o) const { return !(*this == o); }

  Vector<GridTrackSize> grid_template_columns_;
  Vector<GridTrackSize> grid_template_rows_;

  NamedGridLinesMap named_grid_column_lines_;
  NamedGridLinesMap named_grid_row_lines_;

  // In order to reconstruct the original named grid line order, we can't rely
  // on NamedGridLinesMap as it loses the position if multiple grid lines are
  // set on a single track.
  OrderedNamedGridLines ordered_named_grid_column_lines_;
  OrderedNamedGridLines ordered_named_grid_row_lines_;

  NamedGridLinesMap auto_repeat_named_grid_column_lines_;
  NamedGridLinesMap auto_repeat_named_grid_row_lines_;
  OrderedNamedGridLines auto_repeat_ordered_named_grid_column_lines_;
  OrderedNamedGridLines auto_repeat_ordered_named_grid_row_lines_;

  unsigned grid_auto_flow_ : kGridAutoFlowBits;

  Vector<GridTrackSize> grid_auto_rows_;
  Vector<GridTrackSize> grid_auto_columns_;

  NamedGridAreaMap named_grid_area_;
  // Because m_namedGridArea doesn't store the unnamed grid areas, we need to
  // keep track of the explicit grid size defined by both named and unnamed grid
  // areas.
  size_t named_grid_area_row_count_;
  size_t named_grid_area_column_count_;

  Length grid_column_gap_;
  Length grid_row_gap_;

  Vector<GridTrackSize> grid_auto_repeat_columns_;
  Vector<GridTrackSize> grid_auto_repeat_rows_;

  size_t auto_repeat_columns_insertion_point_;
  size_t auto_repeat_rows_insertion_point_;

  AutoRepeatType auto_repeat_columns_type_;
  AutoRepeatType auto_repeat_rows_type_;

 private:
  StyleGridData();
  StyleGridData(const StyleGridData&);
};

}  // namespace blink

#endif  // StyleGridData_h
