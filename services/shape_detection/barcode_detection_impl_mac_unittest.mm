// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_impl_mac.h"

#include <dlfcn.h>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "services/shape_detection/barcode_detection_impl_mac_vision.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
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

std::unique_ptr<mojom::BarcodeDetection> CreateBarcodeDetectorImplMacCoreImage(
    mojom::BarcodeDetectorOptionsPtr options) {
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

void* LoadVisionLibrary() {
  if (@available(macOS 10.13, *)) {
    return dlopen("/System/Library/Frameworks/Vision.framework/Vision",
                  RTLD_LAZY);
  }
  return nullptr;
}

using LibraryLoadCB = base::RepeatingCallback<void*(void)>;

using BarcodeDetectorFactory =
    base::RepeatingCallback<std::unique_ptr<mojom::BarcodeDetection>(
        mojom::BarcodeDetectorOptionsPtr)>;

const std::string kInfoString = "https://www.chromium.org";

struct TestParams {
  size_t num_barcodes;
  LibraryLoadCB library_load_callback;
  BarcodeDetectorFactory factory;
  NSString* test_code_generator;
} kTestParams[] = {
    // CoreImage only supports QR Codes.
    {1, base::BindRepeating([]() { return static_cast<void*>(nullptr); }),
     base::BindRepeating(&CreateBarcodeDetectorImplMacCoreImage),
     @"CIQRCodeGenerator"},
    // Vision only supports a number of 1D/2D codes. Not all of them are
    // available for generation, though, only a few.
    {1, base::BindRepeating(&LoadVisionLibrary),
     base::BindRepeating(&CreateBarcodeDetectorImplMacVision),
     @"CIPDF417BarcodeGenerator"},
    {1, base::BindRepeating(&LoadVisionLibrary),
     base::BindRepeating(&CreateBarcodeDetectorImplMacVision),
     @"CIQRCodeGenerator"},
    {6 /* 1D barcode makes the detector find the same code several times. */,
     base::BindRepeating(&LoadVisionLibrary),
     base::BindRepeating(&CreateBarcodeDetectorImplMacVision),
     @"CICode128BarcodeGenerator"}};
}

class BarcodeDetectionImplMacTest : public TestWithParam<struct TestParams> {
 public:
  ~BarcodeDetectionImplMacTest() override = default;

  void SetUp() override {
    vision_framework_ = GetParam().library_load_callback.Run();
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

  CIFilter* qr_code_generator =
      [CIFilter filterWithName:GetParam().test_code_generator];
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
                base::BindOnce(&BarcodeDetectionImplMacTest::DetectCallback,
                               base::Unretained(this), GetParam().num_barcodes,
                               kInfoString));

  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(, BarcodeDetectionImplMacTest, ValuesIn(kTestParams));

}  // shape_detection namespace
