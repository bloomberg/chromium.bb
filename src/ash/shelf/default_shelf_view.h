// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_DEFAULT_SHELF_VIEW_H_
#define ASH_SHELF_DEFAULT_SHELF_VIEW_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_view.h"
#include "base/macros.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class Shelf;
class ShelfModel;
class ShelfWidget;

// Default shelf view.
// TODO(agawrosnka): Move elements that are not common for default and Kiosk
// Next shelves from ShelfView to this subclass.
class ASH_EXPORT DefaultShelfView : public ShelfView {
 public:
  DefaultShelfView(ShelfModel* model, Shelf* shelf, ShelfWidget* shelf_widget);
  ~DefaultShelfView() override;

  // All ShelfView overrides are public to keep them together.
  // ShelfView:
  void Init() override;
  void LayoutAppListAndBackButtonHighlight() override;

 private:
  // A view to draw a background behind the app list and back buttons.
  // Owned by the view hierarchy.
  views::View* back_and_app_list_background_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DefaultShelfView);
};

}  // namespace ash

#endif  // ASH_SHELF_DEFAULT_SHELF_VIEW_H_
