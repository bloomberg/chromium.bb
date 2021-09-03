// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/vr/model/location_bar_state.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/ui_pixel_test.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace vr {

namespace {

constexpr gfx::Transform kIdentity;

}  // namespace

TEST_F(UiPixelTest, DrawVrBrowsingMode) {
#if defined(OS_WIN)
  // VR is not supported on Windows 7.
  if (base::win::GetVersion() <= base::win::Version::WIN7)
    return;
#endif
  // Set up scene.
  UiInitialState ui_initial_state;
  ui_initial_state.in_web_vr = false;
  MakeUi(ui_initial_state,
         LocationBarState(GURL("https://example.com"), security_state::SECURE,
                          &vector_icons::kHttpsValidIcon, true, false));

  // Draw UI.
  DrawUi(gfx::Vector3dF(0.0f, 0.0f, -1.0f), gfx::Point3F(0.5f, -0.5f, 0.0f),
         ControllerModel::ButtonState::kUp, 1.0f, kIdentity, kIdentity,
         GetPixelDaydreamProjMatrix());

  // Read pixels into SkBitmap.
  auto bitmap = SaveCurrentFrameBufferToSkBitmap();
  EXPECT_TRUE(bitmap);
}

}  // namespace vr
