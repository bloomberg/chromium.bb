// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_connection.h"

#include <fuchsia/scenic/scheduling/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>

#include <string>

#include "base/fuchsia/scoped_service_publisher.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/flatland/tests/fake_flatland.h"

namespace ui {
namespace {

std::string GetCurrentTestName() {
  return ::testing::UnitTest::GetInstance()->current_test_info()->name();
}

// Fixture to exercise Present() logic in FlatlandConnection.
class FlatlandConnectionTest : public ::testing::Test {
 protected:
  FlatlandConnectionTest()
      : fake_flatland_publisher_(test_context_.additional_services(),
                                 fake_flatland_.GetRequestHandler()) {}
  ~FlatlandConnectionTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  FakeFlatland fake_flatland_;

 private:
  base::TestComponentContextForProcess test_context_;
  // Injects binding for responding to Flatland protocol connection requests.
  const base::ScopedServicePublisher<fuchsia::ui::composition::Flatland>
      fake_flatland_publisher_;
};

TEST_F(FlatlandConnectionTest, Initialization) {
  // Create the FlatlandConnection but don't pump the loop.  No FIDL calls are
  // completed yet.
  const std::string debug_name = GetCurrentTestName();
  FlatlandConnection flatland_connection(debug_name);
  EXPECT_EQ(fake_flatland_.debug_name(), "");

  // Ensure the debug name is set.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_flatland_.debug_name(), debug_name);
}

TEST_F(FlatlandConnectionTest, BasicPresent) {
  // Set up callbacks which allow sensing of how many presents were handled.
  size_t presents_called = 0u;
  fake_flatland_.SetPresentHandler(base::BindLambdaForTesting(
      [&presents_called](fuchsia::ui::composition::PresentArgs present_args) {
        presents_called++;
      }));

  // Create the FlatlandConnection but don't pump the loop.  No FIDL calls are
  // completed yet.
  FlatlandConnection flatland_connection(GetCurrentTestName());
  EXPECT_EQ(presents_called, 0u);

  flatland_connection.Present();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(presents_called, 1u);
}

TEST_F(FlatlandConnectionTest, RespectsPresentCredits) {
  // Set up callbacks which allow sensing of how many presents were handled.
  size_t presents_called = 0u;
  fake_flatland_.SetPresentHandler(base::BindLambdaForTesting(
      [&presents_called](fuchsia::ui::composition::PresentArgs present_args) {
        presents_called++;
      }));

  // Create the FlatlandConnection but don't pump the loop.  No FIDL calls are
  // completed yet.
  FlatlandConnection flatland_connection(GetCurrentTestName());
  EXPECT_EQ(presents_called, 0u);

  flatland_connection.Present();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(presents_called, 1u);

  // Next Present should fail because we dont have credits.
  flatland_connection.Present();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(presents_called, 1u);

  // Give additional present credits.
  fuchsia::ui::composition::OnNextFrameBeginValues on_next_frame_begin_values;
  on_next_frame_begin_values.set_additional_present_credits(1);
  fake_flatland_.FireOnNextFrameBeginEvent(
      std::move(on_next_frame_begin_values));
  task_environment_.RunUntilIdle();

  flatland_connection.Present();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(presents_called, 2u);
}

TEST_F(FlatlandConnectionTest, ReleaseFences) {
  // Set up callbacks which allow sensing of how many presents were handled.
  size_t presents_called = 0u;
  zx_handle_t release_fence_handle;
  fake_flatland_.SetPresentHandler(base::BindLambdaForTesting(
      [&presents_called, &release_fence_handle](
          fuchsia::ui::composition::PresentArgs present_args) {
        presents_called++;
        release_fence_handle =
            present_args.release_fences().empty()
                ? ZX_HANDLE_INVALID
                : present_args.release_fences().front().get();
      }));

  // Create the FlatlandConnection but don't pump the loop.  No FIDL calls are
  // completed yet.
  FlatlandConnection flatland_connection(GetCurrentTestName());
  EXPECT_EQ(presents_called, 0u);

  zx::event first_release_fence;
  zx::event::create(0, &first_release_fence);
  const zx_handle_t first_release_fence_handle = first_release_fence.get();
  fuchsia::ui::composition::PresentArgs present_args;
  present_args.set_requested_presentation_time(0);
  present_args.set_acquire_fences({});
  std::vector<zx::event> fences;
  fences.push_back(std::move(first_release_fence));
  present_args.set_release_fences({std::move(fences)});
  present_args.set_unsquashable(false);
  flatland_connection.Present(std::move(present_args),
                              FlatlandConnection::OnFramePresentedCallback());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(presents_called, 1u);
  EXPECT_EQ(release_fence_handle, ZX_HANDLE_INVALID);

  // Give additional present credits
  fuchsia::ui::composition::OnNextFrameBeginValues on_next_frame_begin_values;
  on_next_frame_begin_values.set_additional_present_credits(1);
  fake_flatland_.FireOnNextFrameBeginEvent(
      std::move(on_next_frame_begin_values));
  task_environment_.RunUntilIdle();

  flatland_connection.Present();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(presents_called, 2u);
  EXPECT_EQ(release_fence_handle, first_release_fence_handle);
}

}  // namespace
}  // namespace ui
