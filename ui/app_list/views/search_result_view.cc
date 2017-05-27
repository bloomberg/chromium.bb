// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_view.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/views/search_result_actions_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/progress_bar.h"

namespace app_list {

namespace {

const int kPreferredWidth = 300;
const int kPreferredHeight = 56;
const int kIconLeftPadding = 16;
const int kIconRightPadding = 24;
const int kTextTrailPadding = 16;
const int kSeparatorPadding = 62;
const int kBorderSize = 1;
const SkColor kSeparatorColor = SkColorSetRGB(0xE1, 0xE1, 0xE1);

// Extra margin at the right of the rightmost action icon.
const int kActionButtonRightMargin = 8;

int GetIconViewWidth() {
  return kListIconSize + kIconLeftPadding + kIconRightPadding;
}

// Creates a RenderText of given |text| and |styles|. Caller takes ownership
// of returned RenderText.
gfx::RenderText* CreateRenderText(const base::string16& text,
                                  const SearchResult::Tags& tags) {
  gfx::RenderText* render_text = gfx::RenderText::CreateInstance();
  render_text->SetText(text);
  render_text->SetColor(kResultDefaultTextColor);

  for (SearchResult::Tags::const_iterator it = tags.begin();
       it != tags.end();
       ++it) {
    // NONE means default style so do nothing.
    if (it->styles == SearchResult::Tag::NONE)
      continue;

    if (it->styles & SearchResult::Tag::MATCH)
      render_text->ApplyWeight(gfx::Font::Weight::BOLD, it->range);
    if (it->styles & SearchResult::Tag::DIM)
      render_text->ApplyColor(kResultDimmedTextColor, it->range);
    else if (it->styles & SearchResult::Tag::URL)
      render_text->ApplyColor(kResultURLTextColor, it->range);
  }

  return render_text;
}

}  // namespace

// static
const char SearchResultView::kViewClassName[] = "ui/app_list/SearchResultView";

SearchResultView::SearchResultView(SearchResultListView* list_view)
    : views::CustomButton(this),
      result_(NULL),
      is_last_result_(false),
      list_view_(list_view),
      icon_(new views::ImageView),
      badge_icon_(new views::ImageView),
      actions_view_(new SearchResultActionsView(this)),
      progress_bar_(new views::ProgressBar) {
  icon_->set_can_process_events_within_subtree(false);
  badge_icon_->set_can_process_events_within_subtree(false);

  AddChildView(icon_);
  AddChildView(badge_icon_);
  AddChildView(actions_view_);
  AddChildView(progress_bar_);
  set_context_menu_controller(this);
}

SearchResultView::~SearchResultView() {
  ClearResultNoRepaint();
}

void SearchResultView::SetResult(SearchResult* result) {
  ClearResultNoRepaint();

  result_ = result;
  if (result_)
    result_->AddObserver(this);

  OnIconChanged();
  OnBadgeIconChanged();
  OnActionsChanged();
  UpdateTitleText();
  UpdateDetailsText();
  OnIsInstallingChanged();
  OnPercentDownloadedChanged();
  SchedulePaint();
}

void SearchResultView::ClearResultNoRepaint() {
  if (result_)
    result_->RemoveObserver(this);
  result_ = NULL;
}

void SearchResultView::ClearSelectedAction() {
  actions_view_->SetSelectedAction(-1);
}

void SearchResultView::UpdateTitleText() {
  if (!result_ || result_->title().empty()) {
    title_text_.reset();
  } else {
    title_text_.reset(CreateRenderText(result_->title(),
                                       result_->title_tags()));
  }

  UpdateAccessibleName();
}

void SearchResultView::UpdateDetailsText() {
  if (!result_ || result_->details().empty()) {
    details_text_.reset();
  } else {
    details_text_.reset(CreateRenderText(result_->details(),
                                         result_->details_tags()));
  }

  UpdateAccessibleName();
}

base::string16 SearchResultView::ComputeAccessibleName() const {
  if (!result_)
    return base::string16();

  base::string16 accessible_name = result_->title();
  if (!result_->title().empty() && !result_->details().empty())
    accessible_name += base::ASCIIToUTF16(", ");
  accessible_name += result_->details();

  return accessible_name;
}

void SearchResultView::UpdateAccessibleName() {
  SetAccessibleName(ComputeAccessibleName());
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
  icon_bounds.set_width(GetIconViewWidth());
  const int top_bottom_padding = (rect.height() - kListIconSize) / 2;
  icon_bounds.Inset(kIconLeftPadding, top_bottom_padding, kIconRightPadding,
                    top_bottom_padding);
  icon_bounds.Intersect(rect);
  icon_->SetBoundsRect(icon_bounds);

  gfx::Rect badge_icon_bounds(
      icon_bounds.right() - kListBadgeIconSize + kListBadgeIconOffsetX,
      icon_bounds.bottom() - kListBadgeIconSize + kListBadgeIconOffsetY,
      kListBadgeIconSize, kListBadgeIconSize);
  badge_icon_bounds.Intersect(rect);
  badge_icon_->SetBoundsRect(badge_icon_bounds);

  const int max_actions_width =
      (rect.right() - kActionButtonRightMargin - icon_bounds.right()) / 2;
  int actions_width = std::min(max_actions_width,
                               actions_view_->GetPreferredSize().width());

  gfx::Rect actions_bounds(rect);
  actions_bounds.set_x(rect.right() - kActionButtonRightMargin - actions_width);
  actions_bounds.set_width(actions_width);
  actions_view_->SetBoundsRect(actions_bounds);

  const int progress_width = rect.width() / 5;
  const int progress_height = progress_bar_->GetPreferredSize().height();
  const gfx::Rect progress_bounds(
      rect.right() - kActionButtonRightMargin - progress_width,
      rect.y() + (rect.height() - progress_height) / 2,
      progress_width,
      progress_height);
  progress_bar_->SetBoundsRect(progress_bounds);
}

bool SearchResultView::OnKeyPressed(const ui::KeyEvent& event) {
  // |result_| could be NULL when result list is changing.
  if (!result_)
    return false;

  switch (event.key_code()) {
    case ui::VKEY_TAB: {
      int new_selected = actions_view_->selected_action()
          + (event.IsShiftDown() ? -1 : 1);
      actions_view_->SetSelectedAction(new_selected);
      return actions_view_->IsValidActionIndex(new_selected);
    }
    case ui::VKEY_RETURN: {
      int selected = actions_view_->selected_action();
      if (actions_view_->IsValidActionIndex(selected)) {
        OnSearchResultActionActivated(selected, event.flags());
      } else {
        list_view_->SearchResultActivated(this, event.flags());
      }
      return true;
    }
    default:
      break;
  }

  return false;
}

void SearchResultView::ChildPreferredSizeChanged(views::View* child) {
  Layout();
}

void SearchResultView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect content_rect(rect);
  const bool selected = list_view_->IsResultViewSelected(this);
  const bool hover = state() == STATE_HOVERED || state() == STATE_PRESSED;

  canvas->FillRect(content_rect, kCardBackgroundColor);

  // Possibly call FillRect a second time (these colours are partially
  // transparent, so the previous FillRect is not redundant).
  if (selected)
    canvas->FillRect(content_rect, kSelectedColor);
  else if (hover)
    canvas->FillRect(content_rect, kHighlightedColor);

  if (!is_last_result_) {
    gfx::Rect line_rect = content_rect;
    line_rect.set_height(kBorderSize);
    line_rect.set_y(content_rect.bottom() - kBorderSize);
    line_rect.set_x(kSeparatorPadding);
    canvas->FillRect(line_rect, kSeparatorColor);
  }

  gfx::Rect border_bottom = gfx::SubtractRects(rect, content_rect);
  canvas->FillRect(border_bottom, kResultBorderColor);

  gfx::Rect text_bounds(rect);
  text_bounds.set_x(GetIconViewWidth());
  if (actions_view_->visible()) {
    text_bounds.set_width(
        rect.width() - GetIconViewWidth() - kTextTrailPadding -
        actions_view_->bounds().width() -
        (actions_view_->has_children() ? kActionButtonRightMargin : 0));
  } else {
    text_bounds.set_width(rect.width() - GetIconViewWidth() -
                          kTextTrailPadding - progress_bar_->bounds().width() -
                          kActionButtonRightMargin);
  }
  text_bounds.set_x(GetMirroredXWithWidthInView(text_bounds.x(),
                                                text_bounds.width()));

  if (title_text_ && details_text_) {
    gfx::Size title_size(text_bounds.width(),
                         title_text_->GetStringSize().height());
    gfx::Size details_size(text_bounds.width(),
                           details_text_->GetStringSize().height());
    int total_height = title_size.height() + + details_size.height();
    int y = text_bounds.y() + (text_bounds.height() - total_height) / 2;

    title_text_->SetDisplayRect(gfx::Rect(gfx::Point(text_bounds.x(), y),
                                          title_size));
    title_text_->Draw(canvas);

    y += title_size.height();
    details_text_->SetDisplayRect(gfx::Rect(gfx::Point(text_bounds.x(), y),
                                            details_size));
    details_text_->Draw(canvas);
  } else if (title_text_) {
    gfx::Size title_size(text_bounds.width(),
                         title_text_->GetStringSize().height());
    gfx::Rect centered_title_rect(text_bounds);
    centered_title_rect.ClampToCenteredSize(title_size);
    title_text_->SetDisplayRect(centered_title_rect);
    title_text_->Draw(canvas);
  }
}

void SearchResultView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  DCHECK(sender == this);

  list_view_->SearchResultActivated(this, event.flags());
}

void SearchResultView::OnIconChanged() {
  const gfx::ImageSkia image(result_ ? result_->icon() : gfx::ImageSkia());
  // Note this might leave the view with an old icon. But it is needed to avoid
  // flash when a SearchResult's icon is loaded asynchronously. In this case, it
  // looks nicer to keep the stale icon for a little while on screen instead of
  // clearing it out. It should work correctly as long as the SearchResult does
  // not forget to SetIcon when it's ready.
  if (image.isNull())
    return;

  SetIconImage(image, icon_, kListIconSize);
}

void SearchResultView::OnBadgeIconChanged() {
  const gfx::ImageSkia image(result_ ? result_->badge_icon()
                                     : gfx::ImageSkia());
  if (image.isNull()) {
    badge_icon_->SetVisible(false);
    return;
  }

  SetIconImage(image, badge_icon_, kListBadgeIconSize);
  badge_icon_->SetVisible(true);
}

void SearchResultView::SetIconImage(const gfx::ImageSkia& source,
                                    views::ImageView* const icon,
                                    const int icon_dimension) {
  // Copy.
  gfx::ImageSkia image(source);

  // Scales down big icons but leave small ones unchanged.
  if (image.width() > icon_dimension || image.height() > icon_dimension) {
    image = gfx::ImageSkiaOperations::CreateResizedImage(
        image, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(icon_dimension, icon_dimension));
  } else {
    icon->ResetImageSize();
  }

  // Set the image to an empty image before we reset the image because
  // since we're using the same backing store for our images, sometimes
  // ImageView won't detect that we have a new image set due to the pixel
  // buffer pointers remaining the same despite the image changing.
  icon->SetImage(gfx::ImageSkia());
  icon->SetImage(image);
}

void SearchResultView::OnActionsChanged() {
  actions_view_->SetActions(result_ ? result_->actions()
                                    : SearchResult::Actions());
}

void SearchResultView::OnIsInstallingChanged() {
  const bool is_installing = result_ && result_->is_installing();
  actions_view_->SetVisible(!is_installing);
  progress_bar_->SetVisible(is_installing);
}

void SearchResultView::OnPercentDownloadedChanged() {
  progress_bar_->SetValue(result_ ? result_->percent_downloaded() / 100.0 : 0);
}

void SearchResultView::OnItemInstalled() {
  list_view_->OnSearchResultInstalled(this);
}

void SearchResultView::OnSearchResultActionActivated(size_t index,
                                                     int event_flags) {
  // |result_| could be NULL when result list is changing.
  if (!result_)
    return;

  DCHECK_LT(index, result_->actions().size());

  list_view_->SearchResultActionActivated(this, index, event_flags);
}

void SearchResultView::ShowContextMenuForView(views::View* source,
                                              const gfx::Point& point,
                                              ui::MenuSourceType source_type) {
  // |result_| could be NULL when result list is changing.
  if (!result_)
    return;

  ui::MenuModel* menu_model = result_->GetContextMenuModel();
  if (!menu_model)
    return;

  context_menu_runner_.reset(
      new views::MenuRunner(menu_model, views::MenuRunner::HAS_MNEMONICS));
  context_menu_runner_->RunMenuAt(GetWidget(), NULL,
                                  gfx::Rect(point, gfx::Size()),
                                  views::MENU_ANCHOR_TOPLEFT, source_type);
}

}  // namespace app_list
