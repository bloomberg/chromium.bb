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

#include "core/style/StyleGridData.h"

#include "core/style/ComputedStyle.h"

namespace blink {

StyleGridData::StyleGridData()
    : grid_template_columns_(ComputedStyle::InitialGridTemplateColumns()),
      grid_template_rows_(ComputedStyle::InitialGridTemplateRows()),
      named_grid_column_lines_(ComputedStyle::InitialNamedGridColumnLines()),
      named_grid_row_lines_(ComputedStyle::InitialNamedGridRowLines()),
      ordered_named_grid_column_lines_(
          ComputedStyle::InitialOrderedNamedGridColumnLines()),
      ordered_named_grid_row_lines_(
          ComputedStyle::InitialOrderedNamedGridRowLines()),
      auto_repeat_named_grid_column_lines_(
          ComputedStyle::InitialNamedGridColumnLines()),
      auto_repeat_named_grid_row_lines_(
          ComputedStyle::InitialNamedGridRowLines()),
      auto_repeat_ordered_named_grid_column_lines_(
          ComputedStyle::InitialOrderedNamedGridColumnLines()),
      auto_repeat_ordered_named_grid_row_lines_(
          ComputedStyle::InitialOrderedNamedGridRowLines()),
      grid_auto_rows_(ComputedStyle::InitialGridAutoRows()),
      grid_auto_columns_(ComputedStyle::InitialGridAutoColumns()),
      named_grid_area_(ComputedStyle::InitialNamedGridArea()),
      named_grid_area_row_count_(ComputedStyle::InitialNamedGridAreaCount()),
      named_grid_area_column_count_(ComputedStyle::InitialNamedGridAreaCount()),
      grid_column_gap_(ComputedStyle::InitialGridColumnGap()),
      grid_row_gap_(ComputedStyle::InitialGridRowGap()),
      grid_auto_repeat_columns_(ComputedStyle::InitialGridAutoRepeatTracks()),
      grid_auto_repeat_rows_(ComputedStyle::InitialGridAutoRepeatTracks()),
      auto_repeat_columns_insertion_point_(
          ComputedStyle::InitialGridAutoRepeatInsertionPoint()),
      auto_repeat_rows_insertion_point_(
          ComputedStyle::InitialGridAutoRepeatInsertionPoint()),
      grid_auto_flow_(ComputedStyle::InitialGridAutoFlow()),
      auto_repeat_columns_type_(ComputedStyle::InitialGridAutoRepeatType()),
      auto_repeat_rows_type_(ComputedStyle::InitialGridAutoRepeatType()) {}

StyleGridData::StyleGridData(const StyleGridData& o)
    : RefCounted<StyleGridData>(),
      grid_template_columns_(o.grid_template_columns_),
      grid_template_rows_(o.grid_template_rows_),
      named_grid_column_lines_(o.named_grid_column_lines_),
      named_grid_row_lines_(o.named_grid_row_lines_),
      ordered_named_grid_column_lines_(o.ordered_named_grid_column_lines_),
      ordered_named_grid_row_lines_(o.ordered_named_grid_row_lines_),
      auto_repeat_named_grid_column_lines_(
          o.auto_repeat_named_grid_column_lines_),
      auto_repeat_named_grid_row_lines_(o.auto_repeat_named_grid_row_lines_),
      auto_repeat_ordered_named_grid_column_lines_(
          o.auto_repeat_ordered_named_grid_column_lines_),
      auto_repeat_ordered_named_grid_row_lines_(
          o.auto_repeat_ordered_named_grid_row_lines_),
      grid_auto_rows_(o.grid_auto_rows_),
      grid_auto_columns_(o.grid_auto_columns_),
      named_grid_area_(o.named_grid_area_),
      named_grid_area_row_count_(o.named_grid_area_row_count_),
      named_grid_area_column_count_(o.named_grid_area_column_count_),
      grid_column_gap_(o.grid_column_gap_),
      grid_row_gap_(o.grid_row_gap_),
      grid_auto_repeat_columns_(o.grid_auto_repeat_columns_),
      grid_auto_repeat_rows_(o.grid_auto_repeat_rows_),
      auto_repeat_columns_insertion_point_(
          o.auto_repeat_columns_insertion_point_),
      auto_repeat_rows_insertion_point_(o.auto_repeat_rows_insertion_point_),
      grid_auto_flow_(o.grid_auto_flow_),
      auto_repeat_columns_type_(o.auto_repeat_columns_type_),
      auto_repeat_rows_type_(o.auto_repeat_rows_type_) {}

}  // namespace blink
