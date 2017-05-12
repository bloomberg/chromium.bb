// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/text_detection_impl_mac.h"

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_switches.h"

namespace shape_detection {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class TextDetectionImplMacTest : public ::testing::Test {
 public:
  ~TextDetectionImplMacTest() override = default;

  void DetectCallback(std::vector<mojom::TextDetectionResultPtr> results) {
    // CIDetectorTypeText doesn't return the decoded text, juts bounding boxes.
    Detection(results.size());
  }
  MOCK_METHOD1(Detection, void(size_t));

  TextDetectionImplMac impl_;
  const base::MessageLoop message_loop_;
};

TEST_F(TextDetectionImplMacTest, CreateAndDestroy) {}

// This test generates an image with a single text line and scans it back.
TEST_F(TextDetectionImplMacTest, ScanOnce) {
  // Text detection needs at least MAC OS X 10.11, and GPU infrastructure.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseGpuInTests) ||
      !base::mac::IsAtLeastOS10_11()) {
    return;
  }

  base::ScopedCFTypeRef<CGColorSpaceRef> rgb_colorspace(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));

  const int width = 200;
  const int height = 50;
  base::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
      nullptr, width, height, 8 /* bitsPerComponent */,
      width * 4 /* rowBytes */, rgb_colorspace,
      kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));

  // Draw a white background.
  CGContextSetRGBFillColor(context, 1.0, 1.0, 1.0, 1.0);
  CGContextFillRect(context, CGRectMake(0.0, 0.0, width, height));

  // Create a line of Helvetica 16 text, and draw it in the |context|.
  base::scoped_nsobject<NSFont> helvetica(
      [NSFont fontWithName:@"Helvetica" size:16]);
  NSDictionary* attributes = [NSDictionary
      dictionaryWithObjectsAndKeys:helvetica, kCTFontAttributeName, nil];

  base::scoped_nsobject<NSAttributedString> info([[NSAttributedString alloc]
      initWithString:@"https://www.chromium.org"
          attributes:attributes]);

  base::ScopedCFTypeRef<CTLineRef> line(
      CTLineCreateWithAttributedString((CFAttributedStringRef)info.get()));

  CGContextSetTextPosition(context, 10.0, height / 2.0);
  CTLineDraw(line, context);

  // Extract a CGImage and its raw pixels from |context|.
  base::ScopedCFTypeRef<CGImageRef> cg_image(
      CGBitmapContextCreateImage(context));
  EXPECT_EQ(static_cast<size_t>(width), CGImageGetWidth(cg_image));
  EXPECT_EQ(static_cast<size_t>(height), CGImageGetHeight(cg_image));

  base::ScopedCFTypeRef<CFDataRef> raw_cg_image_data(
      CGDataProviderCopyData(CGImageGetDataProvider(cg_image)));
  EXPECT_TRUE(CFDataGetBytePtr(raw_cg_image_data));
  const int num_bytes = width * height * 4;
  EXPECT_EQ(num_bytes, CFDataGetLength(raw_cg_image_data));

  // Generate a new ScopedSharedBufferHandle of the aproppriate size, map it and
  // copy the generated text image pixels into it.
  auto handle = mojo::SharedBufferHandle::Create(num_bytes);
  ASSERT_TRUE(handle->is_valid());

  mojo::ScopedSharedBufferMapping mapping = handle->Map(num_bytes);
  ASSERT_TRUE(mapping);

  memcpy(mapping.get(), CFDataGetBytePtr(raw_cg_image_data), num_bytes);

  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  // Send the image to Detect() and expect the response in callback.
  EXPECT_CALL(*this, Detection(1)).WillOnce(RunClosure(quit_closure));
  impl_.Detect(std::move(handle), width, height,
               base::Bind(&TextDetectionImplMacTest::DetectCallback,
                          base::Unretained(this)));

  run_loop.Run();
}

}  // shape_detection namespace
