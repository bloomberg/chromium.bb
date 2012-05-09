// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_ITEM_VIEW_H_
#define UI_APP_LIST_APP_LIST_ITEM_VIEW_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_item_model_observer.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/custom_button.h"

class SkBitmap;

namespace views {
class ImageView;
class MenuRunner;
}

namespace app_list {

class AppListItemModel;
class AppListModelView;
class DropShadowLabel;

class APP_LIST_EXPORT AppListItemView : public views::CustomButton,
                                        public views::ContextMenuController,
                                        public AppListItemModelObserver {
 public:
  AppListItemView(AppListModelView* list_model_view,
                  AppListItemModel* model,
                  views::ButtonListener* listener);
  virtual ~AppListItemView();

  static gfx::Size GetPreferredSizeForIconSize(const gfx::Size& icon_size);

  // For testing. Testing calls this function to set minimum title width in
  // pixels to get rid dependency on default font width.
  static void SetMinTitleWidth(int width);

  void SetSelected(bool selected);
  bool selected() const {
    return selected_;
  }

  void SetIconSize(const gfx::Size& size);

  AppListItemModel* model() const {
    return model_;
  }

  // Internal class name.
  static const char kViewClassName[];

 private:
  class IconOperation;

  // Get icon from model and schedule background processing.
  void UpdateIcon();

  // Cancel pending icon operation and reply callback.
  void CancelPendingIconOperation();

  // Reply callback from background shadow generation. |op| is the finished
  // operation and holds the result image.
  void ApplyShadow(scoped_refptr<IconOperation> op);

  // AppListItemModelObserver overrides:
  virtual void ItemIconChanged() OVERRIDE;
  virtual void ItemTitleChanged() OVERRIDE;
  virtual void ItemHighlightedChanged() OVERRIDE;

  // views::View overrides:
  virtual std::string GetClassName() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // views::ContextMenuController overrides:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& point) OVERRIDE;

  // views::CustomButton overrides:
  virtual void StateChanged() OVERRIDE;

  AppListItemModel* model_;

  AppListModelView* list_model_view_;
  views::ImageView* icon_;
  DropShadowLabel* title_;

  scoped_ptr<views::MenuRunner> context_menu_runner_;

  gfx::Size icon_size_;
  bool selected_;

  scoped_refptr<IconOperation> icon_op_;
  base::WeakPtrFactory<AppListItemView> apply_shadow_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListItemView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_ITEM_VIEW_H_
