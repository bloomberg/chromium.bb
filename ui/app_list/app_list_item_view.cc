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
#include "ui/app_list/app_list_item_model.h"
#include "ui/app_list/app_list_model_view.h"
#include "ui/app_list/drop_shadow_label.h"
#include "ui/app_list/icon_cache.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace app_list {

namespace {

const int kTopBottomPadding = 10;
const int kIconTitleSpacing = 10;

const SkColor kTitleColor = SK_ColorWHITE;
const SkColor kTitleColorV2 = SkColorSetARGB(0xFF, 0x88, 0x88, 0x88);

// 0.33 black
const SkColor kHoverAndPushedColor = SkColorSetARGB(0x55, 0x00, 0x00, 0x00);

// 0.16 black
const SkColor kSelectedColor = SkColorSetARGB(0x2A, 0x00, 0x00, 0x00);

const SkColor kHighlightedColor = kHoverAndPushedColor;

// FontSize/IconSize ratio = 24 / 128, which means we should get 24 font size
// when icon size is 128.
const float kFontSizeToIconSizeRatio = 0.1875f;

// Font smaller than kBoldFontSize needs to be bold.
const int kBoldFontSize = 14;

const int kMinFontSize = 12;

const int kMinTitleChars = 15;

const int kLeftRightPaddingChars = 1;

const gfx::Font& GetTitleFontForIconSize(const gfx::Size& size) {
  static int icon_height;
  static gfx::Font* font = NULL;

  if (font && icon_height == size.height())
    return *font;

  delete font;

  icon_height = size.height();
  int font_size = std::max(
      static_cast<int>(icon_height * kFontSizeToIconSizeRatio),
      kMinFontSize);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::Font title_font(rb.GetFont(ui::ResourceBundle::BaseFont).GetFontName(),
                       font_size);
  if (font_size <= kBoldFontSize)
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

// A minimum title width set by test to override the default logic that derives
// the min width from font.
int g_min_title_width = 0;

}  // namespace

// static
const char AppListItemView::kViewClassName[] = "ui/app_list/AppListItemView";

// AppListItemView::IconOperation wraps background icon processing.
class AppListItemView::IconOperation
    : public base::RefCountedThreadSafe<AppListItemView::IconOperation> {
 public:
  IconOperation(const SkBitmap& bitmap, const gfx::Size& size)
      : bitmap_(bitmap),
        size_(size) {
  }

  static void Run(scoped_refptr<IconOperation> op) {
    op->ResizeAndGenerateShadow();
  }

  // Padding space around icon to contain its shadow. Note it should be at least
  // the max size of shadow radius + shadow offset in shadow generation code.
  static const int kShadowPadding = 15;

  void ResizeAndGenerateShadow() {
    // If you change shadow radius and shadow offset, please also update
    // kShadowPaddingAbove.
    const SkColor kShadowColor[] = {
      SkColorSetARGB(0xCC, 0, 0, 0),
      SkColorSetARGB(0x33, 0, 0, 0),
      SkColorSetARGB(0x4C, 0, 0, 0),
    };
    const gfx::Point kShadowOffset[] = {
      gfx::Point(0, 0),
      gfx::Point(0, 4),
      gfx::Point(0, 5),
    };
    const SkScalar kShadowRadius[] = {
      SkIntToScalar(2),
      SkIntToScalar(4),
      SkIntToScalar(10),
    };

    if (cancel_flag_.IsSet())
      return;

    if (size_ != gfx::Size(bitmap_.width(), bitmap_.height()))
      bitmap_ = SkBitmapOperations::CreateResizedBitmap(bitmap_, size_);

    if (cancel_flag_.IsSet())
      return;

    bitmap_ = SkBitmapOperations::CreateDropShadow(
        bitmap_,
        arraysize(kShadowColor),
        kShadowColor,
        kShadowOffset,
        kShadowRadius);
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

  DISALLOW_COPY_AND_ASSIGN(IconOperation);
};

AppListItemView::AppListItemView(AppListModelView* list_model_view,
                                 AppListItemModel* model,
                                 views::ButtonListener* listener)
    : CustomButton(listener),
      model_(model),
      list_model_view_(list_model_view),
      icon_(new StaticImageView),
      title_(new DropShadowLabel),
      selected_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(apply_shadow_factory_(this)) {
  title_->SetBackgroundColor(0);

  if (list_model_view_->fixed_layout()) {
    title_->SetEnabledColor(kTitleColorV2);
  } else {
    title_->SetEnabledColor(kTitleColor);
    const gfx::ShadowValue kTitleShadows[] = {
      gfx::ShadowValue(gfx::Point(0, 0), 1, SkColorSetARGB(0x66, 0, 0, 0)),
      gfx::ShadowValue(gfx::Point(0, 0), 10, SkColorSetARGB(0x66, 0, 0, 0)),
      gfx::ShadowValue(gfx::Point(0, 2), 2, SkColorSetARGB(0x66, 0, 0, 0)),
      gfx::ShadowValue(gfx::Point(0, 2), 4, SkColorSetARGB(0x66, 0, 0, 0)),
    };
    title_->SetTextShadows(arraysize(kTitleShadows), kTitleShadows);
  }

  AddChildView(icon_);
  AddChildView(title_);

  ItemIconChanged();
  ItemTitleChanged();
  model_->AddObserver(this);

  set_context_menu_controller(this);
  set_request_focus_on_press(false);
  set_focusable(true);
}

AppListItemView::~AppListItemView() {
  model_->RemoveObserver(this);
  CancelPendingIconOperation();
}

// static
gfx::Size AppListItemView::GetPreferredSizeForIconSize(
    const gfx::Size& icon_size) {
  int min_title_width = g_min_title_width;
  // Fixed 20px is used for left/right padding before switching to padding
  // based on number of chars. It is also a number used for test case
  // AppList.ModelViewCalculateLayout.
  int left_right_padding = 20;
  if (min_title_width == 0) {
    const gfx::Font& title_font = GetTitleFontForIconSize(icon_size);
    // Use big char such as 'G' to calculate min title width.
    min_title_width = kMinTitleChars *
        title_font.GetStringWidth(ASCIIToUTF16("G"));
    left_right_padding = kLeftRightPaddingChars *
        title_font.GetAverageCharacterWidth();
  }

  int dimension = std::max(icon_size.width() * 2, min_title_width);
  gfx::Size size(dimension, dimension);
  size.Enlarge(left_right_padding, kTopBottomPadding);
  return size;
}

// static
void AppListItemView::SetMinTitleWidth(int width) {
  g_min_title_width = width;
}

void AppListItemView::SetIconSize(const gfx::Size& size) {
  if (icon_size_ == size)
    return;

  icon_size_ = size;
  title_->SetFont(GetTitleFontForIconSize(size));
  UpdateIcon();
}

void AppListItemView::SetSelected(bool selected) {
  if (selected == selected_)
    return;

  RequestFocus();
  selected_ = selected;
  SchedulePaint();
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
    icon_op_ = new IconOperation(icon, icon_size_);
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
  SchedulePaint();
}

std::string AppListItemView::GetClassName() const {
  return kViewClassName;
}

gfx::Size AppListItemView::GetPreferredSize() {
  return GetPreferredSizeForIconSize(icon_size_);
}

void AppListItemView::Layout() {
  gfx::Rect rect(GetContentsBounds());

  int left_right_padding = kLeftRightPaddingChars *
      title_->font().GetAverageCharacterWidth();
  rect.Inset(left_right_padding, kTopBottomPadding);

  gfx::Size title_size = title_->GetPreferredSize();
  int height = icon_size_.height() + kIconTitleSpacing +
      title_size.height();
  int y = rect.y() + (rect.height() - height) / 2;

  gfx::Rect icon_bounds(rect.x(), y, rect.width(), icon_size_.height());
  icon_bounds.Inset(0, -IconOperation::kShadowPadding);
  icon_->SetBoundsRect(icon_bounds);

  title_->SetBounds(rect.x(),
                    y + icon_size_.height() + kIconTitleSpacing,
                    rect.width(),
                    title_size.height());
}

void AppListItemView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());

  if (model_->highlighted()) {
    canvas->FillRect(rect, kHighlightedColor);
  } else if (hover_animation_->is_animating()) {
    int alpha = SkColorGetA(kHoverAndPushedColor) *
        hover_animation_->GetCurrentValue();
    canvas->FillRect(rect, SkColorSetA(kHoverAndPushedColor, alpha));
  } else if (state() == BS_HOT || state() == BS_PUSHED) {
    canvas->FillRect(rect, kHoverAndPushedColor);
  } else if (selected_) {
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
    list_model_view_->SetSelectedItem(this);
  } else {
    list_model_view_->ClearSelectedItem(this);
    model_->SetHighlighted(false);
  }
}

}  // namespace app_list
