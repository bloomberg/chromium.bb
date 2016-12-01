// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/apply_content_protection_task.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/fake_display_snapshot.h"
#include "ui/display/manager/chromeos/display_layout_manager.h"
#include "ui/display/manager/chromeos/test/action_logger_util.h"
#include "ui/display/manager/chromeos/test/test_display_layout_manager.h"
#include "ui/display/manager/chromeos/test/test_native_display_delegate.h"

namespace ui {
namespace test {

namespace {

std::unique_ptr<DisplaySnapshot> CreateDisplaySnapshot(
    int64_t id,
    DisplayConnectionType type) {
  return display::FakeDisplaySnapshot::Builder()
      .SetId(id)
      .SetNativeMode(gfx::Size(1024, 768))
      .SetType(type)
      .Build();
}

}  // namespace

class ApplyContentProtectionTaskTest : public testing::Test {
 public:
  enum Response {
    ERROR,
    SUCCESS,
    NOT_CALLED,
  };

  ApplyContentProtectionTaskTest()
      : response_(NOT_CALLED), display_delegate_(&log_) {}
  ~ApplyContentProtectionTaskTest() override {}

  void ResponseCallback(bool success) { response_ = success ? SUCCESS : ERROR; }

 protected:
  Response response_;
  ActionLogger log_;
  TestNativeDisplayDelegate display_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ApplyContentProtectionTaskTest);
};

TEST_F(ApplyContentProtectionTaskTest, ApplyWithNoHDCPCapableDisplay) {
  ScopedVector<DisplaySnapshot> displays;
  displays.push_back(
      CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_INTERNAL));
  TestDisplayLayoutManager layout_manager(std::move(displays),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);

  DisplayConfigurator::ContentProtections request;
  request[1] = CONTENT_PROTECTION_METHOD_HDCP;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::Bind(&ApplyContentProtectionTaskTest::ResponseCallback,
                 base::Unretained(this)));
  task.Run();

  EXPECT_EQ(SUCCESS, response_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, ApplyWithHDMIDisplay) {
  ScopedVector<DisplaySnapshot> displays;
  displays.push_back(CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_HDMI));
  TestDisplayLayoutManager layout_manager(std::move(displays),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);

  DisplayConfigurator::ContentProtections request;
  request[1] = CONTENT_PROTECTION_METHOD_HDCP;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::Bind(&ApplyContentProtectionTaskTest::ResponseCallback,
                 base::Unretained(this)));
  task.Run();

  EXPECT_EQ(SUCCESS, response_);
  EXPECT_EQ(
      JoinActions(GetSetHDCPStateAction(*layout_manager.GetDisplayStates()[0],
                                        HDCP_STATE_DESIRED)
                      .c_str(),
                  NULL),
      log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, ApplyWithUnknownDisplay) {
  ScopedVector<DisplaySnapshot> displays;
  displays.push_back(CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_UNKNOWN));
  TestDisplayLayoutManager layout_manager(std::move(displays),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);

  DisplayConfigurator::ContentProtections request;
  request[1] = CONTENT_PROTECTION_METHOD_HDCP;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::Bind(&ApplyContentProtectionTaskTest::ResponseCallback,
                 base::Unretained(this)));
  task.Run();

  EXPECT_EQ(ERROR, response_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, FailGettingHDCPState) {
  ScopedVector<DisplaySnapshot> displays;
  displays.push_back(CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_HDMI));
  TestDisplayLayoutManager layout_manager(std::move(displays),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);
  display_delegate_.set_get_hdcp_state_expectation(false);

  DisplayConfigurator::ContentProtections request;
  request[1] = CONTENT_PROTECTION_METHOD_HDCP;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::Bind(&ApplyContentProtectionTaskTest::ResponseCallback,
                 base::Unretained(this)));
  task.Run();

  EXPECT_EQ(ERROR, response_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, FailSettingHDCPState) {
  ScopedVector<DisplaySnapshot> displays;
  displays.push_back(CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_HDMI));
  TestDisplayLayoutManager layout_manager(std::move(displays),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);
  display_delegate_.set_set_hdcp_state_expectation(false);

  DisplayConfigurator::ContentProtections request;
  request[1] = CONTENT_PROTECTION_METHOD_HDCP;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::Bind(&ApplyContentProtectionTaskTest::ResponseCallback,
                 base::Unretained(this)));
  task.Run();

  EXPECT_EQ(ERROR, response_);
  EXPECT_EQ(
      JoinActions(GetSetHDCPStateAction(*layout_manager.GetDisplayStates()[0],
                                        HDCP_STATE_DESIRED)
                      .c_str(),
                  NULL),
      log_.GetActionsAndClear());
}

TEST_F(ApplyContentProtectionTaskTest, ApplyNoopProtection) {
  ScopedVector<DisplaySnapshot> displays;
  displays.push_back(CreateDisplaySnapshot(1, DISPLAY_CONNECTION_TYPE_HDMI));
  TestDisplayLayoutManager layout_manager(std::move(displays),
                                          MULTIPLE_DISPLAY_STATE_SINGLE);
  display_delegate_.set_hdcp_state(HDCP_STATE_UNDESIRED);

  DisplayConfigurator::ContentProtections request;
  request[1] = CONTENT_PROTECTION_METHOD_NONE;
  ApplyContentProtectionTask task(
      &layout_manager, &display_delegate_, request,
      base::Bind(&ApplyContentProtectionTaskTest::ResponseCallback,
                 base::Unretained(this)));
  task.Run();

  EXPECT_EQ(SUCCESS, response_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
}

}  // namespace test
}  // namespace ui
