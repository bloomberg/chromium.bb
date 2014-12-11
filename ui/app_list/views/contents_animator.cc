// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/contents_animator.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/start_page_view.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace app_list {

// ContentsAnimator

ContentsAnimator::ContentsAnimator(ContentsView* contents_view)
    : contents_view_(contents_view) {
}

ContentsAnimator::~ContentsAnimator() {
}

gfx::Rect ContentsAnimator::GetOnscreenPageBounds(int page_index) const {
  return contents_view_->IsStateActive(AppListModel::STATE_CUSTOM_LAUNCHER_PAGE)
             ? contents_view_->GetContentsBounds()
             : contents_view_->GetDefaultContentsBounds();
}

gfx::Rect ContentsAnimator::GetOffscreenPageBounds(int page_index) const {
  gfx::Rect bounds(contents_view_->GetContentsBounds());
  // The start page and search page origins are above; all other pages' origins
  // are below.
  int page_height = bounds.height();
  bool origin_above = contents_view_->GetPageIndexForState(
                          AppListModel::STATE_START) == page_index ||
                      contents_view_->GetPageIndexForState(
                          AppListModel::STATE_SEARCH_RESULTS) == page_index;
  bounds.set_y(origin_above ? -page_height : page_height);
  return bounds;
}

void ContentsAnimator::UpdateCustomPageForDefaultAnimation(double progress,
                                                           int from_page,
                                                           int to_page) const {
  int custom_page_index = contents_view()->GetPageIndexForState(
      AppListModel::STATE_CUSTOM_LAUNCHER_PAGE);
  if (custom_page_index < 0)
    return;

  int start_page_index =
      contents_view()->GetPageIndexForState(AppListModel::STATE_START);
  if (from_page != start_page_index && to_page != start_page_index)
    return;

  views::View* custom_page = contents_view()->GetPageView(custom_page_index);
  gfx::Rect custom_page_collapsed(
      contents_view()->GetCustomPageCollapsedBounds());
  gfx::Rect custom_page_origin(GetOffscreenPageBounds(custom_page_index));
  gfx::Rect custom_page_rect;

  if (from_page == start_page_index) {
    // When transitioning from start page -> any other page, move the custom
    // page from collapsed to hidden. (This method is not used by the start page
    // -> custom page transition.)
    custom_page_rect = gfx::Tween::RectValueBetween(
        progress, custom_page_collapsed, custom_page_origin);
  } else {
    // When transitioning from any page -> start page, move the custom page from
    // hidden to collapsed.
    custom_page_rect = gfx::Tween::RectValueBetween(
        progress, custom_page_origin, custom_page_collapsed);
  }

  custom_page->SetBoundsRect(custom_page_rect);
}

void ContentsAnimator::UpdateSearchBoxForDefaultAnimation(double progress,
                                                          int from_page,
                                                          int to_page) const {
  if (!switches::IsExperimentalAppListEnabled())
    return;

  // These are in ContentsView coordinates.
  gfx::Rect search_box_from(
      contents_view()->GetSearchBoxBoundsForPageIndex(from_page));
  gfx::Rect search_box_to(
      contents_view()->GetSearchBoxBoundsForPageIndex(to_page));

  gfx::Rect search_box_rect =
      gfx::Tween::RectValueBetween(progress, search_box_from, search_box_to);

  views::View* search_box = contents_view()->GetSearchBoxView();
  search_box->GetWidget()->SetBounds(
      contents_view()->ConvertRectToWidget(search_box_rect));
}

void ContentsAnimator::ClipSearchResultsPageToOnscreenBounds(
    int page_index,
    const gfx::Rect& current_bounds,
    const gfx::Rect& onscreen_bounds) {
  int search_results_index =
      contents_view()->GetPageIndexForState(AppListModel::STATE_SEARCH_RESULTS);
  if (page_index != search_results_index)
    return;

  contents_view()
      ->GetPageView(page_index)
      ->set_clip_insets(current_bounds.InsetsFrom(onscreen_bounds));
}

// DefaultAnimator

DefaultAnimator::DefaultAnimator(ContentsView* contents_view)
    : ContentsAnimator(contents_view) {
}

std::string DefaultAnimator::NameForTests() const {
  return "DefaultAnimator";
}

void DefaultAnimator::Update(double progress, int from_page, int to_page) {
  // Move the from page from 0 to its origin. Move the to page from its origin
  // to 0.
  gfx::Rect from_page_onscreen(GetOnscreenPageBounds(from_page));
  gfx::Rect to_page_onscreen(GetOnscreenPageBounds(to_page));
  gfx::Rect from_page_origin(GetOffscreenPageBounds(from_page));
  gfx::Rect to_page_origin(GetOffscreenPageBounds(to_page));
  gfx::Rect from_page_rect(gfx::Tween::RectValueBetween(
      progress, from_page_onscreen, from_page_origin));
  gfx::Rect to_page_rect(
      gfx::Tween::RectValueBetween(progress, to_page_origin, to_page_onscreen));

  contents_view()->GetPageView(from_page)->SetBoundsRect(from_page_rect);
  ClipSearchResultsPageToOnscreenBounds(from_page, from_page_rect,
                                        from_page_onscreen);

  contents_view()->GetPageView(to_page)->SetBoundsRect(to_page_rect);
  ClipSearchResultsPageToOnscreenBounds(to_page, to_page_rect,
                                        to_page_onscreen);

  UpdateCustomPageForDefaultAnimation(progress, from_page, to_page);
  UpdateSearchBoxForDefaultAnimation(progress, from_page, to_page);
}

// StartToAppsAnimator

StartToAppsAnimator::StartToAppsAnimator(ContentsView* contents_view)
    : ContentsAnimator(contents_view) {
}

std::string StartToAppsAnimator::NameForTests() const {
  return "StartToAppsAnimator";
}

void StartToAppsAnimator::Update(double progress,
                                 int start_page,
                                 int apps_page) {
  // TODO(mgiuca): This is just a clone of DefaultAnimator's animation. Write a
  // custom animation for the All Apps button on the Start page.
  gfx::Rect on_screen(contents_view()->GetDefaultContentsBounds());
  gfx::Rect from_page_origin(GetOffscreenPageBounds(start_page));
  gfx::Rect to_page_origin(GetOffscreenPageBounds(apps_page));
  gfx::Rect from_page_rect(
      gfx::Tween::RectValueBetween(progress, on_screen, from_page_origin));
  gfx::Rect to_page_rect(
      gfx::Tween::RectValueBetween(progress, to_page_origin, on_screen));

  contents_view()->GetPageView(start_page)->SetBoundsRect(from_page_rect);
  contents_view()->GetPageView(apps_page)->SetBoundsRect(to_page_rect);

  UpdateCustomPageForDefaultAnimation(progress, start_page, apps_page);
  UpdateSearchBoxForDefaultAnimation(progress, start_page, apps_page);
}

// StartToCustomAnimator

StartToCustomAnimator::StartToCustomAnimator(ContentsView* contents_view)
    : ContentsAnimator(contents_view) {
}

std::string StartToCustomAnimator::NameForTests() const {
  return "StartToCustomAnimator";
}

void StartToCustomAnimator::Update(double progress,
                                   int start_page,
                                   int custom_page) {
  gfx::Rect start_page_on_screen(GetOnscreenPageBounds(start_page));
  gfx::Rect custom_page_on_screen(GetOnscreenPageBounds(custom_page));
  gfx::Rect start_page_origin(GetOffscreenPageBounds(start_page));
  gfx::Rect custom_page_origin(contents_view()->GetCustomPageCollapsedBounds());
  gfx::Rect start_page_rect(gfx::Tween::RectValueBetween(
      progress, start_page_on_screen, start_page_origin));
  gfx::Rect custom_page_rect(gfx::Tween::RectValueBetween(
      progress, custom_page_origin, custom_page_on_screen));

  contents_view()->GetPageView(start_page)->SetBoundsRect(start_page_rect);
  contents_view()->GetPageView(custom_page)->SetBoundsRect(custom_page_rect);

  UpdateSearchBoxForDefaultAnimation(progress, start_page, custom_page);
}

}  // namespace app_list
