// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <sstream>

#include "base/macros.h"
#include "services/device/hid/test_report_descriptors.h"
#include "services/device/public/cpp/hid/hid_report_descriptor.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class HidReportDescriptorTest : public testing::Test {
 protected:
  using HidUsageAndPage = mojom::HidUsageAndPage;
  using HidCollectionInfo = mojom::HidCollectionInfo;
  using HidCollectionInfoPtr = mojom::HidCollectionInfoPtr;

  void SetUp() override { descriptor_ = nullptr; }

  void TearDown() override {
    if (descriptor_) {
      delete descriptor_;
    }
  }

 public:
  void ValidateDetails(
      const std::vector<HidCollectionInfoPtr>& expected_collection_infos,
      const bool expected_has_report_id,
      const size_t expected_max_input_report_size,
      const size_t expected_max_output_report_size,
      const size_t expected_max_feature_report_size,
      const uint8_t* bytes,
      size_t size) {
    descriptor_ =
        new HidReportDescriptor(std::vector<uint8_t>(bytes, bytes + size));

    std::vector<HidCollectionInfoPtr> actual_collection_infos;
    bool actual_has_report_id;
    size_t actual_max_input_report_size;
    size_t actual_max_output_report_size;
    size_t actual_max_feature_report_size;
    descriptor_->GetDetails(&actual_collection_infos, &actual_has_report_id,
                            &actual_max_input_report_size,
                            &actual_max_output_report_size,
                            &actual_max_feature_report_size);

    ASSERT_EQ(expected_collection_infos.size(), actual_collection_infos.size());

    auto actual_info_iter = actual_collection_infos.begin();
    auto expected_info_iter = expected_collection_infos.begin();

    while (expected_info_iter != expected_collection_infos.end() &&
           actual_info_iter != actual_collection_infos.end()) {
      const HidCollectionInfoPtr& expected_info = *expected_info_iter;
      const HidCollectionInfoPtr& actual_info = *actual_info_iter;

      ASSERT_EQ(expected_info->usage->usage_page,
                actual_info->usage->usage_page);
      ASSERT_EQ(expected_info->usage->usage, actual_info->usage->usage);
      ASSERT_THAT(actual_info->report_ids,
                  testing::ContainerEq(expected_info->report_ids));

      ++expected_info_iter;
      ++actual_info_iter;
    }

    ASSERT_EQ(expected_has_report_id, actual_has_report_id);
    ASSERT_EQ(expected_max_input_report_size, actual_max_input_report_size);
    ASSERT_EQ(expected_max_output_report_size, actual_max_output_report_size);
    ASSERT_EQ(expected_max_feature_report_size, actual_max_feature_report_size);
  }

 private:
  HidReportDescriptor* descriptor_;
};

TEST_F(HidReportDescriptorTest, ValidateDetails_Digitizer) {
  const uint16_t usage_page = mojom::kPageDigitizer;
  const uint16_t usage = 0x01;  // Digitizer

  auto digitizer = HidCollectionInfo::New();
  digitizer->usage = HidUsageAndPage::New(usage, usage_page);
  digitizer->report_ids = {0x01, 0x02, 0x03};

  std::vector<HidCollectionInfoPtr> expected_infos;
  expected_infos.push_back(std::move(digitizer));
  ValidateDetails(expected_infos, true, 6, 0, 0, kDigitizer, kDigitizerSize);
}

TEST_F(HidReportDescriptorTest, ValidateDetails_Keyboard) {
  const uint16_t usage_page = mojom::kPageGenericDesktop;
  const uint16_t usage = mojom::kGenericDesktopKeyboard;

  auto keyboard = HidCollectionInfo::New();
  keyboard->usage = HidUsageAndPage::New(usage, usage_page);

  std::vector<HidCollectionInfoPtr> expected_infos;
  expected_infos.push_back(std::move(keyboard));
  ValidateDetails(expected_infos, false, 8, 1, 0, kKeyboard, kKeyboardSize);
}

TEST_F(HidReportDescriptorTest, ValidateDetails_Monitor) {
  const uint16_t usage_page = mojom::kPageMonitor0;  // USB monitor
  const uint16_t usage = 0x01;                       // Monitor control

  auto monitor = HidCollectionInfo::New();
  monitor->usage = HidUsageAndPage::New(usage, usage_page);
  monitor->report_ids = {1, 2, 3, 4, 5};

  std::vector<HidCollectionInfoPtr> expected_infos;
  expected_infos.push_back(std::move(monitor));
  ValidateDetails(expected_infos, true, 0, 0, 243, kMonitor, kMonitorSize);
}

TEST_F(HidReportDescriptorTest, ValidateDetails_Mouse) {
  const uint16_t usage_page = mojom::kPageGenericDesktop;
  const uint16_t usage = mojom::kGenericDesktopMouse;

  auto mouse = HidCollectionInfo::New();
  mouse->usage = HidUsageAndPage::New(usage, usage_page);

  std::vector<HidCollectionInfoPtr> expected_infos;
  expected_infos.push_back(std::move(mouse));
  ValidateDetails(expected_infos, false, 3, 0, 0, kMouse, kMouseSize);
}

TEST_F(HidReportDescriptorTest, ValidateDetails_LogitechUnifyingReceiver) {
  const uint16_t usage_page = mojom::kPageVendor;
  const uint16_t usage_hidpp_short = 0x01;  // Vendor-defined
  const uint16_t usage_hidpp_long = 0x02;   // Vendor-defined
  const uint16_t usage_hidpp_dj = 0x04;     // Vendor-defined

  auto hidpp_short = HidCollectionInfo::New();
  hidpp_short->usage = HidUsageAndPage::New(usage_hidpp_short, usage_page);
  hidpp_short->report_ids = {0x10};
  auto hidpp_long = HidCollectionInfo::New();
  hidpp_long->usage = HidUsageAndPage::New(usage_hidpp_long, usage_page);
  hidpp_long->report_ids = {0x11};
  auto hidpp_dj = HidCollectionInfo::New();
  hidpp_dj->usage = HidUsageAndPage::New(usage_hidpp_dj, usage_page);
  hidpp_dj->report_ids = {0x20, 0x21};

  std::vector<HidCollectionInfoPtr> expected_infos;
  expected_infos.push_back(std::move(hidpp_short));
  expected_infos.push_back(std::move(hidpp_long));
  expected_infos.push_back(std::move(hidpp_dj));
  ValidateDetails(expected_infos, true, 31, 31, 0, kLogitechUnifyingReceiver,
                  kLogitechUnifyingReceiverSize);
}

TEST_F(HidReportDescriptorTest, ValidateDetails_SonyDualshock3) {
  const uint16_t usage_page = mojom::kPageGenericDesktop;
  const uint16_t usage = mojom::kGenericDesktopJoystick;

  auto top_info = HidCollectionInfo::New();
  top_info->usage = HidUsageAndPage::New(usage, usage_page);
  top_info->report_ids = {0x01, 0x02, 0xee, 0xef};

  std::vector<HidCollectionInfoPtr> expected_infos;
  expected_infos.push_back(std::move(top_info));
  ValidateDetails(expected_infos, true, 48, 48, 48, kSonyDualshock3,
                  kSonyDualshock3Size);
}

TEST_F(HidReportDescriptorTest, ValidateDetails_SonyDualshock4) {
  const uint16_t usage_page = mojom::kPageGenericDesktop;
  const uint16_t usage = mojom::kGenericDesktopGamePad;

  auto top_info = HidCollectionInfo::New();
  top_info->usage = HidUsageAndPage::New(usage, usage_page);
  top_info->report_ids = {0x01, 0x05, 0x04, 0x02, 0x08, 0x10, 0x11, 0x12, 0x13,
                          0x14, 0x15, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
                          0x87, 0x88, 0x89, 0x90, 0x91, 0x92, 0x93, 0xa0, 0xa1,
                          0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xf0, 0xf1, 0xf2, 0xa7,
                          0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0};

  std::vector<HidCollectionInfoPtr> expected_infos;
  expected_infos.push_back(std::move(top_info));
  ValidateDetails(expected_infos, true, 63, 31, 63, kSonyDualshock4,
                  kSonyDualshock4Size);
}

TEST_F(HidReportDescriptorTest, ValidateDetails_XboxWirelessController) {
  const uint16_t usage_page = mojom::kPageGenericDesktop;
  const uint16_t usage = mojom::kGenericDesktopGamePad;

  auto top_info = HidCollectionInfo::New();
  top_info->usage = HidUsageAndPage::New(usage, usage_page);
  top_info->report_ids = {0x01, 0x02, 0x03, 0x04};

  std::vector<HidCollectionInfoPtr> expected_info;
  expected_info.push_back(std::move(top_info));
  ValidateDetails(expected_info, true, 15, 8, 0,
                  kMicrosoftXboxWirelessController,
                  kMicrosoftXboxWirelessControllerSize);
}

TEST_F(HidReportDescriptorTest, ValidateDetails_NintendoSwitchProController) {
  const uint16_t usage_page = mojom::kPageGenericDesktop;
  const uint16_t usage = mojom::kGenericDesktopJoystick;

  auto top_info = HidCollectionInfo::New();
  top_info->usage = HidUsageAndPage::New(usage, usage_page);
  top_info->report_ids = {0x30, 0x21, 0x81, 0x01, 0x10, 0x80, 0x82};

  std::vector<HidCollectionInfoPtr> expected_info;
  expected_info.push_back(std::move(top_info));
  ValidateDetails(expected_info, true, 63, 63, 0, kNintendoSwitchProController,
                  kNintendoSwitchProControllerSize);
}

TEST_F(HidReportDescriptorTest, ValidateDetails_XboxAdaptiveController) {
  const uint16_t usage_page = mojom::kPageGenericDesktop;
  const uint16_t usage_gamepad = mojom::kGenericDesktopGamePad;
  const uint16_t usage_keyboard = mojom::kGenericDesktopKeyboard;
  const uint8_t report_ids_gamepad[] = {0x01, 0x02, 0x03, 0x04, 0x06,
                                        0x07, 0x08, 0x09, 0x0a, 0x0b};
  const size_t report_ids_gamepad_size = base::size(report_ids_gamepad);
  const uint8_t report_ids_keyboard[] = {0x05};
  const size_t report_ids_keyboard_size = base::size(report_ids_keyboard);

  auto gamepad_info = HidCollectionInfo::New();
  gamepad_info->usage = HidUsageAndPage::New(usage_gamepad, usage_page);
  gamepad_info->report_ids.insert(gamepad_info->report_ids.begin(),
                                  report_ids_gamepad,
                                  report_ids_gamepad + report_ids_gamepad_size);

  auto keyboard_info = HidCollectionInfo::New();
  keyboard_info->usage = HidUsageAndPage::New(usage_keyboard, usage_page);
  keyboard_info->report_ids.insert(
      keyboard_info->report_ids.begin(), report_ids_keyboard,
      report_ids_keyboard + report_ids_keyboard_size);

  std::vector<HidCollectionInfoPtr> expected_info;
  expected_info.push_back(std::move(gamepad_info));
  expected_info.push_back(std::move(keyboard_info));
  ValidateDetails(expected_info, true, 54, 8, 64,
                  kMicrosoftXboxAdaptiveController,
                  kMicrosoftXboxAdaptiveControllerSize);
}

}  // namespace device
