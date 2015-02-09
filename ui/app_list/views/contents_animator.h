// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_CONTENTS_ANIMATOR_H_
#define UI_APP_LIST_VIEWS_CONTENTS_ANIMATOR_H_

#include <string>

#include "base/macros.h"

namespace gfx {
class Rect;
}

namespace app_list {

class ContentsView;

// Manages an animation between two specific pages of the ContentsView.
// Different subclasses implement different animations, so we can have a custom
// animation for each pair of pages. Animations are reversible (if A -> B is
// implemented, B -> A will implicitly be the inverse animation).
class ContentsAnimator {
 public:
  explicit ContentsAnimator(ContentsView* contents_view);

  virtual ~ContentsAnimator();

  // Gets the name of the animator, for testing.
  virtual std::string NameForTests() const = 0;

  // Updates the state of the ContentsView. |progress| is the amount of progress
  // made on the animation, where 0.0 is the beginning and 1.0 is the end. The
  // animation should be linear (not eased in or out) so that it can be played
  // forwards or reversed. Easing will be applied by the PaginationModel.
  virtual void Update(double progress, int from_page, int to_page) = 0;

 protected:
  const ContentsView* contents_view() const { return contents_view_; }

  // Updates the position of the custom launcher page view (if it exists), in
  // the default way for start page <-> other page transitions. This places it
  // into collapsed state on the start page, and hidden on any other page. Any
  // other behaviour should be implemented with a custom animator. |progress|,
  // |from_page| and |to_page| are parameters from Update().
  void UpdateCustomPageForDefaultAnimation(double progress,
                                           int from_page,
                                           int to_page) const;

  // Updates the position of the search box view, placing it in the correct
  // position for the transition from |from_page| to |to_page|.
  void UpdateSearchBoxForDefaultAnimation(double progress,
                                          int from_page,
                                          int to_page) const;

  // Clips the drawing of the search results page to its onscreen bounds.
  void ClipSearchResultsPageToOnscreenBounds(int page_index,
                                             const gfx::Rect& current_bounds,
                                             const gfx::Rect& onscreen_bounds);

 private:
  ContentsView* contents_view_;

  DISALLOW_COPY_AND_ASSIGN(ContentsAnimator);
};

// Simple animator that slides pages in and out vertically. Appropriate for any
// page pair.
class DefaultAnimator : public ContentsAnimator {
 public:
  explicit DefaultAnimator(ContentsView* contents_view);

  ~DefaultAnimator() override {}

  // ContentsAnimator overrides:
  std::string NameForTests() const override;
  void Update(double progress, int from_page, int to_page) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultAnimator);
};

// Animator between the start page and apps grid page.
class StartToAppsAnimator : public ContentsAnimator {
 public:
  explicit StartToAppsAnimator(ContentsView* contents_view);

  ~StartToAppsAnimator() override {}

  // ContentsAnimator overrides:
  std::string NameForTests() const override;
  void Update(double progress, int start_page, int apps_page) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StartToAppsAnimator);
};

// Animator from start page to custom page.
class StartToCustomAnimator : public ContentsAnimator {
 public:
  explicit StartToCustomAnimator(ContentsView* contents_view);

  std::string NameForTests() const override;
  void Update(double progress, int start_page, int custom_page) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StartToCustomAnimator);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_CONTENTS_ANIMATOR_H_
