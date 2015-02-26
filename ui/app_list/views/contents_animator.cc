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
#include "ui/gfx/shadow_value.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

gfx::ShadowValue GetSearchBoxShadowForState(AppListModel::State state) {
  return GetShadowForZHeight(state == AppListModel::STATE_SEARCH_RESULTS ? 1
                                                                         : 2);
}

}  // namespace

// ContentsAnimator

ContentsAnimator::ContentsAnimator(ContentsView* contents_view)
    : contents_view_(contents_view) {
}

ContentsAnimator::~ContentsAnimator() {
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
  gfx::Rect custom_page_origin(
      contents_view()->GetOffscreenPageBounds(custom_page_index));
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

  AppListModel::State from_state =
      contents_view()->GetStateForPageIndex(from_page);
  AppListModel::State to_state = contents_view()->GetStateForPageIndex(to_page);

  gfx::ShadowValue original_shadow = GetSearchBoxShadowForState(from_state);
  gfx::ShadowValue target_shadow = GetSearchBoxShadowForState(to_state);

  SearchBoxView* search_box = contents_view()->GetSearchBoxView();
  gfx::Vector2d offset(gfx::Tween::LinearIntValueBetween(
                           progress, original_shadow.x(), target_shadow.x()),
                       gfx::Tween::LinearIntValueBetween(
                           progress, original_shadow.y(), target_shadow.y()));
  search_box->SetShadow(gfx::ShadowValue(
      offset, gfx::Tween::LinearIntValueBetween(
                  progress, original_shadow.blur(), target_shadow.blur()),
      gfx::Tween::ColorValueBetween(progress, original_shadow.color(),
                                    target_shadow.color())));

  search_box->GetWidget()->SetBounds(
      search_box->GetViewBoundsForSearchBoxContentsBounds(
          contents_view()->ConvertRectToWidget(search_box_rect)));
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
  gfx::Rect from_page_onscreen(
      contents_view()->GetOnscreenPageBounds(from_page));
  gfx::Rect to_page_onscreen(contents_view()->GetOnscreenPageBounds(to_page));
  gfx::Rect from_page_origin(
      contents_view()->GetOffscreenPageBounds(from_page));
  gfx::Rect to_page_origin(contents_view()->GetOffscreenPageBounds(to_page));
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
  gfx::Rect from_page_onscreen(
      contents_view()->GetOnscreenPageBounds(start_page));
  gfx::Rect to_page_onscreen(contents_view()->GetOnscreenPageBounds(apps_page));
  gfx::Rect from_page_origin(
      contents_view()->GetOffscreenPageBounds(start_page));
  gfx::Rect to_page_origin(contents_view()->GetOffscreenPageBounds(apps_page));
  gfx::Rect from_page_rect(gfx::Tween::RectValueBetween(
      progress, from_page_onscreen, from_page_origin));
  gfx::Rect to_page_rect(
      gfx::Tween::RectValueBetween(progress, to_page_origin, to_page_onscreen));

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
  gfx::Rect start_page_on_screen(
      contents_view()->GetOnscreenPageBounds(start_page));
  gfx::Rect custom_page_on_screen(
      contents_view()->GetOnscreenPageBounds(custom_page));
  gfx::Rect start_page_origin(
      contents_view()->GetOffscreenPageBounds(start_page));
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
