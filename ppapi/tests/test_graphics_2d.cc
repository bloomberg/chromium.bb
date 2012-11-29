// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_graphics_2d.h"

#include <stdlib.h>
#include <string.h>

#include <set>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/graphics_2d_dev.h"
#include "ppapi/cpp/dev/graphics_2d_dev.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Graphics2D);

namespace {

// A NOP flush callback for use in various tests.
void FlushCallbackNOP(void* data, int32_t result) {
}

void FlushCallbackQuitMessageLoop(void* data, int32_t result) {
  static_cast<TestGraphics2D*>(data)->QuitMessageLoop();
}

}  // namespace

TestGraphics2D::TestGraphics2D(TestingInstance* instance)
  : TestCase(instance),
    is_view_changed_(false),
    post_quit_on_view_changed_(false) {
}

bool TestGraphics2D::Init() {
  graphics_2d_interface_ = static_cast<const PPB_Graphics2D*>(
      pp::Module::Get()->GetBrowserInterface(PPB_GRAPHICS_2D_INTERFACE));
  image_data_interface_ = static_cast<const PPB_ImageData*>(
      pp::Module::Get()->GetBrowserInterface(PPB_IMAGEDATA_INTERFACE));
  return graphics_2d_interface_ && image_data_interface_ &&
         CheckTestingInterface();
}

void TestGraphics2D::RunTests(const std::string& filter) {
  RUN_TEST(InvalidResource, filter);
  RUN_TEST(InvalidSize, filter);
  RUN_TEST(Humongous, filter);
  RUN_TEST(InitToZero, filter);
  RUN_TEST(Describe, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(Paint, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(Scroll, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(Replace, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(Flush, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(FlushOffscreenUpdate, filter);
  RUN_TEST(Dev, filter);
  RUN_TEST(ReplaceContentsCaching, filter);
  RUN_TEST(BindNull, filter);
}

void TestGraphics2D::QuitMessageLoop() {
  testing_interface_->QuitMessageLoop(instance_->pp_instance());
}

bool TestGraphics2D::ReadImageData(const pp::Graphics2D& dc,
                                   pp::ImageData* image,
                                   const pp::Point& top_left) const {
  return PP_ToBool(testing_interface_->ReadImageData(
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

bool TestGraphics2D::ResourceHealthCheck(pp::Instance* instance,
                                         pp::Graphics2D* context) {
  TestCompletionCallback callback(instance->pp_instance(), callback_type());
  callback.WaitForResult(context->Flush(callback));
  if (callback.result() < 0)
    return callback.result() != PP_ERROR_FAILED;
  else if (callback.result() == 0)
    return false;
  return true;
}

bool TestGraphics2D::ResourceHealthCheckForC(pp::Instance* instance,
                                             PP_Resource graphics_2d) {
  TestCompletionCallback callback(instance->pp_instance(), callback_type());
  callback.WaitForResult(graphics_2d_interface_->Flush(
      graphics_2d, callback.GetCallback().pp_completion_callback()));
  if (callback.result() < 0)
    return callback.result() != PP_ERROR_FAILED;
  else if (callback.result() == 0)
    return false;
  return true;
}

bool TestGraphics2D::FlushAndWaitForDone(pp::Graphics2D* context) {
  int32_t flags = (force_async_ ? 0 : PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
  pp::CompletionCallback cc(&FlushCallbackQuitMessageLoop, this, flags);
  int32_t rv = context->Flush(cc);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return false;
  if (rv == PP_OK)
    return true;
  if (rv != PP_OK_COMPLETIONPENDING)
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


PP_Resource TestGraphics2D::ReplaceContentsAndReturnID(
    pp::Graphics2D* dc,
    const pp::Size& size) {
  pp::ImageData image(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL, size, true);

  PP_Resource id = image.pp_resource();

  dc->ReplaceContents(&image);
  if (!FlushAndWaitForDone(dc))
    return 0;

  return id;
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
          PP_MakeOptionalCompletionCallback(&FlushCallbackNOP, NULL)) == PP_OK)
    return "Flush succeeded with a different resource";
  if (graphics_2d_interface_->Flush(
          null_context.pp_resource(),
          PP_MakeOptionalCompletionCallback(&FlushCallbackNOP, NULL)) == PP_OK)
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
  if (ResourceHealthCheck(instance_, &a))
    return "0 height accepted";

  pp::Graphics2D b(instance_, pp::Size(0, 16), false);
  if (ResourceHealthCheck(instance_, &b))
    return "0 height accepted";

  // Need to use the C API since pp::Size prevents negative sizes.
  PP_Size size;
  size.width = 16;
  size.height = -16;
  PP_Resource graphics = graphics_2d_interface_->Create(
      instance_->pp_instance(), &size, PP_FALSE);
  ASSERT_FALSE(ResourceHealthCheckForC(instance_, graphics));

  size.width = -16;
  size.height = 16;
  graphics = graphics_2d_interface_->Create(
      instance_->pp_instance(), &size, PP_FALSE);
  ASSERT_FALSE(ResourceHealthCheckForC(instance_, graphics));

  // Overflow to negative size
  size.width = std::numeric_limits<int32_t>::max();
  size.height = std::numeric_limits<int32_t>::max();
  graphics = graphics_2d_interface_->Create(
      instance_->pp_instance(), &size, PP_FALSE);
  ASSERT_FALSE(ResourceHealthCheckForC(instance_, graphics));

  PASS();
}

std::string TestGraphics2D::TestHumongous() {
  pp::Graphics2D a(instance_, pp::Size(100000, 100000), false);
  if (ResourceHealthCheck(instance_, &a))
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
  const bool always_opaque = (::rand() % 2 == 1);
  pp::Graphics2D dc(instance_, pp::Size(w, h), always_opaque);
  if (dc.is_null())
    return "Failure creating a boring device";

  PP_Size size;
  size.width = -1;
  size.height = -1;
  PP_Bool is_always_opaque = PP_FALSE;
  if (!graphics_2d_interface_->Describe(dc.pp_resource(), &size,
                                        &is_always_opaque))
    return "Describe failed";
  if (size.width != w || size.height != h ||
      is_always_opaque != PP_FromBool(always_opaque))
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
  if (!instance_->BindGraphics(dc))
    return "Failure to bind the boring device.";

  // Make sure the device background is 0.
  if (!IsDCUniformColor(dc, 0))
    return "Bad initial color.";

  const int image_width = 15, image_height = 23;
  pp::ImageData test_image(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                           pp::Size(image_width, image_height), false);
  FillImageWithGradient(&test_image);
  pp::ImageData no_image(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                         pp::Size(image_width, image_height), false);
  FillRectInImage(&no_image, pp::Rect(0, 0, image_width, image_height), 0);
  pp::ImageData readback_image(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                               pp::Size(image_width, image_height), false);
  pp::ImageData readback_scroll(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                                pp::Size(image_width, image_height), false);

  if (test_image.size() != pp::Size(image_width, image_height))
    return "Wrong test image size\n";

  int image_x = 51, image_y = 72;
  dc.PaintImageData(test_image, pp::Point(image_x, image_y));
  if (!FlushAndWaitForDone(&dc))
    return "Couldn't flush to fill backing store.";

  // Test Case 1. Incorrect usage when scrolling image to a free space.
  // The clip area is *not* the area to shift around within the graphics device
  // by specified amount. It's the area to which the scroll is limited. So if
  // the clip area is the size of the image and the amount points to free space,
  // the scroll won't result in additional images.
  int dx = -40, dy = -48;
  int scroll_x = image_x + dx, scroll_y = image_y + dy;
  pp::Rect clip(image_x, image_y, image_width, image_height);
  dc.Scroll(clip, pp::Point(dx, dy));
  if (!FlushAndWaitForDone(&dc))
    return "TC1, Couldn't flush to scroll.";
  if (!ReadImageData(dc, &readback_scroll, pp::Point(scroll_x, scroll_y)))
    return "TC1, Couldn't read back scrolled image data.";
  if (!CompareImages(no_image, readback_scroll))
    return "TC1, Read back scrolled image is not the same as no image.";

  // Test Case 2.
  // The amount is intended to place the image in the free space outside
  // of the original, but the clip area extends beyond the graphics device area.
  // This scroll is invalid and will be a noop.
  scroll_x = 11, scroll_y = 24;
  clip = pp::Rect(0, 0, w, h + 1);
  dc.Scroll(clip, pp::Point(scroll_x - image_x, scroll_y - image_y));
  if (!FlushAndWaitForDone(&dc))
    return "TC2, Couldn't flush to scroll.";
  if (!ReadImageData(dc, &readback_scroll, pp::Point(scroll_x, scroll_y)))
    return "TC2, Couldn't read back scrolled image data.";
  if (!CompareImages(no_image, readback_scroll))
    return "TC2, Read back scrolled image is not the same as no image.";

  // Test Case 3.
  // The amount is intended to place the image in the free space outside
  // of the original, but the clip area does not cover the image,
  // so there is nothing to scroll.
  scroll_x = 11, scroll_y = 24;
  clip = pp::Rect(0, 0, image_x, image_y);
  dc.Scroll(clip, pp::Point(scroll_x - image_x, scroll_y - image_y));
  if (!FlushAndWaitForDone(&dc))
    return "TC3, Couldn't flush to scroll.";
  if (!ReadImageData(dc, &readback_scroll, pp::Point(scroll_x, scroll_y)))
    return "TC3, Couldn't read back scrolled image data.";
  if (!CompareImages(no_image, readback_scroll))
    return "TC3, Read back scrolled image is not the same as no image.";

  // Test Case 4.
  // Same as TC3, but the clip covers part of the image.
  // This part will be scrolled to the intended origin.
  int part_w = image_width / 2, part_h = image_height / 2;
  clip = pp::Rect(0, 0, image_x + part_w, image_y + part_h);
  dc.Scroll(clip, pp::Point(scroll_x - image_x, scroll_y - image_y));
  if (!FlushAndWaitForDone(&dc))
    return "TC4, Couldn't flush to scroll.";
  if (!ReadImageData(dc, &readback_scroll, pp::Point(scroll_x, scroll_y)))
    return "TC4, Couldn't read back scrolled image data.";
  if (CompareImages(test_image, readback_scroll))
    return "TC4, Read back scrolled image is the same as test image.";
  pp::Rect part_rect(part_w, part_h);
  if (!CompareImageRect(test_image, part_rect, readback_scroll, part_rect))
    return "TC4, Read back scrolled image is not the same as part test image.";

  // Test Case 5
  // Same as TC3, but the clip area covers the entire image.
  // It will be scrolled to the intended origin.
  clip = pp::Rect(0, 0, image_x + image_width, image_y + image_height);
  dc.Scroll(clip, pp::Point(scroll_x - image_x, scroll_y - image_y));
  if (!FlushAndWaitForDone(&dc))
    return "TC5, Couldn't flush to scroll.";
  if (!ReadImageData(dc, &readback_scroll, pp::Point(scroll_x, scroll_y)))
    return "TC5, Couldn't read back scrolled image data.";
  if (!CompareImages(test_image, readback_scroll))
    return "TC5, Read back scrolled image is not the same as test image.";

  // Note that the undefined area left by the scroll does not actually get
  // cleared, so the original image is still there. This is not guaranteed and
  // is not something for users to rely on, but we can test for this here, so
  // we know when the underlying behavior changes.
  if (!ReadImageData(dc, &readback_image, pp::Point(image_x, image_y)))
    return "Couldn't read back original image data.";
  if (!CompareImages(test_image, readback_image))
    return "Read back original image is not the same as test image.";

  // Test Case 6.
  // Scroll image to an overlapping space. The clip area is limited
  // to the image, so this will just modify its area.
  dx = 6;
  dy = 9;
  scroll_x = image_x + dx;
  scroll_y = image_y + dy;
  clip = pp::Rect(image_x, image_y, image_width, image_height);
  dc.Scroll(clip, pp::Point(dx, dy));
  if (!FlushAndWaitForDone(&dc))
    return "TC6, Couldn't flush to scroll.";
  if (!ReadImageData(dc, &readback_image, pp::Point(image_x, image_y)))
    return "TC6, Couldn't read back image data.";
  if (CompareImages(test_image, readback_image))
    return "TC6, Read back image is still the same as test image.";
  pp::Rect scroll_rect(image_width - dx, image_height - dy);
  if (!ReadImageData(dc, &readback_scroll, pp::Point(scroll_x, scroll_y)))
    return "TC6, Couldn't read back scrolled image data.";
  if (!CompareImageRect(test_image, scroll_rect, readback_scroll, scroll_rect))
    return "TC6, Read back scrolled image is not the same as part test image.";

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
  const int32_t swapped_color = 0x00FF00FF;
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

  int32_t rv = dc.Flush(pp::BlockUntilComplete());
  if (rv == PP_OK || rv == PP_OK_COMPLETIONPENDING)
    return "Flush succeeded from the main thread with no callback.";

  // Test flushing with no operations still issues a callback.
  // (This may also hang if the browser never issues the callback).
  pp::Graphics2D dc_nopaints(instance_, pp::Size(w, h), false);
  if (dc.is_null())
    return "Failure creating the nopaint device";
  if (!FlushAndWaitForDone(&dc_nopaints))
    return "Couldn't flush the nopaint device";

  int32_t flags = (force_async_ ? 0 : PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);

  // Test that multiple flushes fail if we don't get a callback in between.
  rv = dc_nopaints.Flush(pp::CompletionCallback(&FlushCallbackNOP, NULL,
                                                flags));
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return "Flush must complete asynchronously.";
  if (rv != PP_OK && rv != PP_OK_COMPLETIONPENDING)
    return "Couldn't flush first time for multiple flush test.";

  if (rv == PP_OK_COMPLETIONPENDING) {
    // If the first flush completes asynchronously, then a second should fail.
    rv = dc_nopaints.Flush(pp::CompletionCallback(&FlushCallbackNOP, NULL,
                                                  flags));
    if (force_async_) {
      if (rv != PP_OK_COMPLETIONPENDING)
        return "Second flush must fail asynchronously.";
    } else {
      if (rv == PP_OK || rv == PP_OK_COMPLETIONPENDING)
        return "Second flush succeeded before callback ran.";
    }
  }

  PASS();
}

void TestGraphics2D::DidChangeView(const pp::View& view) {
  if (post_quit_on_view_changed_) {
    post_quit_on_view_changed_ = false;
    is_view_changed_ = true;
    testing_interface_->QuitMessageLoop(instance_->pp_instance());
  }
}

void TestGraphics2D::ResetViewChangedState() {
  is_view_changed_ = false;
}

bool TestGraphics2D::WaitUntilViewChanged() {
  // Run a nested message loop. It will exit either on ViewChanged or if the
  // timeout happens.

  // If view changed before we have chance to run message loop, return directly.
  if (is_view_changed_)
    return true;

  post_quit_on_view_changed_ = true;
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  post_quit_on_view_changed_ = false;

  return is_view_changed_;
}

std::string TestGraphics2D::TestFlushOffscreenUpdate() {
  // Tests that callback of offscreen updates should be delayed.
  const PP_Time kFlushDelaySec = 1. / 30;  // 30 fps
  const int w = 80, h = 80;
  pp::Graphics2D dc(instance_, pp::Size(w, h), true);
  if (dc.is_null())
    return "Failure creating a boring device";
  if (!instance_->BindGraphics(dc))
    return "Failure to bind the boring device.";

  // Squeeze from top until bottom half of plugin is out of screen.
  ResetViewChangedState();
  instance_->EvalScript(
      "var big = document.createElement('div');"
      "var offset = "
      "    window.innerHeight - plugin.offsetTop - plugin.offsetHeight / 2;"
      "big.setAttribute('id', 'big-div');"
      "big.setAttribute('style', 'height: ' + offset + '; width: 100%;');"
      "document.body.insertBefore(big, document.body.firstChild);");
  if (!WaitUntilViewChanged())
    return "View didn't change as expected";

  // Allocate a red image chunk
  pp::ImageData chunk(instance_, PP_IMAGEDATAFORMAT_RGBA_PREMUL,
                      pp::Size(w/8, h/8), true);
  if (chunk.is_null())
    return "Failure to allocate image";
  const uint32_t kRed = 0xff0000ff;
  FillRectInImage(&chunk, pp::Rect(chunk.size()), kRed);

  // Paint a invisable chunk, expecting Flush to invoke callback slowly.
  dc.PaintImageData(chunk, pp::Point(0, h*0.75));

  PP_Time begin = pp::Module::Get()->core()->GetTime();
  if (!FlushAndWaitForDone(&dc))
    return "Couldn't flush an invisible paint";
  PP_Time actual_time_elapsed = pp::Module::Get()->core()->GetTime() - begin;
  // Expect actual_time_elapsed >= kFlushDelaySec, but loose a bit to avoid
  // precision issue.
  if (actual_time_elapsed < kFlushDelaySec * 0.9)
    return "Offscreen painting should be delayed";

  // Remove the padding on the top since test cases here isn't independent.
  instance_->EvalScript(
      "var big = document.getElementById('big-div');"
      "big.parentNode.removeChild(big);");
  ResetViewChangedState();
  if (!WaitUntilViewChanged())
    return "View didn't change as expected";

  PASS();
}

std::string TestGraphics2D::TestDev() {
  // Tests GetScale/SetScale via the Graphics2D_Dev C++ wrapper
  const int w = 20, h = 16;
  const float scale = 1.0f/2.0f;
  pp::Graphics2D dc(instance_, pp::Size(w, h), false);
  if (dc.is_null())
    return "Failure creating a boring device";
  pp::Graphics2D_Dev dc_dev(dc);
  if (dc_dev.GetScale() != 1.0f)
    return "GetScale returned unexpected value before SetScale";
  if (!dc_dev.SetScale(scale))
    return "SetScale failed";
  if (dc_dev.GetScale() != scale)
    return "GetScale mismatch with prior SetScale";
  // Try setting a few invalid scale factors. Ensure that we catch these errors
  // and don't change the actual scale
  if (dc_dev.SetScale(-1.0f))
    return "SetScale(-1f) did not fail";
  if (dc_dev.SetScale(0.0f))
    return "SetScale(0.0f) did not fail";
  if (dc_dev.GetScale() != scale)
    return "SetScale with invalid parameter overwrote the scale";

  // Verify that the context has the specified number of pixels, despite the
  // non-identity scale
  PP_Size size;
  size.width = -1;
  size.height = -1;
  PP_Bool is_always_opaque = PP_FALSE;
  if (!graphics_2d_interface_->Describe(dc_dev.pp_resource(), &size,
                                        &is_always_opaque))
    return "Describe failed";
  if (size.width != w || size.height != h ||
      is_always_opaque != PP_FromBool(false))
    return "Mismatch of data.";

  PASS();
}

// This test makes sure that the out-of-process image data caching works as
// expected. Doing ReplaceContents quickly should re-use the image data from
// older ones.
std::string TestGraphics2D::TestReplaceContentsCaching() {
  // The cache is only active when running in the proxy, so skip it otherwise.
  if (!testing_interface_->IsOutOfProcess())
    PASS();

  // Here we test resource IDs as a way to determine if the resource is being
  // cached and re-used. This is non-optimal since it's entirely possible
  // (and maybe better) for the proxy to return new resource IDs for the
  // re-used objects. Howevever, our current implementation does this so it is
  // an easy thing to check for.
  //
  // You could check for the shared memory pointers getting re-used, but the
  // OS is very likely to use the same memory location for a newly-mapped image
  // if one was deleted, meaning that it could pass even if the cache is broken.
  // This would then require that we add some address-re-use-preventing code
  // which would be tricky.
  std::set<PP_Resource> resources;

  pp::Size size(16, 16);
  pp::Graphics2D dc(instance_, size, false);

  // Do two replace contentses, adding the old resource IDs to our map.
  PP_Resource imageres = ReplaceContentsAndReturnID(&dc, size);
  ASSERT_TRUE(imageres);
  resources.insert(imageres);
  imageres = ReplaceContentsAndReturnID(&dc, size);
  ASSERT_TRUE(imageres);
  resources.insert(imageres);

  // Now doing more replace contents should re-use older IDs if the cache is
  // working.
  imageres = ReplaceContentsAndReturnID(&dc, size);
  ASSERT_TRUE(resources.find(imageres) != resources.end());
  imageres = ReplaceContentsAndReturnID(&dc, size);
  ASSERT_TRUE(resources.find(imageres) != resources.end());

  PASS();
}

std::string TestGraphics2D::TestBindNull() {
  // Binding a null resource is not an error, it should clear all bound
  // resources. We can't easily test what resource is bound, but we can test
  // that this doesn't throw an error.
  ASSERT_TRUE(instance_->BindGraphics(pp::Graphics2D()));
  ASSERT_TRUE(instance_->BindGraphics(pp::Graphics3D()));

  const int w = 115, h = 117;
  pp::Graphics2D dc(instance_, pp::Size(w, h), false);
  if (dc.is_null())
    return "Failure creating device.";
  if (!instance_->BindGraphics(dc))
    return "Failure to bind the boring device.";

  ASSERT_TRUE(instance_->BindGraphics(pp::Graphics2D()));
  ASSERT_TRUE(instance_->BindGraphics(pp::Graphics3D()));

  PASS();
}

