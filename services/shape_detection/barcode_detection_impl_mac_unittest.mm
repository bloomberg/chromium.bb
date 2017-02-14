// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_impl_mac.h"

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_switches.h"

namespace shape_detection {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class BarcodeDetectionImplMacTest : public ::testing::Test {
 public:
  ~BarcodeDetectionImplMacTest() override = default;

  void DetectCallback(std::vector<mojom::BarcodeDetectionResultPtr> results) {
    Detection(results.size(), results.empty() ? "" : results[0]->raw_value);
  }
  MOCK_METHOD2(Detection, void(size_t, const std::string&));

  BarcodeDetectionImplMac impl_;
  const base::MessageLoop message_loop_;
};

TEST_F(BarcodeDetectionImplMacTest, CreateAndDestroy) {}

// This test generates a single QR code and scans it back.
TEST_F(BarcodeDetectionImplMacTest, ScanOneBarcode) {
  // Barcode detection needs at least MAC OS X 10.10, and GPU infrastructure.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseGpuInTests) ||
      !base::mac::IsAtLeastOS10_10()) {
    return;
  }

  const std::string kInfoString = "https://www.chromium.org";
  // Generate a QR code image as a CIImage by using |qr_code_generator|.
  NSData* const qr_code_data =
      [[NSString stringWithUTF8String:kInfoString.c_str()]
          dataUsingEncoding:NSISOLatin1StringEncoding];
  // TODO(mcasas): Consider using other generator types (e.g.
  // CI{AztecCode,Code128Barcode,PDF417Barcode}Generator) when the minimal OS X
  // is upgraded to 10.10+ (https://crbug.com/624049).
  CIFilter* qr_code_generator =
      [CIFilter filterWithName:@"CIQRCodeGenerator"];
  [qr_code_generator setValue:qr_code_data forKey:@"inputMessage"];

  // [CIImage outputImage] is available in macOS 10.10+.  Could be added to
  // sdk_forward_declarations.h but seems hardly worth it.
  EXPECT_TRUE([qr_code_generator respondsToSelector:@selector(outputImage)]);
  CIImage* qr_code_image =
      [qr_code_generator performSelector:@selector(outputImage)];

  const gfx::Size size([qr_code_image extent].size.width,
                       [qr_code_image extent].size.height);
  const int num_bytes = size.GetArea() * 4 /* bytes per pixel */;

  base::scoped_nsobject<CIContext> context([[CIContext alloc] init]);

  base::ScopedCFTypeRef<CGImageRef> cg_image(
      [context createCGImage:qr_code_image fromRect:[qr_code_image extent]]);
  EXPECT_EQ(static_cast<size_t>(size.width()), CGImageGetWidth(cg_image));
  EXPECT_EQ(static_cast<size_t>(size.height()), CGImageGetHeight(cg_image));

  base::ScopedCFTypeRef<CFDataRef> raw_cg_image_data(
      CGDataProviderCopyData(CGImageGetDataProvider(cg_image)));
  EXPECT_TRUE(CFDataGetBytePtr(raw_cg_image_data));
  EXPECT_EQ(num_bytes, CFDataGetLength(raw_cg_image_data));

  // Generate a new ScopedSharedBufferHandle of the aproppriate size, map it and
  // copy the generated qr code image pixels into it.
  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(num_bytes);
  ASSERT_TRUE(handle->is_valid());

  mojo::ScopedSharedBufferMapping mapping = handle->Map(num_bytes);
  ASSERT_TRUE(mapping);

  memcpy(mapping.get(), CFDataGetBytePtr(raw_cg_image_data), num_bytes);

  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  // Send the image Detect() and expect the response in callback.
  EXPECT_CALL(*this, Detection(1, kInfoString))
      .WillOnce(RunClosure(quit_closure));
  impl_.Detect(std::move(handle), size.width(), size.height(),
               base::Bind(&BarcodeDetectionImplMacTest::DetectCallback,
                          base::Unretained(this)));

  run_loop.Run();
}

}  // shape_detection namespace
