// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_provider_mac.h"

#include <dlfcn.h>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "services/shape_detection/barcode_detection_provider_mac.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::TestWithParam;
using ::testing::ValuesIn;

namespace shape_detection {

namespace {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

static const std::vector<mojom::BarcodeFormat>& CISupportedFormats = {
    mojom::BarcodeFormat::QR_CODE};
static const std::vector<mojom::BarcodeFormat>& VisionSupportedFormats = {
    mojom::BarcodeFormat::AZTEC,       mojom::BarcodeFormat::CODE_128,
    mojom::BarcodeFormat::CODE_39,     mojom::BarcodeFormat::CODE_93,
    mojom::BarcodeFormat::DATA_MATRIX, mojom::BarcodeFormat::EAN_13,
    mojom::BarcodeFormat::EAN_8,       mojom::BarcodeFormat::ITF,
    mojom::BarcodeFormat::PDF417,      mojom::BarcodeFormat::QR_CODE,
    mojom::BarcodeFormat::UPC_E};

std::unique_ptr<mojom::BarcodeDetectionProvider> CreateBarcodeProviderMac() {
  return std::make_unique<BarcodeDetectionProviderMac>();
}

struct TestParams {
  const std::vector<mojom::BarcodeFormat> formats;
  bool test_vision_api;
} kTestParams[] = {
    {CISupportedFormats, false},
    {VisionSupportedFormats, true},
};
}

class BarcodeDetectionProviderMacTest
    : public TestWithParam<struct TestParams> {
 public:
  ~BarcodeDetectionProviderMacTest() override = default;

  void SetUp() override {
    bool is_vision_available = false;
    if (@available(macOS 10.13, *)) {
      is_vision_available = true;

      // Only load Vision if we're testing it.
      if (GetParam().test_vision_api) {
        vision_framework_ = dlopen(
            "/System/Library/Frameworks/Vision.framework/Vision", RTLD_LAZY);
      }
    }
    valid_combination_ = is_vision_available == GetParam().test_vision_api;
    provider_ = base::BindOnce(&CreateBarcodeProviderMac).Run();
  }

  void TearDown() override {
    if (vision_framework_)
      dlclose(vision_framework_);
  }

  void EnumerateSupportedFormatsCallback(
      const std::vector<mojom::BarcodeFormat>& expected,
      const std::vector<mojom::BarcodeFormat>& results) {
    EXPECT_THAT(results,
                testing::ElementsAreArray(expected.begin(), expected.end()));

    OnEnumerateSupportedFormats();
  }
  MOCK_METHOD0(OnEnumerateSupportedFormats, void(void));

  std::unique_ptr<mojom::BarcodeDetectionProvider> provider_;
  const base::MessageLoop message_loop_;
  void* vision_framework_ = nullptr;
  bool valid_combination_ = false;
};

TEST_P(BarcodeDetectionProviderMacTest, EnumerateSupportedBarcodes) {
  if (!valid_combination_) {
    LOG(WARNING) << "Barcode Detection for this (library, OS version) pair is "
                    "not supported, skipping test.";
    return;
  }

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, OnEnumerateSupportedFormats())
      .WillOnce(RunClosure(quit_closure));
  provider_->EnumerateSupportedFormats(base::BindOnce(
      &BarcodeDetectionProviderMacTest::EnumerateSupportedFormatsCallback,
      base::Unretained(this), GetParam().formats));
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(,
                         BarcodeDetectionProviderMacTest,
                         ValuesIn(kTestParams));

}  // shape_detection namespace
