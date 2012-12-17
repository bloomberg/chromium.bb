// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_TABLE_HEADER_H_
#define UI_VIEWS_CONTROLS_TABLE_TABLE_HEADER_H_

#include "ui/gfx/font.h"
#include "ui/views/view.h"

namespace views {

class TableView;

// Views used to render the header for the table.
class TableHeader : public views::View {
 public:
  explicit TableHeader(TableView* table);
  virtual ~TableHeader();

  const gfx::Font& font() const { return font_; }

  // views::View overrides.
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  const gfx::Font font_;

  TableView* table_;

  DISALLOW_COPY_AND_ASSIGN(TableHeader);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABLE_TABLE_HEADER_H_
