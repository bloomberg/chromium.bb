// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/form_submission_tracker_util.h"

#include "base/logging.h"
#include "components/password_manager/core/browser/form_submission_observer.h"

namespace password_manager {

void NotifyDidNavigateMainFrame(bool is_renderer_initiated,
                                ui::PageTransition transition,
                                bool has_user_gesture,
                                FormSubmissionObserver* observer) {
  DCHECK(observer);

  // Password manager is interested in
  // - form submission navigations,
  // - any JavaScript initiated navigations (i.e. renderer initiated navigations
  // without user gesture), because many form submissions are done with
  // JavaScript.
  // Password manager is not interested in
  // - browser initiated navigations (e.g. reload, bookmark click),
  // - hyperlink navigations (which are renderer navigations with user gesture).
  bool form_may_be_submitted =
      is_renderer_initiated &&
      (ui::PageTransitionCoreTypeIs(transition,
                                    ui::PAGE_TRANSITION_FORM_SUBMIT) ||
       !has_user_gesture);

  observer->DidNavigateMainFrame(form_may_be_submitted);
}

}  // namespace password_manager
