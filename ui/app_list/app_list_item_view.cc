// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_item_view.h"

#include <algorithm>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/worker_pool.h"
#include "base/utf_string_conversions.h"
#include "skia/ext/image_operations.h"
#include "ui/app_list/app_list_item_model.h"
#include "ui/app_list/apps_grid_view.h"
#include "ui/app_list/drop_shadow_label.h"
#include "ui/app_list/icon_cache.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace app_list {

namespace {

const int kTopBottomPadding = 10;
const int kTopPadding = 20;
const int kIconTitleSpacing = 7;

const SkColor kTitleColor = SkColorSetRGB(0x5A, 0x5A, 0x5A);
const SkColor kTitleHoverColor = SkColorSetRGB(0x3C, 0x3C, 0x3C);
const SkColor kTitleBackgroundColor = SkColorSetRGB(0xFC, 0xFC, 0xFC);

const SkColor kHoverAndPushedColor = SkColorSetARGB(0x19, 0, 0, 0);
const SkColor kSelectedColor = SkColorSetARGB(0x0D, 0, 0, 0);
const SkColor kHighlightedColor = kHoverAndPushedColor;

const int kTitleFontSize = 11;
const int kLeftRightPaddingChars = 1;

const gfx::Font& GetTitleFont() {
  static gfx::Font* font = NULL;

  // Reuses current font.
  if (font)
    return *font;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::Font title_font(rb.GetFont(ui::ResourceBundle::BaseFont).GetFontName(),
                       kTitleFontSize);
  title_font = title_font.DeriveFont(0, gfx::Font::BOLD);
  font = new gfx::Font(title_font);
  return *font;
}

// An image view that is not interactive.
class StaticImageView : public views::ImageView {
 public:
  StaticImageView() : ImageView() {
  }

 private:
  // views::View overrides:
  virtual bool HitTest(const gfx::Point& l) const OVERRIDE {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(StaticImageView);
};

}  // namespace

// static
const char AppListItemView::kViewClassName[] = "ui/app_list/AppListItemView";

// AppListItemView::IconOperation wraps background icon processing.
class AppListItemView::IconOperation
    : public base::RefCountedThreadSafe<AppListItemView::IconOperation> {
 public:
  IconOperation(const SkBitmap& bitmap,
                const gfx::Size& size,
                const gfx::ShadowValues& shadows)
      : bitmap_(bitmap),
        size_(size),
        shadows_(shadows) {
  }

  static void Run(scoped_refptr<IconOperation> op) {
    op->ResizeAndGenerateShadow();
  }

  void ResizeAndGenerateShadow() {
    if (cancel_flag_.IsSet())
      return;

    if (size_ != gfx::Size(bitmap_.width(), bitmap_.height())) {
      bitmap_ = skia::ImageOperations::Resize(bitmap_,
          skia::ImageOperations::RESIZE_BEST, size_.width(), size_.height());
    }

    if (cancel_flag_.IsSet())
      return;

    bitmap_ = SkBitmapOperations::CreateDropShadow(bitmap_, shadows_);
  }

  void Cancel() {
    cancel_flag_.Set();
  }

  const SkBitmap& bitmap() const {
    return bitmap_;
  }

 private:
  friend class base::RefCountedThreadSafe<AppListItemView::IconOperation>;

  base::CancellationFlag cancel_flag_;

  SkBitmap bitmap_;
  const gfx::Size size_;
  const gfx::ShadowValues shadows_;

  DISALLOW_COPY_AND_ASSIGN(IconOperation);
};

AppListItemView::AppListItemView(AppsGridView* apps_grid_view,
                                 AppListItemModel* model,
                                 views::ButtonListener* listener)
    : CustomButton(listener),
      model_(model),
      apps_grid_view_(apps_grid_view),
      icon_(new StaticImageView),
      title_(new DropShadowLabel),
      ALLOW_THIS_IN_INITIALIZER_LIST(apply_shadow_factory_(this)) {
  title_->SetBackgroundColor(kTitleBackgroundColor);
  title_->SetEnabledColor(kTitleColor);
  title_->SetFont(GetTitleFont());

  const gfx::ShadowValue kIconShadows[] = {
    gfx::ShadowValue(gfx::Point(0, 2), 2, SkColorSetARGB(0x24, 0, 0, 0)),
  };
  icon_shadows_.assign(kIconShadows, kIconShadows + arraysize(kIconShadows));

  AddChildView(icon_);
  AddChildView(title_);

  ItemIconChanged();
  ItemTitleChanged();
  model_->AddObserver(this);

  set_context_menu_controller(this);
  set_request_focus_on_press(false);
}

AppListItemView::~AppListItemView() {
  model_->RemoveObserver(this);
  CancelPendingIconOperation();
}

void AppListItemView::SetIconSize(const gfx::Size& size) {
  if (icon_size_ == size)
    return;

  icon_size_ = size;
  UpdateIcon();
}

void AppListItemView::UpdateIcon() {
  // Skip if |icon_size_| has not been determined.
  if (icon_size_.IsEmpty())
    return;

  SkBitmap icon = model_->icon();
  // Clear icon and bail out if model icon is empty.
  if (icon.empty()) {
    icon_->SetImage(NULL);
    return;
  }

  CancelPendingIconOperation();

  SkBitmap shadow;
  if (IconCache::GetInstance()->Get(icon, icon_size_, &shadow)) {
    icon_->SetImage(shadow);
  } else {
    // Schedule resize and shadow generation.
    icon_op_ = new IconOperation(icon, icon_size_, icon_shadows_);
    base::WorkerPool::PostTaskAndReply(
        FROM_HERE,
        base::Bind(&IconOperation::Run, icon_op_),
        base::Bind(&AppListItemView::ApplyShadow,
                   apply_shadow_factory_.GetWeakPtr(),
                   icon_op_),
        true /* task_is_slow */);
  }
}

void AppListItemView::CancelPendingIconOperation() {
  // Set canceled flag of previous request to skip unneeded processing.
  if (icon_op_.get())
    icon_op_->Cancel();

  // Cancel reply callback for previous request.
  apply_shadow_factory_.InvalidateWeakPtrs();
}

void AppListItemView::ApplyShadow(scoped_refptr<IconOperation> op) {
  icon_->SetImage(op->bitmap());
  IconCache::GetInstance()->Put(model_->icon(), icon_size_, op->bitmap());

  DCHECK(op.get() == icon_op_.get());
  icon_op_ = NULL;
}

void AppListItemView::ItemIconChanged() {
  UpdateIcon();
}

void AppListItemView::ItemTitleChanged() {
  title_->SetText(UTF8ToUTF16(model_->title()));
}

void AppListItemView::ItemHighlightedChanged() {
  apps_grid_view_->EnsureItemVisible(this);
  SchedulePaint();
}

std::string AppListItemView::GetClassName() const {
  return kViewClassName;
}

void AppListItemView::Layout() {
  gfx::Rect rect(GetContentsBounds());

  int left_right_padding = kLeftRightPaddingChars *
      title_->font().GetAverageCharacterWidth();
  gfx::Size title_size = title_->GetPreferredSize();

  rect.Inset(left_right_padding, kTopPadding);
  int y = rect.y();

  gfx::Rect icon_bounds(rect.x(), y, rect.width(), icon_size_.height());
  icon_bounds.Inset(gfx::ShadowValue::GetMargin(icon_shadows_));
  icon_->SetBoundsRect(icon_bounds);

  title_->SetBounds(rect.x(),
                    y + icon_size_.height() + kIconTitleSpacing,
                    rect.width(),
                    title_size.height());
}

void AppListItemView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());

  bool selected = apps_grid_view_->IsSelectedItem(this);

  if (model_->highlighted()) {
    canvas->FillRect(rect, kHighlightedColor);
  } else if (hover_animation_->is_animating()) {
    int alpha = SkColorGetA(kHoverAndPushedColor) *
        hover_animation_->GetCurrentValue();
    canvas->FillRect(rect, SkColorSetA(kHoverAndPushedColor, alpha));
  } else if (state() == BS_HOT || state() == BS_PUSHED) {
    canvas->FillRect(rect, kHoverAndPushedColor);
  } else if (selected) {
    canvas->FillRect(rect, kSelectedColor);
  }
}

void AppListItemView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = UTF8ToUTF16(model_->title());
}

void AppListItemView::ShowContextMenuForView(views::View* source,
                                             const gfx::Point& point) {
  ui::MenuModel* menu_model = model_->GetContextMenuModel();
  if (!menu_model)
    return;

  views::MenuModelAdapter menu_adapter(menu_model);
  context_menu_runner_.reset(
      new views::MenuRunner(new views::MenuItemView(&menu_adapter)));
  menu_adapter.BuildMenu(context_menu_runner_->GetMenu());
  if (context_menu_runner_->RunMenuAt(
          GetWidget(), NULL, gfx::Rect(point, gfx::Size()),
          views::MenuItemView::TOPLEFT, views::MenuRunner::HAS_MNEMONICS) ==
      views::MenuRunner::MENU_DELETED)
    return;
}

void AppListItemView::StateChanged() {
  if (state() == BS_HOT || state() == BS_PUSHED) {
    apps_grid_view_->SetSelectedItem(this);
    title_->SetEnabledColor(kTitleHoverColor);
  } else {
    apps_grid_view_->ClearSelectedItem(this);
    model_->SetHighlighted(false);
    title_->SetEnabledColor(kTitleColor);
  }
}

}  // namespace app_list
