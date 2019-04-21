// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/flag_warning/flag_warning_tray.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/ui_base_features.h"

namespace ash {
namespace {

using FlagWarningTrayTest = AshTestBase;

TEST_F(FlagWarningTrayTest, VisibleForMash) {
  // Simulate enabling Mash.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {::features::kMash} /* enabled */,
      {::features::kSingleProcessMash} /* disabled */);

  StatusAreaWidget widget(Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                              kShellWindowId_StatusContainer),
                          GetPrimaryShelf());
  widget.Initialize();
  FlagWarningTray* tray = widget.flag_warning_tray_for_testing();
  ASSERT_TRUE(tray);
  EXPECT_TRUE(tray->visible());
}

}  // namespace
}  // namespace ash
