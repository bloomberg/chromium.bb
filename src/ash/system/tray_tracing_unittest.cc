// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_tracing.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_test_api.h"
#include "ash/test/ash_test_base.h"

namespace ash {
namespace {

using TrayTracingTest = AshTestBase;

// Tests that the icon becomes visible when the tray controller toggles it.
TEST_F(TrayTracingTest, Visibility) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  SystemTray* tray = GetPrimarySystemTray();
  TrayTracing* tray_tracing = SystemTrayTestApi(tray).tray_tracing();

  // The system starts with tracing off, so the icon isn't visible.
  EXPECT_FALSE(tray_tracing->tray_view()->visible());

  // Simulate turning on tracing.
  SystemTrayModel* model = Shell::Get()->system_tray_model();
  model->SetPerformanceTracingIconVisible(true);
  EXPECT_TRUE(tray_tracing->tray_view()->visible());

  // Simulate turning off tracing.
  model->SetPerformanceTracingIconVisible(false);
  EXPECT_FALSE(tray_tracing->tray_view()->visible());
}

}  // namespace
}  // namespace ash
