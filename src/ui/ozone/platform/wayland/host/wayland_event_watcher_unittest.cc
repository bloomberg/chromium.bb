// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server-core.h>

#include "base/debug/crash_logging.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::Values;

namespace ui {

namespace {

std::string NumberToString(uint32_t number) {
  return base::UTF16ToUTF8(base::FormatNumber(number));
}

}  // namespace

class WaylandEventWatcherTest : public WaylandTest {
 public:
  WaylandEventWatcherTest() = default;
};

TEST_P(WaylandEventWatcherTest, CrashKeyResourceError) {
  const std::string kTestErrorString = "This is a nice error.";
  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  auto* xdg_surface = mock_surface->xdg_surface();

  // Prepare the expectation error string.
  const std::string expected_error_code =
      base::StrCat({wl_resource_get_class(xdg_surface->resource()), ": error ",
                    NumberToString(static_cast<uint32_t>(
                        XDG_SURFACE_ERROR_UNCONFIGURED_BUFFER)),
                    ": ", kTestErrorString});

  wl_resource_post_error(xdg_surface->resource(),
                         XDG_SURFACE_ERROR_UNCONFIGURED_BUFFER, "%s",
                         kTestErrorString.c_str());

  Sync();

  EXPECT_EQ(expected_error_code,
            crash_reporter::GetCrashKeyValue("wayland_error"));
}

TEST_P(WaylandEventWatcherTest, CrashKeyResourceNoMemory) {
  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  auto* xdg_surface = mock_surface->xdg_surface();

  // Prepare the expectation error string.
  const std::string expected_error_code = base::StrCat(
      {"wl_display: error ",
       NumberToString(static_cast<uint32_t>(WL_DISPLAY_ERROR_NO_MEMORY)),
       ": no memory"});

  wl_resource_post_no_memory(xdg_surface->resource());

  Sync();

  EXPECT_EQ(expected_error_code,
            crash_reporter::GetCrashKeyValue("wayland_error"));
}

TEST_P(WaylandEventWatcherTest, CrashKeyClientNoMemoryError) {
  const std::string expected_error_code = base::StrCat(
      {"wl_display: error ",
       NumberToString(static_cast<uint32_t>(WL_DISPLAY_ERROR_NO_MEMORY)),
       ": no memory"});

  wl_client_post_no_memory(server_.client());

  Sync();

  EXPECT_EQ(expected_error_code,
            crash_reporter::GetCrashKeyValue("wayland_error"));
}

TEST_P(WaylandEventWatcherTest, CrashKeyClientImplementationError) {
  const std::string kError = "A nice error.";
  const std::string expected_error_code = base::StrCat(
      {"wl_display: error ",
       NumberToString(static_cast<uint32_t>(WL_DISPLAY_ERROR_IMPLEMENTATION)),
       ": ", kError});

  wl_client_post_implementation_error(server_.client(), "%s", kError.c_str());

  Sync();

  EXPECT_EQ(expected_error_code,
            crash_reporter::GetCrashKeyValue("wayland_error"));
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandEventWatcherTest,
                         Values(wl::ServerConfig{
                             .shell_version = wl::ShellVersion::kStable}));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandEventWatcherTest,
                         Values(wl::ServerConfig{
                             .shell_version = wl::ShellVersion::kV6}));

}  // namespace ui
