// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_tile_item_view.h"

#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/vector_icons/vector_icons.h"
#include "ui/app_list/views/search_result_container_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace app_list {

namespace {

constexpr int kSearchTileWidth = 80;
constexpr int kSearchTileTopPadding = 4;
constexpr int kSearchTitleSpacing = 5;
constexpr int kSearchPriceSize = 37;
constexpr int kSearchRatingSize = 26;
constexpr int kSearchRatingStarSize = 12;
constexpr int kSearchRatingStarHorizontalSpacing = 1;
constexpr int kSearchRatingStarVerticalSpacing = 2;

constexpr SkColor kSearchTitleColor =
    SkColorSetARGBMacro(0xDF, 0x00, 0x00, 0x00);
constexpr SkColor kSearchAppRatingColor =
    SkColorSetARGBMacro(0x8F, 0x00, 0x00, 0x00);
constexpr SkColor kSearchAppPriceColor =
    SkColorSetARGBMacro(0xFF, 0x0F, 0x9D, 0x58);
constexpr SkColor kSearchRatingStarColor =
    SkColorSetARGBMacro(0x8F, 0x00, 0x00, 0x00);

}  // namespace

SearchResultTileItemView::SearchResultTileItemView(
    SearchResultContainerView* result_container,
    AppListViewDelegate* view_delegate,
    PaginationModel* pagination_model,
    bool is_suggested_app,
    bool is_fullscreen_app_list_enabled,
    bool is_play_store_search_enabled)
    : is_suggested_app_(is_suggested_app),
      result_container_(result_container),
      view_delegate_(view_delegate),
      pagination_model_(pagination_model),
      is_fullscreen_app_list_enabled_(is_fullscreen_app_list_enabled) {
  // When |item_| is null, the tile is invisible. Calling SetSearchResult with a
  // non-null item makes the tile visible.
  SetVisible(false);

  if (is_play_store_search_enabled) {
    const gfx::FontList& font = FullscreenAppListAppTitleFont();
    rating_ = new views::Label;
    rating_->SetEnabledColor(kSearchAppRatingColor);
    rating_->SetFontList(font);
    rating_->SetLineHeight(font.GetHeight());
    rating_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
    rating_->SetVisible(false);
    AddChildView(rating_);

    rating_star_ = new views::ImageView;
    rating_star_->set_can_process_events_within_subtree(false);
    rating_star_->SetVerticalAlignment(views::ImageView::LEADING);
    rating_star_->SetImage(gfx::CreateVectorIcon(
        kIcBadgeRatingIcon, kSearchRatingStarSize, kSearchRatingStarColor));
    rating_star_->SetVisible(false);
    AddChildView(rating_star_);

    price_ = new views::Label;
    price_->SetEnabledColor(kSearchAppPriceColor);
    price_->SetFontList(font);
    price_->SetLineHeight(font.GetHeight());
    price_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    price_->SetVisible(false);
    AddChildView(price_);
  }

  set_context_menu_controller(this);
}

SearchResultTileItemView::~SearchResultTileItemView() {
  if (item_)
    item_->RemoveObserver(this);
}

void SearchResultTileItemView::SetSearchResult(SearchResult* item) {
  // Handle the case where this may be called from a nested run loop while its
  // context menu is showing. This cancels the menu (it's for the old item).
  context_menu_runner_.reset();

  SetVisible(!!item);

  SearchResult* old_item = item_;
  if (old_item)
    old_item->RemoveObserver(this);

  item_ = item;

  if (!item)
    return;

  item_->AddObserver(this);

  SetTitle(item_->title());
  SetRating(item_->rating());
  SetPrice(item_->formatted_price());

  if (is_fullscreen_app_list_enabled_) {
    const gfx::FontList& font = FullscreenAppListAppTitleFont();
    if (item_->display_type() == SearchResult::DISPLAY_RECOMMENDATION) {
      set_is_recommendation(true);

      title()->SetFontList(font);
      title()->SetLineHeight(font.GetHeight());
      title()->SetEnabledColor(kGridTitleColorFullscreen);
    } else if (item_->display_type() == SearchResult::DISPLAY_TILE) {
      // Set solid color background to avoid broken text. See crbug.com/746563.
      if (rating_) {
        rating_->SetBackground(
            views::CreateSolidBackground(kCardBackgroundColorFullscreen));
      }
      if (price_) {
        price_->SetBackground(
            views::CreateSolidBackground(kCardBackgroundColorFullscreen));
      }
      title()->SetBackground(
          views::CreateSolidBackground(kCardBackgroundColorFullscreen));
      title()->SetFontList(font);
      title()->SetLineHeight(font.GetHeight());
      title()->SetEnabledColor(kSearchTitleColor);
    }

    title()->SetMaxLines(2);
    title()->SetMultiLine(item_->display_type() == SearchResult::DISPLAY_TILE &&
                          item_->result_type() ==
                              SearchResult::RESULT_INSTALLED_APP);
  }

  // Only refresh the icon if it's different from the old one. This prevents
  // flickering.
  // If the new icon is null, it's being decoded asynchronously. Not updating it
  // now to prevent flickering from showing an empty icon while decoding.
  if (!item->icon().isNull() &&
      (!old_item || !item->icon().BackedBySameObjectAs(old_item->icon()))) {
    OnIconChanged();
  }
  if (!old_item ||
      !item->badge_icon().BackedBySameObjectAs(old_item->badge_icon())) {
    OnBadgeIconChanged();
  }

  base::string16 accessible_name = title()->text();
  if (rating_ && rating_->visible()) {
    accessible_name +=
        base::UTF8ToUTF16(", ") +
        l10n_util::GetStringFUTF16(IDS_APP_ACCESSIBILITY_STAR_RATING_ARC,
                                   rating_->text());
  }
  if (price_ && price_->visible())
    accessible_name += base::UTF8ToUTF16(", ") + price_->text();
  SetAccessibleName(accessible_name);
}

void SearchResultTileItemView::SetRating(float rating) {
  if (!rating_)
    return;

  if (rating < 0) {
    rating_->SetVisible(false);
    rating_star_->SetVisible(false);
    return;
  }

  rating_->SetText(base::FormatDouble(rating, 1));
  rating_->SetVisible(true);
  rating_star_->SetVisible(true);
}

void SearchResultTileItemView::SetPrice(const base::string16& price) {
  if (!price_)
    return;

  if (price.empty()) {
    price_->SetVisible(false);
    return;
  }

  price_->SetText(price);
  price_->SetVisible(true);
}

void SearchResultTileItemView::LogAppLaunch() const {
  // Only log the app launch if the class is being used as a suggested app.
  if (!is_suggested_app_)
    return;

  UMA_HISTOGRAM_BOOLEAN(is_fullscreen_app_list_enabled_
                            ? kAppListAppLaunchedFullscreen
                            : kAppListAppLaunched,
                        is_suggested_app_);
}

void SearchResultTileItemView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  LogAppLaunch();
  view_delegate_->OpenSearchResult(item_, false, event.flags());
}

bool SearchResultTileItemView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN) {
    LogAppLaunch();
    view_delegate_->OpenSearchResult(item_, false, event.flags());
    return true;
  }

  return false;
}

void SearchResultTileItemView::OnFocus() {
  if (pagination_model_ && is_recommendation() &&
      view_delegate_->GetModel()->state() == AppListModel::STATE_APPS) {
    // Go back to first page when app in suggestions container is focused.
    pagination_model_->SelectPage(0, false);
  } else if (!is_recommendation()) {
    ScrollRectToVisible(GetLocalBounds());
  }
  TileItemView::OnFocus();
}

void SearchResultTileItemView::OnIconChanged() {
  SetIcon(item_->icon());
  Layout();
}

void SearchResultTileItemView::OnBadgeIconChanged() {
  SetBadgeIcon(item_->badge_icon());
  Layout();
}

void SearchResultTileItemView::OnRatingChanged() {
  SetRating(item_->rating());
}

void SearchResultTileItemView::OnFormattedPriceChanged() {
  SetPrice(item_->formatted_price());
}

void SearchResultTileItemView::OnResultDestroying() {
  // The menu comes from |item_|. If we're showing a menu we need to cancel it.
  context_menu_runner_.reset();

  if (item_)
    item_->RemoveObserver(this);

  SetSearchResult(nullptr);
}

void SearchResultTileItemView::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // |item_| could be null when result list is changing.
  if (!item_)
    return;

  ui::MenuModel* menu_model = item_->GetContextMenuModel();
  if (!menu_model)
    return;

  if (!selected())
    result_container_->ClearSelectedIndex();

  context_menu_runner_.reset(
      new views::MenuRunner(menu_model, views::MenuRunner::HAS_MNEMONICS));
  context_menu_runner_->RunMenuAt(GetWidget(), nullptr,
                                  gfx::Rect(point, gfx::Size()),
                                  views::MENU_ANCHOR_TOPLEFT, source_type);
}

void SearchResultTileItemView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  if (!is_fullscreen_app_list_enabled_ || !item_) {
    TileItemView::Layout();
    return;
  }

  if (item_->display_type() == SearchResult::DISPLAY_RECOMMENDATION) {
    rect.Inset(0, kGridIconTopPadding, 0, 0);
    icon()->SetBoundsRect(rect);

    rect.Inset(kGridTitleHorizontalPadding,
               kGridIconDimension + kGridTitleSpacing,
               kGridTitleHorizontalPadding, 0);
    rect.set_height(title()->GetPreferredSize().height());
    title()->SetBoundsRect(rect);
  } else if (item_->display_type() == SearchResult::DISPLAY_TILE) {
    rect.Inset(0, kSearchTileTopPadding, 0, 0);
    icon()->SetBoundsRect(rect);

    if (badge()) {
      gfx::Rect badge_rect(rect);
      gfx::Size icon_size = icon()->GetImage().size();
      badge_rect.Offset(
          (icon_size.width() - kAppBadgeIconSize) / 2,
          icon_size.height() - kBadgeBackgroundRadius - kAppBadgeIconSize / 2);
      badge()->SetBoundsRect(badge_rect);
    }

    rect.Inset(0, kGridIconDimension + kSearchTitleSpacing, 0, 0);
    rect.set_height(title()->GetPreferredSize().height());
    title()->SetBoundsRect(rect);

    if (rating_) {
      gfx::Rect rating_rect(rect);
      rating_rect.Inset(0, title()->GetPreferredSize().height(), 0, 0);
      rating_rect.set_size(rating_->GetPreferredSize());
      rating_rect.set_width(kSearchRatingSize);
      rating_->SetBoundsRect(rating_rect);
    }

    if (rating_star_) {
      gfx::Rect rating_star_rect(rect);
      rating_star_rect.Inset(
          kSearchRatingSize + kSearchRatingStarHorizontalSpacing,
          title()->GetPreferredSize().height() +
              kSearchRatingStarVerticalSpacing,
          0, 0);
      rating_star_rect.set_size(rating_star_->GetPreferredSize());
      rating_star_->SetBoundsRect(rating_star_rect);
    }

    if (price_) {
      gfx::Rect price_rect(rect);
      price_rect.Inset(rect.width() - kSearchPriceSize,
                       title()->GetPreferredSize().height(), 0, 0);
      price_rect.set_size(price_->GetPreferredSize());
      price_->SetBoundsRect(price_rect);
    }
  } else {
    TileItemView::Layout();
  }
}

gfx::Size SearchResultTileItemView::CalculatePreferredSize() const {
  if (is_fullscreen_app_list_enabled_ && item_) {
    if (item_->display_type() == SearchResult::DISPLAY_RECOMMENDATION)
      return gfx::Size(kGridTileWidth, kGridTileHeight);
    if (item_->display_type() == SearchResult::DISPLAY_TILE)
      return gfx::Size(kSearchTileWidth, kSearchTileHeight);
  }

  return TileItemView::CalculatePreferredSize();
}

void SearchResultTileItemView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  TileItemView::GetAccessibleNodeData(node_data);
  // Specify |ui::AX_ATTR_DESCRIPTION| with an empty string, so that long
  // truncated names are not read twice.
  // Details of this issue:
  // - The Play Store app's name is shown in a label |TileItemView::title_|.
  // - If the name is too long, it'll get truncated and the full name will
  //   go to the label's tooltip.
  // - |app_list::TileItemView| uses that label's tooltip as its tooltip.
  // - If a view doesn't have |ui::AX_ATTR_DESCRIPTION| defined in the
  //   |AXNodeData|, |AXViewObjWrapper::Serialize| will use the tooltip text
  //   as its description.
  // - We're customizing this view's accessible name, so it get focused
  //   ChromeVox will read its accessible name and then its description.
  node_data->AddStringAttribute(ui::AX_ATTR_DESCRIPTION, "");
}

}  // namespace app_list
