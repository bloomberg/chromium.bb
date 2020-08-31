// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/table_model.h"

#include "base/check.h"
#include "base/i18n/string_compare.h"
#include "base/notreached.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"

namespace ui {

// TableColumn -----------------------------------------------------------------

TableColumn::TableColumn()
    : id(0),
      title(),
      alignment(LEFT),
      width(-1),
      percent(),
      min_visible_width(0),
      sortable(false),
      initial_sort_is_ascending(true) {
}

TableColumn::TableColumn(int id, Alignment alignment, int width, float percent)
    : id(id),
      title(l10n_util::GetStringUTF16(id)),
      alignment(alignment),
      width(width),
      percent(percent),
      min_visible_width(0),
      sortable(false),
      initial_sort_is_ascending(true) {
}

TableColumn::TableColumn(const TableColumn& other) = default;

// TableModel -----------------------------------------------------------------

// Used for sorting.
static icu::Collator* collator = NULL;

gfx::ImageSkia TableModel::GetIcon(int row) {
  return gfx::ImageSkia();
}

base::string16 TableModel::GetTooltip(int row) {
  return base::string16();
}

int TableModel::CompareValues(int row1, int row2, int column_id) {
  DCHECK(row1 >= 0 && row1 < RowCount() &&
         row2 >= 0 && row2 < RowCount());
  base::string16 value1 = GetText(row1, column_id);
  base::string16 value2 = GetText(row2, column_id);
  icu::Collator* collator = GetCollator();

  if (collator)
    return base::i18n::CompareString16WithCollator(*collator, value1, value2);

  NOTREACHED();
  return 0;
}

void TableModel::ClearCollator() {
  delete collator;
  collator = NULL;
}

icu::Collator* TableModel::GetCollator() {
  if (!collator) {
    UErrorCode create_status = U_ZERO_ERROR;
    collator = icu::Collator::createInstance(create_status);
    if (!U_SUCCESS(create_status)) {
      collator = NULL;
      NOTREACHED();
    }
  }
  return collator;
}

}  // namespace ui
