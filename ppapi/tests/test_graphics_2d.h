// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_GRAPHICS_2D_H_
#define PPAPI_TESTS_TEST_GRAPHICS_2D_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/tests/test_case.h"

namespace pp {
class Graphics2D;
class ImageData;
class Point;
class Rect;
}

class TestGraphics2D : public TestCase {
 public:
  explicit TestGraphics2D(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  void QuitMessageLoop();

 private:
  bool ReadImageData(const pp::Graphics2D& dc,
                     pp::ImageData* image,
                     const pp::Point& top_left) const;

  void FillRectInImage(pp::ImageData* image,
                       const pp::Rect& rect,
                       uint32_t color) const;

  // Fill image with gradient colors.
  void FillImageWithGradient(pp::ImageData* image) const;

  // Return true if images are the same.
  bool CompareImages(const pp::ImageData& image1,
                     const pp::ImageData& image2);

  // Return true if images within specified rectangles are the same.
  bool CompareImageRect(const pp::ImageData& image1,
                        const pp::Rect& rc1,
                        const pp::ImageData& image2,
                        const pp::Rect& rc2) const;

  // Validates that the given image is a single color with a square of another
  // color inside it.
  bool IsSquareInImage(const pp::ImageData& image_data,
                       uint32_t background_color,
                       const pp::Rect& square, uint32_t square_color) const;

  // Validates that the given device context is a single color with a square of
  // another color inside it.
  bool IsSquareInDC(const pp::Graphics2D& dc, uint32_t background_color,
                    const pp::Rect& square, uint32_t square_color) const;

  // Validates that the given device context is filled with the given color.
  bool IsDCUniformColor(const pp::Graphics2D& dc, uint32_t color) const;

  // Issues a flush on the given device context and blocks until the flush
  // has issued its callback. Returns true on success.
  bool FlushAndWaitForDone(pp::Graphics2D* context);

  std::string TestInvalidResource();
  std::string TestInvalidSize();
  std::string TestHumongous();
  std::string TestInitToZero();
  std::string TestDescribe();
  std::string TestPaint();
  std::string TestScroll();
  std::string TestReplace();
  std::string TestFlush();
  std::string TestDev();

  // Used by the tests that access the C API directly.
  const PPB_Graphics2D* graphics_2d_interface_;
  const PPB_ImageData* image_data_interface_;
};

#endif  // PPAPI_TESTS_TEST_GRAPHICS_2D_H_
