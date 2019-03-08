// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_impl_mac.h"

#include <dlfcn.h>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "services/shape_detection/barcode_detection_impl_mac_vision.h"
#include "services/shape_detection/barcode_detection_provider_mac.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"
#include "ui/gl/gl_switches.h"

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

std::unique_ptr<mojom::BarcodeDetection> CreateBarcodeDetectorImplMacCI(
    mojom::BarcodeDetectorOptionsPtr options) {
  if (@available(macOS 10.13, *)) {
    return nullptr;
  }
  if (@available(macOS 10.10, *)) {
    return std::make_unique<BarcodeDetectionImplMac>();
  }
  return nullptr;
}

std::unique_ptr<mojom::BarcodeDetection> CreateBarcodeDetectorImplMacVision(
    mojom::BarcodeDetectorOptionsPtr options) {
  if (@available(macOS 10.13, *)) {
    return std::make_unique<BarcodeDetectionImplMacVision>(
        mojom::BarcodeDetectorOptions::New());
  }
  return nullptr;
}

using BarcodeDetectorFactory =
    base::Callback<std::unique_ptr<mojom::BarcodeDetection>(
        mojom::BarcodeDetectorOptionsPtr)>;

const std::string kInfoString = "https://www.chromium.org";

struct TestParams {
  size_t num_barcodes;
  const std::string barcode_value;
  const std::vector<mojom::BarcodeFormat> formats;
  BarcodeDetectorFactory factory;
} kTestParams[] = {
    {1, kInfoString, CISupportedFormats,
     base::BindRepeating(&CreateBarcodeDetectorImplMacCI)},
    {1, kInfoString, VisionSupportedFormats,
     base::BindRepeating(&CreateBarcodeDetectorImplMacVision)},
};
}

class BarcodeDetectionImplMacTest : public TestWithParam<struct TestParams> {
 public:
  ~BarcodeDetectionImplMacTest() override = default;

  void SetUp() override {
    if (@available(macOS 10.13, *)) {
      vision_framework_ = dlopen(
          "/System/Library/Frameworks/Vision.framework/Vision", RTLD_LAZY);
    }
    provider_ = base::BindOnce(&CreateBarcodeProviderMac).Run();
  }

  void TearDown() override {
    if (vision_framework_)
      dlclose(vision_framework_);
  }

  void DetectCallback(size_t num_barcodes,
                      const std::string& barcode_value,
                      std::vector<mojom::BarcodeDetectionResultPtr> results) {
    EXPECT_EQ(num_barcodes, results.size());
    for (const auto& barcode : results)
      EXPECT_EQ(barcode_value, barcode->raw_value);

    Detection();
  }
  MOCK_METHOD0(Detection, void(void));

  void EnumerateSupportedFormatsCallback(
      const std::vector<mojom::BarcodeFormat>& expected,
      const std::vector<mojom::BarcodeFormat>& results) {
    EXPECT_THAT(results,
                testing::ElementsAreArray(expected.begin(), expected.end()));

    OnEnumerateSupportedFormats();
  }
  MOCK_METHOD0(OnEnumerateSupportedFormats, void(void));

  std::unique_ptr<mojom::BarcodeDetection> impl_;
  std::unique_ptr<mojom::BarcodeDetectionProvider> provider_;
  const base::MessageLoop message_loop_;
  void* vision_framework_ = nullptr;
};

TEST_P(BarcodeDetectionImplMacTest, CreateAndDestroy) {
  impl_ = GetParam().factory.Run(mojom::BarcodeDetectorOptions::New());
  if (!impl_) {
    LOG(WARNING) << "Barcode Detection for this (library, OS version) pair is "
                    "not supported, skipping test.";
    return;
  }
}

TEST_P(BarcodeDetectionImplMacTest, EnumerateSupportedBarcodes) {
  impl_ = GetParam().factory.Run(mojom::BarcodeDetectorOptions::New());
  if (!impl_) {
    LOG(WARNING) << "Barcode Detection for this (library, OS version) pair is "
                    "not supported, skipping test.";
    return;
  }

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, OnEnumerateSupportedFormats())
      .WillOnce(RunClosure(quit_closure));
  provider_->EnumerateSupportedFormats(base::BindOnce(
      &BarcodeDetectionImplMacTest::EnumerateSupportedFormatsCallback,
      base::Unretained(this), GetParam().formats));
  run_loop.Run();
}

// This test generates a single QR code and scans it back.
TEST_P(BarcodeDetectionImplMacTest, ScanOneBarcode) {
  // Barcode detection needs at least MAC OS X 10.10, and GPU infrastructure.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseGpuInTests)) {
    return;
  }

  impl_ = GetParam().factory.Run(mojom::BarcodeDetectorOptions::New());
  if (!impl_) {
    LOG(WARNING) << "Barcode Detection is not supported before Mac OSX 10.10."
                 << "Skipping test.";
    return;
  }

  // Generate a QR code image as a CIImage by using |qr_code_generator|.
  NSData* const qr_code_data =
      [[NSString stringWithUTF8String:kInfoString.c_str()]
          dataUsingEncoding:NSISOLatin1StringEncoding];
  // TODO(mcasas): Consider using other generator types (e.g.
  // CI{AztecCode,Code128Barcode,PDF417Barcode}Generator) when the minimal OS
  // X is upgraded to 10.10+ (https://crbug.com/624049).
  CIFilter* qr_code_generator = [CIFilter filterWithName:@"CIQRCodeGenerator"];
  [qr_code_generator setValue:qr_code_data forKey:@"inputMessage"];

  // [CIImage outputImage] is available in macOS 10.10+.  Could be added to
  // sdk_forward_declarations.h but seems hardly worth it.
  EXPECT_TRUE([qr_code_generator respondsToSelector:@selector(outputImage)]);
  CIImage* qr_code_image =
      [qr_code_generator performSelector:@selector(outputImage)];

  const gfx::Size size([qr_code_image extent].size.width,
                       [qr_code_image extent].size.height);

  base::scoped_nsobject<CIContext> context([[CIContext alloc] init]);

  base::ScopedCFTypeRef<CGImageRef> cg_image(
      [context createCGImage:qr_code_image fromRect:[qr_code_image extent]]);
  EXPECT_EQ(static_cast<size_t>(size.width()), CGImageGetWidth(cg_image));
  EXPECT_EQ(static_cast<size_t>(size.height()), CGImageGetHeight(cg_image));

  SkBitmap bitmap;
  ASSERT_TRUE(SkCreateBitmapFromCGImage(&bitmap, cg_image));

  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  // Send the image Detect() and expect the response in callback.
  EXPECT_CALL(*this, Detection()).WillOnce(RunClosure(quit_closure));
  impl_->Detect(bitmap,
                base::Bind(&BarcodeDetectionImplMacTest::DetectCallback,
                           base::Unretained(this), GetParam().num_barcodes,
                           GetParam().barcode_value));

  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(, BarcodeDetectionImplMacTest, ValuesIn(kTestParams));

}  // shape_detection namespace
