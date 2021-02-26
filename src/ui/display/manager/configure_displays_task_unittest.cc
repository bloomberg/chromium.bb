// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/fake/fake_display_snapshot.h"
#include "ui/display/manager/configure_displays_task.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/test_native_display_delegate.h"

namespace display {
namespace test {

namespace {

class ConfigureDisplaysTaskTest : public testing::Test {
 public:
  ConfigureDisplaysTaskTest()
      : delegate_(&log_),
        small_mode_(gfx::Size(1366, 768), false, 60.0f),
        big_mode_(gfx::Size(2560, 1600), false, 60.0f) {}
  ~ConfigureDisplaysTaskTest() override = default;

  void SetUp() override {
    displays_.push_back(FakeDisplaySnapshot::Builder()
                            .SetId(123)
                            .SetNativeMode(small_mode_.Clone())
                            .SetCurrentMode(small_mode_.Clone())
                            .Build());
    displays_.push_back(FakeDisplaySnapshot::Builder()
                            .SetId(456)
                            .SetNativeMode(big_mode_.Clone())
                            .SetCurrentMode(big_mode_.Clone())
                            .AddMode(small_mode_.Clone())
                            .Build());
  }

  void ConfigureCallback(ConfigureDisplaysTask::Status status) {
    callback_called_ = true;
    status_ = status;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ActionLogger log_;
  TestNativeDisplayDelegate delegate_;

  bool callback_called_ = false;
  ConfigureDisplaysTask::Status status_ = ConfigureDisplaysTask::ERROR;

  const DisplayMode small_mode_;
  const DisplayMode big_mode_;

  std::vector<std::unique_ptr<DisplaySnapshot>> displays_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigureDisplaysTaskTest);
};

}  // namespace

TEST_F(ConfigureDisplaysTaskTest, ConfigureWithOneDisplay) {
  ConfigureDisplaysTask::ResponseCallback callback = base::BindOnce(
      &ConfigureDisplaysTaskTest::ConfigureCallback, base::Unretained(this));

  std::vector<DisplayConfigureRequest> requests(
      1,
      DisplayConfigureRequest(displays_[0].get(), &small_mode_, gfx::Point()));
  ConfigureDisplaysTask task(&delegate_, requests, std::move(callback));
  task.Run();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(ConfigureDisplaysTask::SUCCESS, status_);
  EXPECT_EQ(
      GetCrtcAction({displays_[0]->display_id(), gfx::Point(), &small_mode_}),
      log_.GetActionsAndClear());
}

TEST_F(ConfigureDisplaysTaskTest, ConfigureWithTwoDisplay) {
  ConfigureDisplaysTask::ResponseCallback callback = base::BindOnce(
      &ConfigureDisplaysTaskTest::ConfigureCallback, base::Unretained(this));

  std::vector<DisplayConfigureRequest> requests;
  for (const auto& display : displays_) {
    requests.emplace_back(display.get(), display->native_mode(), gfx::Point());
  }

  ConfigureDisplaysTask task(&delegate_, requests, std::move(callback));
  task.Run();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(ConfigureDisplaysTask::SUCCESS, status_);
  EXPECT_EQ(JoinActions(GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[1]->display_id(), gfx::Point(),
                                       &big_mode_})
                            .c_str(),
                        nullptr),
            log_.GetActionsAndClear());
}

TEST_F(ConfigureDisplaysTaskTest, DisableDisplayFails) {
  ConfigureDisplaysTask::ResponseCallback callback = base::BindOnce(
      &ConfigureDisplaysTaskTest::ConfigureCallback, base::Unretained(this));

  delegate_.set_max_configurable_pixels(1);

  std::vector<DisplayConfigureRequest> requests(
      1, DisplayConfigureRequest(displays_[0].get(), nullptr, gfx::Point()));
  ConfigureDisplaysTask task(&delegate_, requests, std::move(callback));
  task.Run();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(ConfigureDisplaysTask::ERROR, status_);
  EXPECT_EQ(JoinActions(GetCrtcAction(
                            {displays_[0]->display_id(), gfx::Point(), nullptr})
                            .c_str(),
                        nullptr),
            log_.GetActionsAndClear());
}

TEST_F(ConfigureDisplaysTaskTest, ConfigureWithOneDisplayFails) {
  ConfigureDisplaysTask::ResponseCallback callback = base::BindOnce(
      &ConfigureDisplaysTaskTest::ConfigureCallback, base::Unretained(this));

  delegate_.set_max_configurable_pixels(1);

  std::vector<DisplayConfigureRequest> requests(
      1, DisplayConfigureRequest(displays_[1].get(), &big_mode_, gfx::Point()));
  ConfigureDisplaysTask task(&delegate_, requests, std::move(callback));
  task.Run();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(ConfigureDisplaysTask::ERROR, status_);
  EXPECT_EQ(JoinActions(GetCrtcAction({displays_[1]->display_id(), gfx::Point(),
                                       &big_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[1]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        nullptr),
            log_.GetActionsAndClear());
}

TEST_F(ConfigureDisplaysTaskTest, ConfigureWithTwoDisplayFails) {
  ConfigureDisplaysTask::ResponseCallback callback = base::BindOnce(
      &ConfigureDisplaysTaskTest::ConfigureCallback, base::Unretained(this));

  delegate_.set_max_configurable_pixels(1);

  std::vector<DisplayConfigureRequest> requests;
  for (const auto& display : displays_) {
    requests.emplace_back(display.get(), display->native_mode(), gfx::Point());
  }

  ConfigureDisplaysTask task(&delegate_, requests, std::move(callback));
  task.Run();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(ConfigureDisplaysTask::ERROR, status_);
  EXPECT_EQ(JoinActions(GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[1]->display_id(), gfx::Point(),
                                       &big_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[1]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        nullptr),
            log_.GetActionsAndClear());
}

TEST_F(ConfigureDisplaysTaskTest, ReconfigureLastDisplayPartialSuccess) {
  ConfigureDisplaysTask::ResponseCallback callback = base::BindOnce(
      &ConfigureDisplaysTaskTest::ConfigureCallback, base::Unretained(this));

  delegate_.set_max_configurable_pixels(small_mode_.size().GetArea());

  std::vector<DisplayConfigureRequest> requests;
  for (const auto& display : displays_) {
    requests.emplace_back(display.get(), display->native_mode(), gfx::Point());
  }

  ConfigureDisplaysTask task(&delegate_, requests, std::move(callback));
  task.Run();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(ConfigureDisplaysTask::PARTIAL_SUCCESS, status_);
  EXPECT_EQ(JoinActions(GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[1]->display_id(), gfx::Point(),
                                       &big_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[1]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        nullptr),
            log_.GetActionsAndClear());
}

TEST_F(ConfigureDisplaysTaskTest, ReconfigureMiddleDisplayPartialSuccess) {
  ConfigureDisplaysTask::ResponseCallback callback = base::BindOnce(
      &ConfigureDisplaysTaskTest::ConfigureCallback, base::Unretained(this));

  displays_.push_back(FakeDisplaySnapshot::Builder()
                          .SetId(789)
                          .SetNativeMode(small_mode_.Clone())
                          .SetCurrentMode(small_mode_.Clone())
                          .Build());

  delegate_.set_max_configurable_pixels(small_mode_.size().GetArea());

  std::vector<DisplayConfigureRequest> requests;
  for (const auto& display : displays_) {
    requests.emplace_back(display.get(), display->native_mode(), gfx::Point());
  }

  ConfigureDisplaysTask task(&delegate_, requests, std::move(callback));
  task.Run();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(ConfigureDisplaysTask::PARTIAL_SUCCESS, status_);
  EXPECT_EQ(JoinActions(GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[1]->display_id(), gfx::Point(),
                                       &big_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[2]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[1]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[2]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        nullptr),
            log_.GetActionsAndClear());
}

TEST_F(ConfigureDisplaysTaskTest, AsyncConfigureWithTwoDisplaysPartialSuccess) {
  ConfigureDisplaysTask::ResponseCallback callback = base::BindOnce(
      &ConfigureDisplaysTaskTest::ConfigureCallback, base::Unretained(this));

  delegate_.set_run_async(true);
  delegate_.set_max_configurable_pixels(small_mode_.size().GetArea());

  std::vector<DisplayConfigureRequest> requests;
  for (const auto& display : displays_) {
    requests.emplace_back(display.get(), display->native_mode(), gfx::Point());
  }

  ConfigureDisplaysTask task(&delegate_, requests, std::move(callback));
  task.Run();

  EXPECT_FALSE(callback_called_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(ConfigureDisplaysTask::PARTIAL_SUCCESS, status_);
  EXPECT_EQ(JoinActions(GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[1]->display_id(), gfx::Point(),
                                       &big_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[1]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        nullptr),
            log_.GetActionsAndClear());
}

}  // namespace test
}  // namespace display
