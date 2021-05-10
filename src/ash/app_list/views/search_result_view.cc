// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_view.h"

#include <algorithm>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/remove_query_confirmation_dialog.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_actions_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/bind.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

namespace {

constexpr int kPreferredWidth = 640;
constexpr int kPreferredHeight = 48;
constexpr int kPreferredIconViewWidth = 56;
constexpr int kTextTrailPadding = 16;
// Extra margin at the right of the rightmost action icon.
constexpr int kActionButtonRightMargin = 8;
// Text line height in the search result.
constexpr int kTitleLineHeight = 20;
constexpr int kDetailsLineHeight = 16;

// URL color.
constexpr SkColor kUrlColor = gfx::kGoogleBlue600;
// The width of the focus bar.
constexpr int kFocusBarWidth = 3;

// Delta applied to font size of all AppListSearchResult titles.
constexpr int kSearchResultTitleTextSizeDelta = 2;

// Corner radius for downloaded image icons.
constexpr int kImageIconCornerRadius = 4;

class RoundedCornerImageView : public views::ImageView {
 public:
  RoundedCornerImageView() = default;

  RoundedCornerImageView(const RoundedCornerImageView&) = delete;
  RoundedCornerImageView& operator=(const RoundedCornerImageView&) = delete;

 protected:
  // views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override {
    SkPath mask;
    mask.addRoundRect(gfx::RectToSkRect(GetImageBounds()),
                      kImageIconCornerRadius, kImageIconCornerRadius);
    canvas->ClipPath(mask, true);
    ImageView::OnPaint(canvas);
  }
};

}  // namespace

// static
const char SearchResultView::kViewClassName[] = "ui/app_list/SearchResultView";

SearchResultView::SearchResultView(SearchResultListView* list_view,
                                   AppListViewDelegate* view_delegate)
    : list_view_(list_view), view_delegate_(view_delegate) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetCallback(base::BindRepeating(&SearchResultView::OnButtonPressed,
                                  base::Unretained(this)));

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  image_icon_ = AddChildView(std::make_unique<RoundedCornerImageView>());
  badge_icon_ = AddChildView(std::make_unique<views::ImageView>());
  auto* actions_view =
      AddChildView(std::make_unique<SearchResultActionsView>(this));
  set_actions_view(actions_view);

  icon_->SetCanProcessEventsWithinSubtree(false);
  image_icon_->SetCanProcessEventsWithinSubtree(false);
  badge_icon_->SetCanProcessEventsWithinSubtree(false);

  set_context_menu_controller(this);
  SetNotifyEnterExitOnChild(true);
}

SearchResultView::~SearchResultView() = default;

void SearchResultView::OnResultChanged() {
  OnMetadataChanged();
  UpdateTitleText();
  UpdateDetailsText();
  SchedulePaint();
}

void SearchResultView::UpdateTitleText() {
  if (!result() || result()->title().empty())
    title_text_.reset();
  else
    CreateTitleRenderText();

  UpdateAccessibleName();
}

void SearchResultView::UpdateDetailsText() {
  if (!result() || result()->details().empty())
    details_text_.reset();
  else
    CreateDetailsRenderText();

  UpdateAccessibleName();
}

void SearchResultView::CreateTitleRenderText() {
  std::unique_ptr<gfx::RenderText> render_text =
      gfx::RenderText::CreateRenderText();
  render_text->SetText(result()->title());
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  render_text->SetFontList(
      rb.GetFontList(AppListConfig::instance().search_result_title_font_style())
          .DeriveWithSizeDelta(kSearchResultTitleTextSizeDelta));
  // When result is an omnibox non-url search, the matched tag indicates
  // proposed query. For all other cases, the matched tag indicates typed search
  // query.
  render_text->SetColor(AppListColorProvider::Get()->GetSearchBoxTextColor(
      kDeprecatedSearchBoxTextDefaultColor));
  const SearchResult::Tags& tags = result()->title_tags();
  for (const auto& tag : tags) {
    if (tag.styles & SearchResult::Tag::URL) {
      render_text->ApplyColor(kUrlColor, tag.range);
    } else if (tag.styles & SearchResult::Tag::MATCH) {
      render_text->ApplyColor(
          AppListColorProvider::Get()->GetSearchBoxTextColor(
              kDeprecatedSearchBoxTextDefaultColor),
          tag.range);
    }
  }
  title_text_ = std::move(render_text);
}

void SearchResultView::CreateDetailsRenderText() {
  if (result()->is_omnibox_search() &&
      !app_list_features::IsOmniboxRichEntitiesEnabled()) {
    // Ensures single line row for omnibox non-url search result.
    details_text_.reset();
    return;
  }
  std::unique_ptr<gfx::RenderText> render_text =
      gfx::RenderText::CreateRenderText();
  render_text->SetText(result()->details());
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  render_text->SetFontList(rb.GetFontList(ui::ResourceBundle::BaseFont));
  render_text->SetColor(AppListColorProvider::Get()->GetSearchBoxTextColor(
      kDeprecatedSearchBoxTextDefaultColor));
  const SearchResult::Tags& tags = result()->details_tags();
  for (const auto& tag : tags) {
    if (tag.styles & SearchResult::Tag::URL)
      render_text->ApplyColor(kUrlColor, tag.range);
  }
  details_text_ = std::move(render_text);
}

void SearchResultView::OnQueryRemovalAccepted(bool accepted) {
  if (accepted) {
    list_view_->SearchResultActionActivated(
        this, OmniBoxZeroStateAction::kRemoveSuggestion);
  }

  if (confirm_remove_by_long_press_) {
    confirm_remove_by_long_press_ = false;
    SetSelected(false, base::nullopt);
  }

  RecordZeroStateSearchResultRemovalHistogram(
      accepted ? ZeroStateSearchResutRemovalConfirmation::kRemovalConfirmed
               : ZeroStateSearchResutRemovalConfirmation::kRemovalCanceled);
}

const char* SearchResultView::GetClassName() const {
  return kViewClassName;
}

gfx::Size SearchResultView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredWidth, kPreferredHeight);
}

void SearchResultView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect icon_bounds(rect);

  const bool use_image_icon =
      IsRichImage() && !image_icon_->GetImage().isNull();
  views::ImageView* icon = use_image_icon ? image_icon_ : icon_;
  int left_right_padding =
      (kPreferredIconViewWidth - icon->GetImage().width()) / 2;
  int top_bottom_padding = (rect.height() - icon->GetImage().height()) / 2;
  icon_bounds.set_width(kPreferredIconViewWidth);
  icon_bounds.Inset(left_right_padding, top_bottom_padding);
  icon_bounds.Intersect(rect);
  icon->SetBoundsRect(icon_bounds);

  gfx::Rect badge_icon_bounds;

  const int badge_icon_dimension =
      AppListConfig::instance().search_list_badge_icon_dimension();
  badge_icon_bounds = gfx::Rect(icon_bounds.right() - badge_icon_dimension / 2,
                                icon_bounds.bottom() - badge_icon_dimension / 2,
                                badge_icon_dimension, badge_icon_dimension);

  badge_icon_bounds.Intersect(rect);
  badge_icon_->SetBoundsRect(badge_icon_bounds);

  const int max_actions_width =
      (rect.right() - kActionButtonRightMargin - icon_bounds.right()) / 2;
  int actions_width =
      std::min(max_actions_width, actions_view()->GetPreferredSize().width());

  gfx::Rect actions_bounds(rect);
  actions_bounds.set_x(rect.right() - kActionButtonRightMargin - actions_width);
  actions_bounds.set_width(actions_width);
  actions_view()->SetBoundsRect(actions_bounds);
}

bool SearchResultView::OnKeyPressed(const ui::KeyEvent& event) {
  // result() could be null when result list is changing.
  if (!result())
    return false;

  switch (event.key_code()) {
    case ui::VKEY_RETURN:
      if (actions_view()->HasSelectedAction()) {
        OnSearchResultActionActivated(static_cast<OmniBoxZeroStateAction>(
            actions_view()->GetSelectedAction()));
      } else {
        list_view_->SearchResultActivated(this, event.flags(),
                                          false /* by_button_press */);
      }
      return true;
    case ui::VKEY_DELETE:
    case ui::VKEY_BROWSER_BACK:
      // Allows alt+(back or delete) to trigger the 'remove result' dialog.
      OnSearchResultActionActivated(OmniBoxZeroStateAction::kRemoveSuggestion);
      return true;
    default:
      return false;
  }
}

void SearchResultView::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect content_rect(rect);
  gfx::Rect text_bounds(rect);
  text_bounds.set_x(kPreferredIconViewWidth);
  if (actions_view()->GetVisible()) {
    text_bounds.set_width(
        rect.width() - kPreferredIconViewWidth - kTextTrailPadding -
        actions_view()->bounds().width() -
        (actions_view()->children().empty() ? 0 : kActionButtonRightMargin));
  } else {
    text_bounds.set_width(rect.width() - kPreferredIconViewWidth -
                          kTextTrailPadding - kActionButtonRightMargin);
  }
  text_bounds.set_x(
      GetMirroredXWithWidthInView(text_bounds.x(), text_bounds.width()));

  if (title_text_ && details_text_) {
    gfx::Size title_size(text_bounds.width(), kTitleLineHeight);
    gfx::Size details_size(text_bounds.width(), kDetailsLineHeight);
    int total_height = title_size.height() + details_size.height();
    int y = text_bounds.y() + (text_bounds.height() - total_height) / 2;

    title_text_->SetDisplayRect(
        gfx::Rect(gfx::Point(text_bounds.x(), y), title_size));
    title_text_->Draw(canvas);

    y += title_size.height();
    details_text_->SetDisplayRect(
        gfx::Rect(gfx::Point(text_bounds.x(), y), details_size));
    details_text_->Draw(canvas);
  } else if (title_text_) {
    gfx::Size title_size(text_bounds.width(),
                         title_text_->GetStringSize().height());
    gfx::Rect centered_title_rect(text_bounds);
    centered_title_rect.ClampToCenteredSize(title_size);
    title_text_->SetDisplayRect(centered_title_rect);
    title_text_->Draw(canvas);
  }

  // Possibly call FillRect a second time (these colours are partially
  // transparent, so the previous FillRect is not redundant).
  if (selected() && !actions_view()->HasSelectedAction()) {
    // Fill search result view row item.
    const AppListColorProvider* color_provider = AppListColorProvider::Get();
    const SkColor bg_color = color_provider->GetSearchBoxBackgroundColor();
    canvas->FillRect(
        content_rect,
        SkColorSetA(
            color_provider->GetRippleAttributesBaseColor(bg_color),
            color_provider->GetRippleAttributesHighlightOpacity(bg_color) *
                255));

    SkPath path;
    gfx::Rect focus_ring_bounds = content_rect;
    focus_ring_bounds.set_x(focus_ring_bounds.x() - kFocusBarWidth);
    focus_ring_bounds.set_width(kFocusBarWidth * 2);
    path.addRRect(SkRRect::MakeRectXY(RectToSkRect(focus_ring_bounds),
                                      kFocusBarWidth, kFocusBarWidth));
    canvas->ClipPath(path, true);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(AppListColorProvider::Get()->GetFocusRingColor());
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kFocusBarWidth);
    gfx::Point top_point = content_rect.origin();
    gfx::Point bottom_point =
        top_point + gfx::Vector2d(0, content_rect.height());
    canvas->DrawLine(top_point, bottom_point, flags);
  }
}

void SearchResultView::OnMouseEntered(const ui::MouseEvent& event) {
  actions_view()->UpdateButtonsOnStateChanged();
}

void SearchResultView::OnMouseExited(const ui::MouseEvent& event) {
  actions_view()->UpdateButtonsOnStateChanged();
}

void SearchResultView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (!GetVisible())
    return;

  // Mark the result is a list item in the list of search results.
  // Also avoids an issue with the nested button case(append and remove
  // button are child button of SearchResultView), which is not supported by
  // ChromeVox. see details in crbug.com/924776.
  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, selected());
  node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);
  node_data->SetName(GetAccessibleName());
}

void SearchResultView::VisibilityChanged(View* starting_from, bool is_visible) {
  NotifyAccessibilityEvent(ax::mojom::Event::kLayoutComplete, true);
}

void SearchResultView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_LONG_PRESS:
      if (actions_view()->IsValidActionIndex(
              OmniBoxZeroStateAction::kRemoveSuggestion)) {
        ScrollRectToVisible(GetLocalBounds());
        SetSelected(true, base::nullopt);
        confirm_remove_by_long_press_ = true;
        OnSearchResultActionActivated(
            OmniBoxZeroStateAction::kRemoveSuggestion);
        event->SetHandled();
      }
      break;
    default:
      break;
  }
  if (!event->handled())
    Button::OnGestureEvent(event);
}

void SearchResultView::OnMetadataChanged() {
  // Updates |icon_|.
  // Note: this might leave the view with an old icon. But it is needed to avoid
  // flash when a SearchResult's icon is loaded asynchronously. In this case, it
  // looks nicer to keep the stale icon for a little while on screen instead of
  // clearing it out. It should work correctly as long as the SearchResult does
  // not forget to SetIcon when it's ready.
  if (result() && !result()->icon().isNull()) {
    if (IsRichImage()) {
      gfx::ImageSkia image = result()->icon();

      // Images could be rectangular, and we should preserve the aspect ratio.
      const int dimension =
          AppListConfig::instance().search_list_image_icon_dimension();
      int width = image.width();
      int height = image.height();
      if (width != height) {
        const int max = std::max(width, height);
        width = dimension * width / max;
        height = dimension * height / max;
        SetIconImage(image, image_icon_, gfx::Size(width, height));
      } else {
        SetIconImage(image, image_icon_, gfx::Size(dimension, dimension));
      }

      icon_->SetVisible(false);
      image_icon_->SetVisible(true);
    } else {
      const int dimension =
          IsAnswer()
              ? AppListConfig::instance().search_list_answer_icon_dimension()
              : AppListConfig::instance().search_list_icon_dimension();
      SetIconImage(result()->icon(), icon_, gfx::Size(dimension, dimension));
      icon_->SetVisible(true);
      image_icon_->SetVisible(false);
    }
  }

  // Updates |badge_icon_|.
  const gfx::ImageSkia badge_icon(result() ? result()->badge_icon()
                                           : gfx::ImageSkia());
  if (badge_icon.isNull()) {
    badge_icon_->SetVisible(false);
  } else {
    const int dimension =
        AppListConfig::instance().search_list_badge_icon_dimension();
    SetIconImage(badge_icon, badge_icon_, gfx::Size(dimension, dimension));
    badge_icon_->SetVisible(true);
  }

  // Updates |actions_view()|.
  actions_view()->SetActions(result() ? result()->actions()
                                      : SearchResult::Actions());
}

void SearchResultView::OnButtonPressed(const ui::Event& event) {
  list_view_->SearchResultActivated(this, event.flags(),
                                    true /* by_button_press */);
}

void SearchResultView::SetIconImage(const gfx::ImageSkia& source,
                                    views::ImageView* const icon,
                                    const gfx::Size& size) {
  gfx::ImageSkia image(source);
  image = gfx::ImageSkiaOperations::CreateResizedImage(
      source, skia::ImageOperations::RESIZE_BEST, size);
  icon->SetImage(image);
  icon->SetImageSize(size);
}

void SearchResultView::OnSearchResultActionActivated(size_t index) {
  // |result()| could be nullptr when result list is changing.
  if (!result())
    return;

  DCHECK_LT(index, result()->actions().size());

  if (result()->is_omnibox_search()) {
    OmniBoxZeroStateAction button_action = GetOmniBoxZeroStateAction(index);

    if (button_action == OmniBoxZeroStateAction::kRemoveSuggestion) {
      RecordZeroStateSearchResultUserActionHistogram(
          ZeroStateSearchResultUserActionType::kRemoveResult);
      auto dialog = std::make_unique<RemoveQueryConfirmationDialog>(
          result()->title(),
          base::BindOnce(&SearchResultView::OnQueryRemovalAccepted,
                         weak_ptr_factory_.GetWeakPtr()));
      list_view_->app_list_main_view()
          ->contents_view()
          ->search_results_page_view()
          ->ShowAnchoredDialog(std::move(dialog));
    } else if (button_action == OmniBoxZeroStateAction::kAppendSuggestion) {
      RecordZeroStateSearchResultUserActionHistogram(
          ZeroStateSearchResultUserActionType::kAppendResult);
      list_view_->SearchResultActionActivated(this, index);
    }
  }
}

bool SearchResultView::IsSearchResultHoveredOrSelected() {
  return IsMouseHovered() || selected();
}

void SearchResultView::OnMenuClosed() {
  // Release menu since its menu model delegate (AppContextMenu) could be
  // released as a result of menu command execution.
  context_menu_.reset();
}

void SearchResultView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // |result()| could be nullptr when result list is changing.
  if (!result())
    return;

  view_delegate_->GetSearchResultContextMenuModel(
      result()->id(), base::BindOnce(&SearchResultView::OnGetContextMenu,
                                     weak_ptr_factory_.GetWeakPtr(), source,
                                     point, source_type));
}

void SearchResultView::OnGetContextMenu(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type,
    std::unique_ptr<ui::SimpleMenuModel> menu_model) {
  if (!menu_model || (context_menu_ && context_menu_->IsShowingMenu()))
    return;

  AppLaunchedMetricParams metric_params = {
      AppListLaunchedFrom::kLaunchedFromSearchBox,
      AppListLaunchType::kSearchResult};
  view_delegate_->GetAppLaunchedMetricParams(&metric_params);

  context_menu_ = std::make_unique<AppListMenuModelAdapter>(
      std::string(), std::move(menu_model), GetWidget(), source_type,
      metric_params, AppListMenuModelAdapter::SEARCH_RESULT,
      base::BindOnce(&SearchResultView::OnMenuClosed,
                     weak_ptr_factory_.GetWeakPtr()),
      view_delegate_->GetSearchModel()->tablet_mode());
  context_menu_->Run(gfx::Rect(point, gfx::Size()),
                     views::MenuAnchorPosition::kTopLeft,
                     views::MenuRunner::HAS_MNEMONICS);
  source->RequestFocus();
}

bool SearchResultView::IsAnswer() const {
  return app_list_features::IsOmniboxRichEntitiesEnabled() && result() &&
         result()->omnibox_type() == SearchResultOmniboxType::kAnswer;
}

bool SearchResultView::IsRichImage() const {
  return app_list_features::IsOmniboxRichEntitiesEnabled() && result() &&
         result()->omnibox_type() == SearchResultOmniboxType::kRichImage;
}

}  // namespace ash
