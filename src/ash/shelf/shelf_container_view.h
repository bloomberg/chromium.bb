// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONTAINER_VIEW_H_
#define ASH_SHELF_SHELF_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_view.h"
#include "ui/views/view.h"

namespace ash {

class ASH_EXPORT ShelfContainerView : public views::View {
 public:
  explicit ShelfContainerView(ShelfView* shelf_view);
  ~ShelfContainerView() override;

  void Initialize();

  // Calculates the ideal size of |shelf_view_| to accommodate all of app
  // buttons without scrolling.
  gfx::Size CalculateIdealSize(int button_size) const;

  // Translate |shelf_view_| by |offset|.
  // TODO(https://crbug.com/973481): now we implement ShelfView scrolling
  // through view translation, which is not as efficient as ScrollView. Redesign
  // this class with ScrollView.
  virtual void TranslateShelfView(const gfx::Vector2dF& offset);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  const char* GetClassName() const override;

 protected:
  // Owned by views hierarchy.
  ShelfView* shelf_view_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfContainerView);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONTAINER_VIEW_H_
