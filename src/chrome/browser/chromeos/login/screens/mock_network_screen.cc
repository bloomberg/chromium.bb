// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_network_screen.h"

namespace chromeos {

using ::testing::AtLeast;
using ::testing::_;

MockNetworkScreen::MockNetworkScreen(BaseScreenDelegate* base_screen_delegate,
                                     NetworkScreenView* view)
    : NetworkScreen(base_screen_delegate, view) {}

MockNetworkScreen::~MockNetworkScreen() = default;

MockNetworkScreenView::MockNetworkScreenView() {
  EXPECT_CALL(*this, MockBind(_)).Times(AtLeast(1));
}

MockNetworkScreenView::~MockNetworkScreenView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockNetworkScreenView::Bind(NetworkScreen* screen) {
  screen_ = screen;
  MockBind(screen);
}

void MockNetworkScreenView::Unbind() {
  screen_ = nullptr;
  MockUnbind();
}

}  // namespace chromeos
