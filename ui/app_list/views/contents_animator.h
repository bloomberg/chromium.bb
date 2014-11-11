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
  ContentsAnimator(ContentsView* contents_view);

  virtual ~ContentsAnimator();

  // Gets the name of the animator, for testing.
  virtual std::string NameForTests() const = 0;

  // Updates the state of the ContentsView. |progress| is the amount of progress
  // made on the animation, where 0.0 is the beginning and 1.0 is the end. The
  // animation should be linear (not eased in or out) so that it can be played
  // forwards or reversed. Easing will be applied by the PaginationModel.
  virtual void Update(double progress, int from_page, int to_page) = 0;

 protected:
  ContentsView* contents_view() { return contents_view_; }

  // Gets the origin (the off-screen resting place) for a given launcher page
  // with index |page_index|.
  gfx::Rect GetOffscreenPageBounds(int page_index) const;

 private:
  ContentsView* contents_view_;

  DISALLOW_COPY_AND_ASSIGN(ContentsAnimator);
};

// Simple animator that slides pages in and out vertically. Appropriate for any
// page pair.
class DefaultAnimator : public ContentsAnimator {
 public:
  DefaultAnimator(ContentsView* contents_view);

  ~DefaultAnimator() override {}

  // ContentsAnimator overrides:
  std::string NameForTests() const override;
  void Update(double progress, int from_page, int to_page) override;

  DISALLOW_COPY_AND_ASSIGN(DefaultAnimator);
};

// Animator between the start page and apps grid page.
class StartToAppsAnimator : public ContentsAnimator {
 public:
  StartToAppsAnimator(ContentsView* contents_view);

  ~StartToAppsAnimator() override {}

  // ContentsAnimator overrides:
  std::string NameForTests() const override;
  void Update(double progress, int start_page, int apps_page) override;

  DISALLOW_COPY_AND_ASSIGN(StartToAppsAnimator);
};

}  // app_list

#endif  // UI_APP_LIST_VIEWS_CONTENTS_ANIMATOR_H_
