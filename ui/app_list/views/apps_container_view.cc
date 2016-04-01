// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/apps_container_view.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/folder_background_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace app_list {

namespace {

#if !defined(OS_CHROMEOS)
// Deprecation notice dimensions.
const int kDeprecationBannerLabelSpacingPx = 6;
const int kDeprecationBannerPaddingPx = 16;
const int kDeprecationBannerMarginPx = 12;
const SkColor kDeprecationBannerBackgroundColor =
    SkColorSetRGB(0xff, 0xfd, 0xe7);
const SkColor kDeprecationBannerBorderColor = SkColorSetRGB(0xc1, 0xc1, 0xc1);
const int kDeprecationBannerBorderThickness = 1;
const int kDeprecationBannerBorderCornerRadius = 2;
// Relative to the platform-default font size.
const int kDeprecationBannerTitleSize = 2;
const int kDeprecationBannerTextSize = 0;
const SkColor kLinkColor = SkColorSetRGB(0x33, 0x67, 0xd6);

// A background that paints a filled rounded rectangle.
class RoundedRectBackground : public views::Background {
 public:
  RoundedRectBackground(SkColor color, int corner_radius)
      : color_(color), corner_radius_(corner_radius) {}
  ~RoundedRectBackground() override {}

  // views::Background overrides:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    gfx::Rect bounds = view->GetContentsBounds();

    SkPaint paint;
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    paint.setColor(color_);
    canvas->DrawRoundRect(bounds, corner_radius_, paint);
  }

 private:
  SkColor color_;
  int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(RoundedRectBackground);
};

base::string16 GetDeprecationText(const AppListViewDelegate& delegate) {
  size_t message_break;
  base::string16 text = delegate.GetMessageText(&message_break);
  base::string16 apps_shortcut_name = delegate.GetAppsShortcutName();

  // TODO(mgiuca): Insert the Apps shortcut with an image, rather than just
  // concatenating it into the string.
  text.insert(message_break, apps_shortcut_name);
  return text;
}

views::View* BuildDeprecationNotice(const AppListViewDelegate& delegate,
                                    views::StyledLabelListener* listener) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::FontList& title_font =
      rb.GetFontListWithDelta(kDeprecationBannerTitleSize);
  const gfx::FontList& text_font =
      rb.GetFontListWithDelta(kDeprecationBannerTextSize);

  base::string16 title = delegate.GetMessageTitle();
  views::Label* title_label = new views::Label(title);
  title_label->SetMultiLine(true);
  title_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title_label->SetFontList(title_font);
  title_label->SetBackgroundColor(kDeprecationBannerBackgroundColor);

  base::string16 text = GetDeprecationText(delegate);
  base::string16 learn_more = delegate.GetLearnMoreText();
  base::string16 message = text + base::ASCIIToUTF16(" ") + learn_more;
  size_t learn_more_start = text.size() + 1;
  size_t learn_more_end = learn_more_start + learn_more.size();
  views::StyledLabel* text_label = new views::StyledLabel(message, listener);
  auto learn_more_style = views::StyledLabel::RangeStyleInfo::CreateForLink();
  learn_more_style.color = kLinkColor;
  if (learn_more.size()) {
    text_label->AddStyleRange(gfx::Range(learn_more_start, learn_more_end),
                              learn_more_style);
  }
  text_label->SetDisplayedOnBackgroundColor(kDeprecationBannerBackgroundColor);
  text_label->SetBaseFontList(text_font);

  views::View* deprecation_banner_box = new views::View;
  deprecation_banner_box->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, kDeprecationBannerPaddingPx,
      kDeprecationBannerPaddingPx, kDeprecationBannerLabelSpacingPx));
  deprecation_banner_box->AddChildView(title_label);
  deprecation_banner_box->AddChildView(text_label);
  deprecation_banner_box->set_background(new RoundedRectBackground(
      kDeprecationBannerBackgroundColor, kDeprecationBannerBorderCornerRadius));
  deprecation_banner_box->SetBorder(views::Border::CreateRoundedRectBorder(
      kDeprecationBannerBorderThickness, kDeprecationBannerBorderCornerRadius,
      kDeprecationBannerBorderColor));

  views::View* deprecation_banner_view = new views::View;
  deprecation_banner_view->AddChildView(deprecation_banner_box);
  deprecation_banner_view->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, kDeprecationBannerMarginPx,
      kDeprecationBannerMarginPx, 0));

  return deprecation_banner_view;
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace

AppsContainerView::AppsContainerView(AppListMainView* app_list_main_view,
                                     AppListModel* model)
    : show_state_(SHOW_NONE),
      view_delegate_(app_list_main_view->view_delegate()),
      top_icon_animation_pending_count_(0) {
#if !defined(OS_CHROMEOS)
  deprecation_banner_view_ = BuildDeprecationNotice(*view_delegate_, this);
  AddChildView(deprecation_banner_view_);
#endif  // !defined(OS_CHROMEOS)

  apps_grid_view_ = new AppsGridView(app_list_main_view);
  int cols;
  int rows;
  if (switches::IsExperimentalAppListEnabled()) {
    cols = kExperimentalPreferredCols;
    rows = kExperimentalPreferredRows;
  } else if (app_list_main_view->ShouldCenterWindow()) {
    cols = kCenteredPreferredCols;
    rows = kCenteredPreferredRows;
  } else {
    cols = kPreferredCols;
    rows = kPreferredRows;
  }
  apps_grid_view_->SetLayout(cols, rows);
  AddChildView(apps_grid_view_);

  folder_background_view_ = new FolderBackgroundView();
  AddChildView(folder_background_view_);

  app_list_folder_view_ =
      new AppListFolderView(this, model, app_list_main_view);
  // The folder view is initially hidden.
  app_list_folder_view_->SetVisible(false);
  AddChildView(app_list_folder_view_);

  apps_grid_view_->SetModel(model);
  apps_grid_view_->SetItemList(model->top_level_item_list());
  SetShowState(SHOW_APPS, false);
}

AppsContainerView::~AppsContainerView() {
}

void AppsContainerView::ShowActiveFolder(AppListFolderItem* folder_item) {
  // Prevent new animations from starting if there are currently animations
  // pending. This fixes crbug.com/357099.
  if (top_icon_animation_pending_count_)
    return;

  app_list_folder_view_->SetAppListFolderItem(folder_item);
  SetShowState(SHOW_ACTIVE_FOLDER, false);

  CreateViewsForFolderTopItemsAnimation(folder_item, true);

  apps_grid_view_->ClearAnySelectedView();
}

void AppsContainerView::ShowApps(AppListFolderItem* folder_item) {
  if (top_icon_animation_pending_count_)
    return;

  PrepareToShowApps(folder_item);
  SetShowState(SHOW_APPS, true);
}

void AppsContainerView::ResetForShowApps() {
  SetShowState(SHOW_APPS, false);
  folder_background_view_->UpdateFolderContainerBubble(
      FolderBackgroundView::NO_BUBBLE);
}

void AppsContainerView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  apps_grid_view()->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
  app_list_folder_view()->items_grid_view()->
      SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void AppsContainerView::ReparentFolderItemTransit(
    AppListFolderItem* folder_item) {
  if (top_icon_animation_pending_count_)
    return;

  PrepareToShowApps(folder_item);
  SetShowState(SHOW_ITEM_REPARENT, false);
}

bool AppsContainerView::IsInFolderView() const {
  return show_state_ == SHOW_ACTIVE_FOLDER;
}

void AppsContainerView::ReparentDragEnded() {
  DCHECK_EQ(SHOW_ITEM_REPARENT, show_state_);
  show_state_ = AppsContainerView::SHOW_APPS;
}

gfx::Size AppsContainerView::GetPreferredSize() const {
  const int deprecation_banner_height =
      deprecation_banner_view_
          ? deprecation_banner_view_->GetPreferredSize().height()
          : 0;
  const gfx::Size grid_size = apps_grid_view_->GetPreferredSize();
  const gfx::Size folder_view_size = app_list_folder_view_->GetPreferredSize();

  int width = std::max(grid_size.width(), folder_view_size.width());
  int height = std::max(grid_size.height() + deprecation_banner_height,
                        folder_view_size.height());
  return gfx::Size(width, height);
}

void AppsContainerView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  int deprecation_banner_height = 0;
  if (deprecation_banner_view_) {
    deprecation_banner_height =
        deprecation_banner_view_->GetPreferredSize().height();
    gfx::Size deprecation_banner_size(rect.width(), deprecation_banner_height);
    deprecation_banner_view_->SetBoundsRect(
        gfx::Rect(rect.origin(), deprecation_banner_size));
  }

  switch (show_state_) {
    case SHOW_APPS:
      rect.Inset(0, deprecation_banner_height, 0, 0);
      apps_grid_view_->SetBoundsRect(rect);
      break;
    case SHOW_ACTIVE_FOLDER:
      folder_background_view_->SetBoundsRect(rect);
      app_list_folder_view_->SetBoundsRect(rect);
      break;
    case SHOW_ITEM_REPARENT:
      break;
    default:
      NOTREACHED();
  }
}

bool AppsContainerView::OnKeyPressed(const ui::KeyEvent& event) {
  if (show_state_ == SHOW_APPS)
    return apps_grid_view_->OnKeyPressed(event);
  else
    return app_list_folder_view_->OnKeyPressed(event);
}

void AppsContainerView::OnWillBeShown() {
  apps_grid_view()->ClearAnySelectedView();
  app_list_folder_view()->items_grid_view()->ClearAnySelectedView();
}

gfx::Rect AppsContainerView::GetPageBoundsForState(
    AppListModel::State state) const {
  gfx::Rect onscreen_bounds = GetDefaultContentsBounds();
  if (state == AppListModel::STATE_APPS)
    return onscreen_bounds;

  return GetBelowContentsOffscreenBounds(onscreen_bounds.size());
}

void AppsContainerView::OnTopIconAnimationsComplete() {
  --top_icon_animation_pending_count_;

  if (!top_icon_animation_pending_count_) {
    // Clean up the transitional views used for top item icon animation.
    top_icon_views_.clear();

    // Show the folder icon when closing the folder.
    if ((show_state_ == SHOW_APPS || show_state_ == SHOW_ITEM_REPARENT) &&
        apps_grid_view_->activated_folder_item_view()) {
      apps_grid_view_->activated_folder_item_view()->SetVisible(true);
    }
  }
}

void AppsContainerView::StyledLabelLinkClicked(views::StyledLabel* label,
                                               const gfx::Range& range,
                                               int event_flags) {
#if !defined(OS_CHROMEOS)
  // The only style label is the "Learn more" link in the deprecation banner, so
  // assume that that's what was clicked.
  view_delegate_->OpenLearnMoreLink();
#endif  // !defined(OS_CHROMEOS)
}

void AppsContainerView::SetShowState(ShowState show_state,
                                     bool show_apps_with_animation) {
  if (show_state_ == show_state)
    return;

  show_state_ = show_state;

  switch (show_state_) {
    case SHOW_APPS:
      if (deprecation_banner_view_)
        deprecation_banner_view_->SetVisible(true);
      folder_background_view_->SetVisible(false);
      if (show_apps_with_animation) {
        app_list_folder_view_->ScheduleShowHideAnimation(false, false);
        apps_grid_view_->ScheduleShowHideAnimation(true);
      } else {
        app_list_folder_view_->HideViewImmediately();
        apps_grid_view_->ResetForShowApps();
      }
      break;
    case SHOW_ACTIVE_FOLDER:
      if (deprecation_banner_view_)
        deprecation_banner_view_->SetVisible(false);
      folder_background_view_->SetVisible(true);
      apps_grid_view_->ScheduleShowHideAnimation(false);
      app_list_folder_view_->ScheduleShowHideAnimation(true, false);
      break;
    case SHOW_ITEM_REPARENT:
      if (deprecation_banner_view_)
        deprecation_banner_view_->SetVisible(true);
      folder_background_view_->SetVisible(false);
      folder_background_view_->UpdateFolderContainerBubble(
          FolderBackgroundView::NO_BUBBLE);
      app_list_folder_view_->ScheduleShowHideAnimation(false, true);
      apps_grid_view_->ScheduleShowHideAnimation(true);
      break;
    default:
      NOTREACHED();
  }

  app_list_folder_view_->SetBackButtonLabel(IsInFolderView());
  Layout();
}

std::vector<gfx::Rect> AppsContainerView::GetTopItemIconBoundsInActiveFolder() {
  // Get the active folder's icon bounds relative to AppsContainerView.
  AppListItemView* folder_item_view =
      apps_grid_view_->activated_folder_item_view();
  gfx::Rect to_grid_view = folder_item_view->ConvertRectToParent(
      folder_item_view->GetIconBounds());
  gfx::Rect to_container = apps_grid_view_->ConvertRectToParent(to_grid_view);

  return FolderImage::GetTopIconsBounds(to_container);
}

void AppsContainerView::CreateViewsForFolderTopItemsAnimation(
    AppListFolderItem* active_folder,
    bool open_folder) {
  top_icon_views_.clear();
  std::vector<gfx::Rect> top_items_bounds =
      GetTopItemIconBoundsInActiveFolder();
  top_icon_animation_pending_count_ =
      std::min(kNumFolderTopItems, active_folder->item_list()->item_count());
  for (size_t i = 0; i < top_icon_animation_pending_count_; ++i) {
    if (active_folder->GetTopIcon(i).isNull())
      continue;

    TopIconAnimationView* icon_view = new TopIconAnimationView(
        active_folder->GetTopIcon(i), top_items_bounds[i], open_folder);
    icon_view->AddObserver(this);
    top_icon_views_.push_back(icon_view);

    // Add the transitional views into child views, and set its bounds to the
    // same location of the item in the folder list view.
    AddChildView(top_icon_views_[i]);
    top_icon_views_[i]->SetBoundsRect(
        app_list_folder_view_->ConvertRectToParent(
            app_list_folder_view_->GetItemIconBoundsAt(i)));
    static_cast<TopIconAnimationView*>(top_icon_views_[i])->TransformView();
  }
}

void AppsContainerView::PrepareToShowApps(AppListFolderItem* folder_item) {
  if (folder_item)
    CreateViewsForFolderTopItemsAnimation(folder_item, false);

  // Hide the active folder item until the animation completes.
  if (apps_grid_view_->activated_folder_item_view())
    apps_grid_view_->activated_folder_item_view()->SetVisible(false);
}

}  // namespace app_list
