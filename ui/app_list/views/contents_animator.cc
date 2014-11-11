// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/contents_animator.h"

#include "ui/app_list/views/contents_view.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

namespace app_list {

// ContentsAnimator

ContentsAnimator::ContentsAnimator(ContentsView* contents_view)
    : contents_view_(contents_view) {
}

ContentsAnimator::~ContentsAnimator() {
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
  gfx::Rect on_screen(contents_view()->GetDefaultContentsBounds());
  gfx::Rect from_page_origin(GetOffscreenPageBounds(from_page));
  gfx::Rect to_page_origin(GetOffscreenPageBounds(to_page));
  gfx::Rect from_page_rect(
      gfx::Tween::RectValueBetween(progress, on_screen, from_page_origin));
  gfx::Rect to_page_rect(
      gfx::Tween::RectValueBetween(progress, to_page_origin, on_screen));

  contents_view()->GetPageView(from_page)->SetBoundsRect(from_page_rect);
  contents_view()->GetPageView(to_page)->SetBoundsRect(to_page_rect);
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
}

}  // app_list
