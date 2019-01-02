// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/arc/arc_input_method_surface_manager.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "components/exo/input_method_surface.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/env.h"

namespace ash {

class ArcInputMethodSurfaceManagerTest : public AshTestBase {
 public:
  ArcInputMethodSurfaceManagerTest() = default;
  ~ArcInputMethodSurfaceManagerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    wm_helper_ = std::make_unique<exo::WMHelper>(Shell::Get()->aura_env());
    exo::WMHelper::SetInstance(wm_helper_.get());
  }

  void TearDown() override {
    exo::WMHelper::SetInstance(nullptr);
    wm_helper_.reset();
    AshTestBase::TearDown();
  }

 private:
  std::unique_ptr<exo::WMHelper> wm_helper_;

  DISALLOW_COPY_AND_ASSIGN(ArcInputMethodSurfaceManagerTest);
};

TEST_F(ArcInputMethodSurfaceManagerTest, AddRemoveSurface) {
  ArcInputMethodSurfaceManager manager;
  EXPECT_EQ(nullptr, manager.GetSurface());
  auto surface = std::make_unique<exo::Surface>();
  auto input_method_surface =
      std::make_unique<exo::InputMethodSurface>(nullptr, surface.get(), 1.0);
  manager.AddSurface(input_method_surface.get());
  EXPECT_EQ(input_method_surface.get(), manager.GetSurface());
  manager.RemoveSurface(input_method_surface.get());
  EXPECT_EQ(nullptr, manager.GetSurface());
}

}  // namespace ash
