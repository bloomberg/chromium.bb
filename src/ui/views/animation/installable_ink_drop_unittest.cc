// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/installable_ink_drop.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/animation/ink_drop_state.h"

namespace views {

TEST(InstallableInkDropTest, UpdatesState) {
  InstallableInkDrop installable_ink_drop;

  // Initial state should be HIDDEN.
  EXPECT_EQ(installable_ink_drop.GetTargetInkDropState(), InkDropState::HIDDEN);

  installable_ink_drop.AnimateToState(InkDropState::ACTIVATED);
  EXPECT_EQ(installable_ink_drop.GetTargetInkDropState(),
            InkDropState::ACTIVATED);
}

}  // namespace views
