// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_tile_item_view.h"

#include "base/i18n/number_formatting.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/views/search_result_container_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace app_list {

namespace {

constexpr int kRecommendationTileWidth = 96;
constexpr int kRecommendationTileHeight = 99;
constexpr int kRecommendationIconTopPadding = 24;
constexpr int kRecommendationTitleSpacing = 10;
constexpr int kRecommendationTileMaxWidth = 80;

constexpr int kSearchTileWidth = 80;
constexpr int kSearchTileHeight = 92;
constexpr int kSearchTileTopPadding = 4;
constexpr int kSearchTitleSpacing = 6;

constexpr SkColor kRecommendationTileColor = SK_ColorWHITE;

constexpr SkColor kSearchTitleColor = SkColorSetA(SK_ColorBLACK, 223);
constexpr SkColor kSearchAppRatingColor = SkColorSetA(SK_ColorBLACK, 143);
constexpr SkColor kSearchAppPriceColor = SkColorSetRGB(0x0F, 0x9D, 0x58);

}  // namespace

SearchResultTileItemView::SearchResultTileItemView(
    SearchResultContainerView* result_container,
    AppListViewDelegate* view_delegate)
    : result_container_(result_container),
      view_delegate_(view_delegate),
      is_fullscreen_app_list_enabled_(features::IsFullscreenAppListEnabled()) {
  // When |item_| is null, the tile is invisible. Calling SetSearchResult with a
  // non-null item makes the tile visible.
  SetVisible(false);

  if (is_fullscreen_app_list_enabled_) {
    const gfx::FontList& base_font =
        ui::ResourceBundle::GetSharedInstance().GetFontList(
            ui::ResourceBundle::BaseFont);

    rating_ = new views::Label;
    rating_->SetEnabledColor(kSearchAppRatingColor);
    rating_->SetFontList(base_font);
    rating_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    rating_->SetVisible(false);
    AddChildView(rating_);

    price_ = new views::Label;
    price_->SetEnabledColor(kSearchAppPriceColor);
    price_->SetFontList(base_font);
    price_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
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
    const gfx::FontList& base_font =
        ui::ResourceBundle::GetSharedInstance().GetFontList(
            ui::ResourceBundle::BaseFont);

    // Customize title UI
    if (item_->display_type() == SearchResult::DISPLAY_RECOMMENDATION) {
      title()->SetFontList(base_font.DeriveWithSizeDelta(1));
      title()->SetEnabledColor(kRecommendationTileColor);
    } else if (item_->display_type() == SearchResult::DISPLAY_TILE) {
      title()->SetFontList(base_font.DeriveWithSizeDelta(1));
      title()->SetEnabledColor(kSearchTitleColor);
    }
  }

  // Only refresh the icon if it's different from the old one. This prevents
  // flickering.
  if (!old_item || !item->icon().BackedBySameObjectAs(old_item->icon())) {
    OnIconChanged();
  }
}

void SearchResultTileItemView::SetRating(float rating) {
  if (!rating_)
    return;

  if (rating < 0) {
    rating_->SetVisible(false);
    return;
  }

  rating_->SetText(base::FormatDouble(rating, 1));
  rating_->SetVisible(true);
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

void SearchResultTileItemView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  view_delegate_->OpenSearchResult(item_, false, event.flags());
}

bool SearchResultTileItemView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN) {
    view_delegate_->OpenSearchResult(item_, false, event.flags());
    return true;
  }

  return false;
}

void SearchResultTileItemView::OnIconChanged() {
  SetIcon(item_->icon());
}

void SearchResultTileItemView::OnBadgeIconChanged() {
  SetBadgeIcon(item_->badge_icon());
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
    rect.Inset(0, kRecommendationIconTopPadding, 0, 0);
    icon()->SetBoundsRect(rect);

    rect.Inset(0, kGridIconDimension + kRecommendationTitleSpacing, 0, 0);
    rect.set_height(title()->GetPreferredSize().height());
    rect.set_width(kRecommendationTileMaxWidth);
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
      rating_rect.set_height(rating_->GetPreferredSize().height());
      rating_->SetBoundsRect(rating_rect);
    }

    if (price_) {
      gfx::Rect price_rect(rect);
      price_rect.Inset(0, title()->GetPreferredSize().height(), 0, 0);
      price_rect.set_height(price_->GetPreferredSize().height());
      price_->SetBoundsRect(price_rect);
    }
  } else {
    TileItemView::Layout();
  }
}

gfx::Size SearchResultTileItemView::CalculatePreferredSize() const {
  if (is_fullscreen_app_list_enabled_ && item_) {
    if (item_->display_type() == SearchResult::DISPLAY_RECOMMENDATION)
      return gfx::Size(kRecommendationTileWidth, kRecommendationTileHeight);
    if (item_->display_type() == SearchResult::DISPLAY_TILE)
      return gfx::Size(kSearchTileWidth, kSearchTileHeight);
  }

  return TileItemView::CalculatePreferredSize();
}

}  // namespace app_list
