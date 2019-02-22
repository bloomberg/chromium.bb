// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/flag_warning/flag_warning_tray.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/ui_base_features.h"

namespace ash {
namespace {

class FlagWarningTrayTest : public AshTestBase {
 public:
  FlagWarningTrayTest() = default;
  ~FlagWarningTrayTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(::features::kMash);
    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(FlagWarningTrayTest);
};

TEST_F(FlagWarningTrayTest, VisibleForMash) {
  FlagWarningTray* tray = Shell::GetPrimaryRootWindowController()
                              ->GetStatusAreaWidget()
                              ->flag_warning_tray_for_testing();
  ASSERT_TRUE(tray);
  EXPECT_TRUE(tray->visible());
}

}  // namespace
}  // namespace ash
