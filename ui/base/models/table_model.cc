// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/table_model.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"

namespace ui {

// TableColumn -----------------------------------------------------------------

TableColumn::TableColumn()
    : id(0),
      title(),
      alignment(LEFT),
      width(-1),
      percent(),
      min_visible_width(0),
      sortable(false) {
}

TableColumn::TableColumn(int id, const string16& title,
                         Alignment alignment,
                         int width)
    : id(id),
      title(title),
      alignment(alignment),
      width(width),
      percent(0),
      min_visible_width(0),
      sortable(false) {
}

TableColumn::TableColumn(int id, const string16& title,
                         Alignment alignment, int width, float percent)
    : id(id),
      title(title),
      alignment(alignment),
      width(width),
      percent(percent),
      min_visible_width(0),
      sortable(false) {
}

// It's common (but not required) to use the title's IDS_* tag as the column
// id. In this case, the provided conveniences look up the title string on
// bahalf of the caller.
TableColumn::TableColumn(int id, Alignment alignment, int width)
    : id(id),
      alignment(alignment),
      width(width),
      percent(0),
      min_visible_width(0),
      sortable(false) {
  title = l10n_util::GetStringUTF16(id);
}

TableColumn::TableColumn(int id, Alignment alignment, int width, float percent)
    : id(id),
      alignment(alignment),
      width(width),
      percent(percent),
      min_visible_width(0),
      sortable(false) {
  title = l10n_util::GetStringUTF16(id);
}

// TableModel -----------------------------------------------------------------

// Used for sorting.
static icu::Collator* collator = NULL;

SkBitmap TableModel::GetIcon(int row) {
  return SkBitmap();
}

string16 TableModel::GetTooltip(int row) {
  return string16();
}

bool TableModel::ShouldIndent(int row) {
  return false;
}

bool TableModel::HasGroups() {
  return false;
}

TableModel::Groups TableModel::GetGroups() {
  // If you override HasGroups to return true, you must override this as
  // well.
  NOTREACHED();
  return std::vector<Group>();
}

int TableModel::GetGroupID(int row) {
  // If you override HasGroups to return true, you must override this as
  // well.
  NOTREACHED();
  return 0;
}

int TableModel::CompareValues(int row1, int row2, int column_id) {
  DCHECK(row1 >= 0 && row1 < RowCount() &&
         row2 >= 0 && row2 < RowCount());
  string16 value1 = GetText(row1, column_id);
  string16 value2 = GetText(row2, column_id);
  icu::Collator* collator = GetCollator();

  if (collator)
    return l10n_util::CompareString16WithCollator(collator, value1, value2);

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
