// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_arc_terms_of_service_screen.h"

using ::testing::AtLeast;
using ::testing::NotNull;

namespace chromeos {

MockArcTermsOfServiceScreen::MockArcTermsOfServiceScreen(
    BaseScreenDelegate* base_screen_delegate,
    ArcTermsOfServiceScreenView* view)
    : ArcTermsOfServiceScreen(base_screen_delegate, view) {}

MockArcTermsOfServiceScreen::~MockArcTermsOfServiceScreen() = default;

MockArcTermsOfServiceScreenView::MockArcTermsOfServiceScreenView() = default;

MockArcTermsOfServiceScreenView::~MockArcTermsOfServiceScreenView() {
  if (observer_)
    observer_->OnViewDestroyed(this);
}

void MockArcTermsOfServiceScreenView::AddObserver(
    ArcTermsOfServiceScreenViewObserver* observer) {
  observer_ = observer;
  MockAddObserver(observer);
}

}  // namespace chromeos
