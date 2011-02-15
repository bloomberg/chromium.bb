// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_graphics_2d.h"

#include <string.h>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/cpp/common.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Graphics2D);

namespace {

// A NOP flush callback for use in various tests.
void FlushCallbackNOP(void* data, int32_t result) {
}

void FlushCallbackQuitMessageLoop(void* data, int32_t result) {
  reinterpret_cast<TestGraphics2D*>(data)->QuitMessageLoop();
}

}  // namespace

bool TestGraphics2D::Init() {
  graphics_2d_interface_ = reinterpret_cast<PPB_Graphics2D const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_GRAPHICS_2D_INTERFACE));
  image_data_interface_ = reinterpret_cast<PPB_ImageData const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_IMAGEDATA_INTERFACE));
  testing_interface_ = reinterpret_cast<PPB_Testing_Dev const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_TESTING_DEV_INTERFACE));
  if (!testing_interface_) {
    // Give a more helpful error message for the testing interface being gone
    // since that needs special enabling in Chrome.
    instance_->AppendError("This test needs the testing interface, which is "
        "not currently available. In Chrome, use --enable-pepper-testing when "
        "launching.");
  }
  return graphics_2d_interface_ && image_data_interface_ &&
         testing_interface_;
}

void TestGraphics2D::RunTest() {
  instance_->LogTest("InvalidResource", TestInvalidResource());
  instance_->LogTest("InvalidSize", TestInvalidSize());
  instance_->LogTest("Humongous", TestHumongous());
  instance_->LogTest("InitToZero", TestInitToZero());
  instance_->LogTest("Describe", TestDescribe());
  instance_->LogTest("Paint", TestPaint());
  //instance_->LogTest("Scroll", TestScroll());  // TODO(brettw) implement.
  instance_->LogTest("Replace", TestReplace());
  instance_->LogTest("Flush", TestFlush());
}

void TestGraphics2D::QuitMessageLoop() {
  testing_interface_->QuitMessageLoop(instance_->pp_instance());
}

bool TestGraphics2D::ReadImageData(const pp::Graphics2D& dc,
                                   pp::ImageData* image,
                                   const pp::Point& top_left) const {
  return pp::PPBoolToBool(testing_interface_->ReadImageData(
      dc.pp_resource(),
      image->pp_resource(),
      &top_left.pp_point()));
}

bool TestGraphics2D::IsDCUniformColor(const pp::Graphics2D& dc,
                                      uint32_t color) const {
  pp::ImageData readback(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                         dc.size(), false);
  if (readback.is_null())
    return false;
  if (!ReadImageData(dc, &readback, pp::Point(0, 0)))
    return false;
  return IsSquareInImage(readback, 0, pp::Rect(dc.size()), color);
}

bool TestGraphics2D::FlushAndWaitForDone(pp::Graphics2D* context) {
  pp::CompletionCallback cc(&FlushCallbackQuitMessageLoop, this);
  int32_t rv = context->Flush(cc);
  if (rv == PP_OK)
    return true;
  if (rv != PP_ERROR_WOULDBLOCK)
    return false;
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  return true;
}

void TestGraphics2D::FillRectInImage(pp::ImageData* image,
                                     const pp::Rect& rect,
                                     uint32_t color) const {
  for (int y = rect.y(); y < rect.bottom(); y++) {
    uint32_t* row = image->GetAddr32(pp::Point(rect.x(), y));
    for (int pixel = 0; pixel < rect.width(); pixel++)
      row[pixel] = color;
  }
}

void TestGraphics2D::FillImageWithGradient(pp::ImageData* image) const {
  for (int y = 0; y < image->size().height(); y++) {
    uint32_t red = ((y * 256) / image->size().height()) & 0xFF;
    for (int x = 0; x < image->size().width(); x++) {
      uint32_t green = ((x * 256) / image->size().width()) & 0xFF;
      uint32_t blue = ((red + green) / 2) & 0xFF;
      uint32_t* pixel = image->GetAddr32(pp::Point(x, y));
      *pixel = (blue << 24) | (green << 16) | (red << 8);
    }
  }
}

bool TestGraphics2D::CompareImages(const pp::ImageData& image1,
                                   const pp::ImageData& image2) {
  return CompareImageRect(
      image1, pp::Rect(0, 0, image1.size().width(), image1.size().height()),
      image2, pp::Rect(0, 0, image2.size().width(), image2.size().height()));
}

bool TestGraphics2D::CompareImageRect(const pp::ImageData& image1,
                                      const pp::Rect& rc1,
                                      const pp::ImageData& image2,
                                      const pp::Rect& rc2) const {
  if (rc1.width() != rc2.width() || rc1.height() != rc2.height())
    return false;

  for (int y = 0; y < rc1.height(); y++) {
    for (int x = 0; x < rc1.width(); x++) {
      if (*(image1.GetAddr32(pp::Point(rc1.x() + x, rc1.y() + y))) !=
          *(image2.GetAddr32(pp::Point(rc2.x() + x, rc2.y() + y))))
        return false;
    }
  }
  return true;
}

bool TestGraphics2D::IsSquareInImage(const pp::ImageData& image_data,
                                     uint32_t background_color,
                                     const pp::Rect& square,
                                     uint32_t square_color) const {
  for (int y = 0; y < image_data.size().height(); y++) {
    for (int x = 0; x < image_data.size().width(); x++) {
      uint32_t pixel = *image_data.GetAddr32(pp::Point(x, y));
      uint32_t desired_color;
      if (square.Contains(x, y))
        desired_color = square_color;
      else
        desired_color = background_color;
      if (pixel != desired_color)
        return false;
    }
  }
  return true;
}

bool TestGraphics2D::IsSquareInDC(const pp::Graphics2D& dc,
                                  uint32_t background_color,
                                  const pp::Rect& square,
                                  uint32_t square_color) const {
  pp::ImageData readback(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                         dc.size(), false);
  if (readback.is_null())
    return false;
  if (!ReadImageData(dc, &readback, pp::Point(0, 0)))
    return false;
  return IsSquareInImage(readback, background_color, square, square_color);
}

// Test all the functions with an invalid handle. Most of these just check for
// a crash since the browser don't return a value.
std::string TestGraphics2D::TestInvalidResource() {
  pp::Graphics2D null_context;
  pp::ImageData image(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                      pp::Size(16, 16), true);

  // Describe.
  PP_Size size;
  PP_Bool opaque;
  graphics_2d_interface_->Describe(image.pp_resource(), &size, &opaque);
  graphics_2d_interface_->Describe(null_context.pp_resource(),
                                   &size, &opaque);

  // PaintImageData.
  PP_Point zero_zero;
  zero_zero.x = 0;
  zero_zero.y = 0;
  graphics_2d_interface_->PaintImageData(image.pp_resource(),
                                         image.pp_resource(),
                                         &zero_zero, NULL);
  graphics_2d_interface_->PaintImageData(null_context.pp_resource(),
                                         image.pp_resource(),
                                         &zero_zero, NULL);

  // Scroll.
  PP_Point zero_ten;
  zero_ten.x = 0;
  zero_ten.y = 10;
  graphics_2d_interface_->Scroll(image.pp_resource(), NULL, &zero_ten);
  graphics_2d_interface_->Scroll(null_context.pp_resource(),
                                 NULL, &zero_ten);

  // ReplaceContents.
  graphics_2d_interface_->ReplaceContents(image.pp_resource(),
                                          image.pp_resource());
  graphics_2d_interface_->ReplaceContents(null_context.pp_resource(),
                                          image.pp_resource());

  // Flush.
  if (graphics_2d_interface_->Flush(
          image.pp_resource(),
          PP_MakeCompletionCallback(&FlushCallbackNOP, NULL)) == PP_OK)
    return "Flush succeeded with a different resource";
  if (graphics_2d_interface_->Flush(
          null_context.pp_resource(),
          PP_MakeCompletionCallback(&FlushCallbackNOP, NULL)) == PP_OK)
    return "Flush succeeded with a NULL resource";

  // ReadImageData.
  if (testing_interface_->ReadImageData(image.pp_resource(),
                                        image.pp_resource(),
                                        &zero_zero))
    return "ReadImageData succeeded with a different resource";
  if (testing_interface_->ReadImageData(null_context.pp_resource(),
                                        image.pp_resource(),
                                        &zero_zero))
    return "ReadImageData succeeded with a NULL resource";

  PASS();
}

std::string TestGraphics2D::TestInvalidSize() {
  pp::Graphics2D a(instance_, pp::Size(16, 0), false);
  if (!a.is_null())
    return "0 height accepted";

  pp::Graphics2D b(instance_, pp::Size(0, 16), false);
  if (!b.is_null())
    return "0 width accepted";

  // Need to use the C API since pp::Size prevents negative sizes.
  PP_Size size;
  size.width = 16;
  size.height = -16;
  ASSERT_FALSE(!!graphics_2d_interface_->Create(
      pp::Module::Get()->pp_module(), &size, PP_FALSE));

  size.width = -16;
  size.height = 16;
  ASSERT_FALSE(!!graphics_2d_interface_->Create(
      pp::Module::Get()->pp_module(), &size, PP_FALSE));

  PASS();
}

std::string TestGraphics2D::TestHumongous() {
  pp::Graphics2D a(instance_, pp::Size(100000, 100000), false);
  if (!a.is_null())
    return "Humongous device created";
  PASS();
}

std::string TestGraphics2D::TestInitToZero() {
  const int w = 15, h = 17;
  pp::Graphics2D dc(instance_, pp::Size(w, h), false);
  if (dc.is_null())
    return "Failure creating a boring device";

  // Make an image with nonzero data in it (so we can test that zeros were
  // actually read versus ReadImageData being a NOP).
  pp::ImageData image(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                      pp::Size(w, h), true);
  if (image.is_null())
    return "Failure to allocate an image";
  memset(image.data(), 0xFF, image.stride() * image.size().height() * 4);

  // Read out the initial data from the device & check.
  if (!ReadImageData(dc, &image, pp::Point(0, 0)))
    return "Couldn't read image data";
  if (!IsSquareInImage(image, 0, pp::Rect(0, 0, w, h), 0))
    return "Got a nonzero pixel";

  PASS();
}

std::string TestGraphics2D::TestDescribe() {
  const int w = 15, h = 17;
  pp::Graphics2D dc(instance_, pp::Size(w, h), false);
  if (dc.is_null())
    return "Failure creating a boring device";

  PP_Size size;
  size.width = -1;
  size.height = -1;
  PP_Bool is_always_opaque = PP_TRUE;
  if (!graphics_2d_interface_->Describe(dc.pp_resource(), &size,
                                        &is_always_opaque))
    return "Describe failed";
  if (size.width != w || size.height != h || is_always_opaque != PP_FALSE)
    return "Mismatch of data.";

  PASS();
}

std::string TestGraphics2D::TestPaint() {
  const int w = 15, h = 17;
  pp::Graphics2D dc(instance_, pp::Size(w, h), false);
  if (dc.is_null())
    return "Failure creating a boring device";

  // Make sure the device background is 0.
  if (!IsDCUniformColor(dc, 0))
    return "Bad initial color";

  // Fill the backing store with white.
  const uint32_t background_color = 0xFFFFFFFF;
  pp::ImageData background(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                           pp::Size(w, h), false);
  FillRectInImage(&background, pp::Rect(0, 0, w, h), background_color);
  dc.PaintImageData(background, pp::Point(0, 0));
  if (!FlushAndWaitForDone(&dc))
    return "Couldn't flush to fill backing store";

  // Make an image to paint with that's opaque white and enqueue a paint.
  const int fill_w = 2, fill_h = 3;
  pp::ImageData fill(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                     pp::Size(fill_w, fill_h), true);
  if (fill.is_null())
    return "Failure to allocate fill image";
  FillRectInImage(&fill, pp::Rect(fill.size()), background_color);
  const int paint_x = 4, paint_y = 5;
  dc.PaintImageData(fill, pp::Point(paint_x, paint_y));

  // Validate that nothing has been actually painted.
  if (!IsDCUniformColor(dc, background_color))
    return "Image updated before flush (or failure in readback).";

  // The paint hasn't been flushed so we can still change the bitmap. Fill with
  // 50% blue. This will also verify that the backing store is replaced
  // with the contents rather than blended.
  const uint32_t fill_color = 0x80000080;
  FillRectInImage(&fill, pp::Rect(fill.size()), fill_color);
  if (!FlushAndWaitForDone(&dc))
    return "Couldn't flush 50% blue paint";

  if (!IsSquareInDC(dc, background_color,
                    pp::Rect(paint_x, paint_y, fill_w, fill_h),
                    fill_color))
    return "Image not painted properly.";

  // Reset the DC to blank white & paint our image slightly off the buffer.
  // This should succeed. We also try painting the same thing where the
  // dirty rect falls outeside of the device, which should fail.
  dc.PaintImageData(background, pp::Point(0, 0));
  const int second_paint_x = -1, second_paint_y = -2;
  dc.PaintImageData(fill, pp::Point(second_paint_x, second_paint_y));
  dc.PaintImageData(fill, pp::Point(second_paint_x, second_paint_y),
                    pp::Rect(-second_paint_x, -second_paint_y, 1, 1));
  if (!FlushAndWaitForDone(&dc))
    return "Couldn't flush second paint";

  // Now we should have a little bit of the image peeking out the top left.
  if (!IsSquareInDC(dc, background_color, pp::Rect(0, 0, 1, 1),
                    fill_color))
    return "Partially offscreen paint failed.";

  // Now repaint that top left pixel by doing a subset of the source image.
  pp::ImageData subset(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                       pp::Size(w, h), false);
  uint32_t subset_color = 0x80808080;
  const int subset_x = 2, subset_y = 1;
  *subset.GetAddr32(pp::Point(subset_x, subset_y)) = subset_color;
  dc.PaintImageData(subset, pp::Point(-subset_x, -subset_y),
                    pp::Rect(subset_x, subset_y, 1, 1));
  if (!FlushAndWaitForDone(&dc))
    return "Couldn't flush repaint";
  if (!IsSquareInDC(dc, background_color, pp::Rect(0, 0, 1, 1),
                    subset_color))
    return "Subset paint failed.";

  PASS();
}

std::string TestGraphics2D::TestScroll() {
  const int w = 115, h = 117;
  pp::Graphics2D dc(instance_, pp::Size(w, h), false);
  if (dc.is_null())
    return "Failure creating a boring device.";

  // Make sure the device background is 0.
  if (!IsDCUniformColor(dc, 0))
    return "Bad initial color.";

  const int image_w = 15, image_h = 23;
  pp::ImageData test_image(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                           pp::Size(image_w, image_h), false);
  FillImageWithGradient(&test_image);

  int image_x = 51, image_y = 72;
  dc.PaintImageData(test_image, pp::Point(image_x, image_y));
  if (!FlushAndWaitForDone(&dc))
    return "Couldn't flush to fill backing store.";

  // TC1, Scroll image to a free space.
  int dx = -40, dy = -48;
  pp::Rect clip = pp::Rect(image_x, image_y, test_image.size().width(),
                           test_image.size().height());
  dc.Scroll(clip, pp::Point(dx, dy));

  if (!FlushAndWaitForDone(&dc))
    return "TC1, Couldn't flush to scroll.";

  image_x += dx;
  image_y += dy;

  pp::ImageData readback(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                         pp::Size(image_w, image_h), false);
  if (!ReadImageData(dc, &readback, pp::Point(image_x, image_y)))
    return "TC1, Couldn't read back image data.";

  if (!CompareImages(test_image, readback))
    return "TC1, Read back image is not the same as test image.";

  // TC2, Scroll image to an overlapping space.
  dx = 6;
  dy = 9;
  clip = pp::Rect(image_x, image_y, test_image.size().width(),
                  test_image.size().height());
  dc.Scroll(clip, pp::Point(dx, dy));

  if (!FlushAndWaitForDone(&dc))
    return "TC2, Couldn't flush to scroll.";

  image_x += dx;
  image_y += dy;

  if (!ReadImageData(dc, &readback, pp::Point(image_x, image_y)))
    return "TC2, Couldn't read back image data.";

  if (!CompareImages(test_image, readback))
    return "TC2, Read back image is not the same as test image.";

  PASS();
}

std::string TestGraphics2D::TestReplace() {
  const int w = 15, h = 17;
  pp::Graphics2D dc(instance_, pp::Size(w, h), false);
  if (dc.is_null())
    return "Failure creating a boring device";

  // Replacing with a different size image should fail.
  pp::ImageData weird_size(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                           pp::Size(w - 1, h), true);
  if (weird_size.is_null())
    return "Failure allocating the weird sized image";
  dc.ReplaceContents(&weird_size);

  // Fill the background with blue but don't flush yet.
  const int32_t background_color = 0xFF0000FF;
  pp::ImageData background(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                           pp::Size(w, h), true);
  if (background.is_null())
    return "Failure to allocate background image";
  FillRectInImage(&background, pp::Rect(0, 0, w, h), background_color);
  dc.PaintImageData(background, pp::Point(0, 0));

  // Replace with a green background but don't flush yet.
  const int32_t swapped_color = 0xFF0000FF;
  pp::ImageData swapped(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                        pp::Size(w, h), true);
  if (swapped.is_null())
    return "Failure to allocate swapped image";
  FillRectInImage(&swapped, pp::Rect(0, 0, w, h), swapped_color);
  dc.ReplaceContents(&swapped);

  // The background should be unchanged since we didn't flush yet.
  if (!IsDCUniformColor(dc, 0))
    return "Image updated before flush (or failure in readback).";

  // Test the C++ wrapper. The size of the swapped image should be reset.
  if (swapped.pp_resource() || swapped.size().width() ||
      swapped.size().height() || swapped.data())
    return "Size of the swapped image should be reset.";

  // Painting with the swapped image should fail.
  dc.PaintImageData(swapped, pp::Point(0, 0));

  // Flush and make sure the result is correct.
  if (!FlushAndWaitForDone(&dc))
    return "Couldn't flush";

  // The background should be green from the swapped image.
  if (!IsDCUniformColor(dc, swapped_color))
    return "Flushed color incorrect (or failure in readback).";

  PASS();
}

std::string TestGraphics2D::TestFlush() {
  // Tests that synchronous flushes (NULL callback) fail on the main thread
  // (which is the current one).
  const int w = 15, h = 17;
  pp::Graphics2D dc(instance_, pp::Size(w, h), false);
  if (dc.is_null())
    return "Failure creating a boring device";

  // Fill the background with blue but don't flush yet.
  pp::ImageData background(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                           pp::Size(w, h), true);
  if (background.is_null())
    return "Failure to allocate background image";
  dc.PaintImageData(background, pp::Point(0, 0));

  int32_t rv = dc.Flush(pp::CompletionCallback::Block());
  if (rv == PP_OK || rv == PP_ERROR_WOULDBLOCK)
    return "Flush succeeded from the main thread with no callback.";

  // Test flushing with no operations still issues a callback.
  // (This may also hang if the browser never issues the callback).
  pp::Graphics2D dc_nopaints(instance_, pp::Size(w, h), false);
  if (dc.is_null())
    return "Failure creating the nopaint device";
  if (!FlushAndWaitForDone(&dc_nopaints))
    return "Couldn't flush the nopaint device";

  // Test that multiple flushes fail if we don't get a callback in between.
  rv = dc_nopaints.Flush(pp::CompletionCallback(&FlushCallbackNOP, NULL));
  if (rv != PP_OK && rv != PP_ERROR_WOULDBLOCK)
    return "Couldn't flush first time for multiple flush test.";

  if (rv != PP_OK) {
    // If the first flush would block, then a second should fail.
    rv = dc_nopaints.Flush(pp::CompletionCallback(&FlushCallbackNOP, NULL));
    if (rv == PP_OK || rv == PP_ERROR_WOULDBLOCK)
      return "Second flush succeeded before callback ran.";
  }

  PASS();
}
