// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Test cases for PPB_Graphics2D functions.
// As most of them return void, the test automatically confirms that
// there is no crash while requiring a visual inspection of the painted output.

#include <string.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_url_loader.h"

namespace {

const PP_Bool kAlwaysOpaque = PP_TRUE;
const PP_Bool kNotAlwaysOpaque = PP_FALSE;
const PP_Size kNegativeWidth = PP_MakeSize(-1, 1);
const PP_Size kNegativeHeight = PP_MakeSize(1, -1);
const PP_Size kZeroPixels = PP_MakeSize(0, 0);
const PP_Size kOnePixel = PP_MakeSize(1, 1);
const PP_Size k90x90 = PP_MakeSize(90, 90);
const PP_Size k60x60 = PP_MakeSize(60, 60);
const PP_Size k30x30 = PP_MakeSize(30, 30);
const PP_Size k2500x2500 = PP_MakeSize(2500, 2500);
const PP_Size k100000x100000 = PP_MakeSize(100000, 100000);
const PP_Point kOrigin = PP_MakePoint(0, 0);
const PP_Rect kBadRect = { PP_MakePoint(-1, -1), PP_MakeSize(-1, -1) };
const PP_Rect* kEntireImage = NULL;
const PP_Bool kInitToZero = PP_TRUE;

// TODO(polina, nfullagar): allow specification of non-premultipled colors
// and provide alpha premultiplcation in FormatColor(). This will be required
// when future PPAPI pixel formats are extended to include non-premultipled
// or ignored alpha.

struct ColorPremul { uint32_t A, R, G, B; };  // Use premultipled Alpha.
const ColorPremul kSheerRed = { 0x88, 0x88, 0x00, 0x00 };
const ColorPremul kSheerBlue = { 0x88, 0x00, 0x00, 0x88 };
const ColorPremul kSheerGray = { 0x77, 0x55, 0x55, 0x55 };
const ColorPremul kOpaqueGreen = { 0xFF, 0x00, 0xFF, 0x00 };
const ColorPremul kOpaqueBlack = { 0xFF, 0x00, 0x00, 0x00 };
const ColorPremul kOpaqueWhite = { 0xFF, 0xFF, 0xFF, 0xFF };
const ColorPremul kOpaqueYellow = { 0xFF, 0xFF, 0xFF, 0x00 };
const int kBytesPerPixel = sizeof(uint32_t);  // 4 bytes for BGRA or RGBA.

// Assumes premultipled Alpha.
uint32_t FormatColor(PP_ImageDataFormat format, ColorPremul color) {
  if (format == PP_IMAGEDATAFORMAT_BGRA_PREMUL)
    return (color.A << 24) | (color.R << 16) | (color.G << 8) | (color.B);
  else if (format == PP_IMAGEDATAFORMAT_RGBA_PREMUL)
    return (color.A << 24) | (color.B << 16) | (color.G << 8) | (color.R);
  else
    NACL_NOTREACHED();
}

// Make graphics2d contexts for each test the same size, so we can layer
// the images without invalidating the previous ones.
PP_Resource CreateGraphics2D_90x90() {
  PP_Resource graphics2d = PPBGraphics2D()->Create(
      pp_instance(), &k90x90, kNotAlwaysOpaque);
  CHECK(graphics2d != kInvalidResource);
  PPBInstance()->BindGraphics(pp_instance(), graphics2d);
  return graphics2d;
}

PP_Resource CreateImageData(PP_Size size, ColorPremul pixel_color, void** bmp) {
  PP_ImageDataFormat image_format = PPBImageData()->GetNativeImageDataFormat();
  uint32_t formatted_pixel_color = FormatColor(image_format, pixel_color);
  PP_Resource image_data = PPBImageData()->Create(
      pp_instance(), image_format, &size, kInitToZero);
  CHECK(image_data != kInvalidResource);
  PP_ImageDataDesc image_desc;
  CHECK(PPBImageData()->Describe(image_data, &image_desc) == PP_TRUE);
  *bmp = NULL;
  *bmp = PPBImageData()->Map(image_data);
  CHECK(*bmp != NULL);
  uint32_t* bmp_words = static_cast<uint32_t*>(*bmp);
  int num_pixels = image_desc.stride / kBytesPerPixel * image_desc.size.height;
  for (int i = 0; i < num_pixels; i++)
    bmp_words[i] = formatted_pixel_color;
  return image_data;
}

PP_Resource CreateImageData(PP_Size size, ColorPremul pixel_color) {
  void* bitmap = NULL;
  return CreateImageData(size, pixel_color, &bitmap);
}

struct FlushData {
  FlushData(PP_Resource g, PP_Resource i) : graphics2d(g), image_data(i) {}
  PP_Resource graphics2d;
  PP_Resource image_data;
};

void FlushCallback(void* user_data, int32_t result);
int g_expected_flush_calls = 0;
PP_CompletionCallback MakeTestableFlushCallback(const char* name,
                                                PP_Resource graphics2d,
                                                PP_Resource image_data,
                                                int num_calls) {
  g_expected_flush_calls = num_calls;
  void* user_data = new FlushData(graphics2d, image_data);
  return MakeTestableCompletionCallback(name, FlushCallback, user_data);
}

void FlushCallback(void* user_data, int32_t result) {
  --g_expected_flush_calls;
  CHECK(g_expected_flush_calls >= 0);
  CHECK(user_data != NULL);
  CHECK(result == PP_OK);

  FlushData* data = static_cast<FlushData*>(user_data);
  PP_Resource graphics2d = data->graphics2d;
  PP_Resource image_data = data->image_data;
  if (g_expected_flush_calls == 0) {
    PPBCore()->ReleaseResource(graphics2d);
    PPBCore()->ReleaseResource(image_data);
    delete data;
  } else {
    PPBGraphics2D()->PaintImageData(graphics2d, image_data, &kOrigin, NULL);
    PP_CompletionCallback cc = MakeTestableFlushCallback(
        "FlushAnimationCallback",
        graphics2d, image_data, g_expected_flush_calls);
    CHECK(PP_OK_COMPLETIONPENDING == PPBGraphics2D()->Flush(graphics2d, cc));
  }
}

bool IsEqualSize(PP_Size size1, PP_Size size2) {
  return (size1.width == size2.width && size1.height == size2.height);
}

bool IsSquareOnScreen(PP_Resource graphics2d, PP_Point origin, PP_Size square,
                      ColorPremul color) {
  PP_Size size;
  PP_Bool dummy;
  CHECK(PP_TRUE == PPBGraphics2D()->Describe(graphics2d, &size, &dummy));

  void* bitmap = NULL;
  PP_Resource image = CreateImageData(size, kOpaqueBlack, &bitmap);

  PP_ImageDataDesc image_desc;
  CHECK(PP_TRUE == PPBImageData()->Describe(image, &image_desc));
  int32_t stride = image_desc.stride / kBytesPerPixel;  // width + padding.
  uint32_t expected_color = FormatColor(image_desc.format, color);
  CHECK(origin.x >= 0 && origin.y >= 0 &&
        (origin.x + square.width) <= stride &&
        (origin.y + square.height) <= image_desc.size.height);

  CHECK(PP_TRUE == PPBTestingDev()->ReadImageData(graphics2d, image, &kOrigin));
  bool found_error = false;
  for (int y = origin.y; y < origin.y + square.height && !found_error; y++) {
    for (int x = origin.x; x < origin.x + square.width && !found_error; x++) {
      uint32_t pixel_color = static_cast<uint32_t*>(bitmap)[stride * y + x];
      found_error = (pixel_color != expected_color);
    }
  }

  PPBCore()->ReleaseResource(image);
  return !found_error;
}

////////////////////////////////////////////////////////////////////////////////
// Test Cases
////////////////////////////////////////////////////////////////////////////////

// Tests PPB_Graphics2D::Create().
void TestCreate() {
  PP_Resource graphics2d = kInvalidResource;
  const PPB_Graphics2D* ppb = PPBGraphics2D();

  // Invalid instance and size -> invalid resource.
  graphics2d = ppb->Create(kInvalidInstance, &k30x30, kAlwaysOpaque);
  EXPECT(graphics2d == kInvalidResource);
  graphics2d = ppb->Create(kInvalidInstance, &k30x30, kNotAlwaysOpaque);
  EXPECT(graphics2d == kInvalidResource);
  graphics2d = ppb->Create(kNotAnInstance, &k30x30, kAlwaysOpaque);
  EXPECT(graphics2d == kInvalidResource);
  graphics2d = ppb->Create(kNotAnInstance, &k30x30, kNotAlwaysOpaque);
  EXPECT(graphics2d == kInvalidResource);
  graphics2d = ppb->Create(pp_instance(), &k100000x100000, kAlwaysOpaque);
  EXPECT(graphics2d == kInvalidResource);
  graphics2d = ppb->Create(pp_instance(), &k100000x100000, kNotAlwaysOpaque);
  EXPECT(graphics2d == kInvalidResource);
  graphics2d = ppb->Create(pp_instance(), &kZeroPixels, kAlwaysOpaque);
  EXPECT(graphics2d == kInvalidResource);
  graphics2d = ppb->Create(pp_instance(), &kZeroPixels, kNotAlwaysOpaque);
  EXPECT(graphics2d == kInvalidResource);
  graphics2d = ppb->Create(pp_instance(), &kNegativeWidth, kAlwaysOpaque);
  EXPECT(graphics2d == kInvalidResource);
  graphics2d = ppb->Create(pp_instance(), &kNegativeHeight, kNotAlwaysOpaque);
  EXPECT(graphics2d == kInvalidResource);
  // NULL size -> Internal error in rpc method.
  graphics2d = ppb->Create(pp_instance(), NULL, kAlwaysOpaque);
  EXPECT(graphics2d == kInvalidResource);
  graphics2d = ppb->Create(pp_instance(), NULL, kNotAlwaysOpaque);
  EXPECT(graphics2d == kInvalidResource);

  // Valid instance and size -> valid resource.
  graphics2d = ppb->Create(pp_instance(), &kOnePixel, kAlwaysOpaque);
  EXPECT(graphics2d != kInvalidResource);
  PPBCore()->ReleaseResource(graphics2d);
  graphics2d = ppb->Create(pp_instance(), &kOnePixel, kNotAlwaysOpaque);
  EXPECT(graphics2d != kInvalidResource);
  PPBCore()->ReleaseResource(graphics2d);
  graphics2d = ppb->Create(pp_instance(), &k30x30, kAlwaysOpaque);
  EXPECT(graphics2d != kInvalidResource);
  PPBCore()->ReleaseResource(graphics2d);
  graphics2d = ppb->Create(pp_instance(), &k30x30, kNotAlwaysOpaque);
  EXPECT(graphics2d != kInvalidResource);
  PPBCore()->ReleaseResource(graphics2d);
  graphics2d = ppb->Create(pp_instance(), &k2500x2500, kAlwaysOpaque);
  EXPECT(graphics2d != kInvalidResource);
  PPBCore()->ReleaseResource(graphics2d);
  graphics2d = ppb->Create(pp_instance(), &k2500x2500, kNotAlwaysOpaque);
  EXPECT(graphics2d != kInvalidResource);
  PPBCore()->ReleaseResource(graphics2d);

  TEST_PASSED;
}

// Tests PPB_Graphics2D::IsGraphics2D().
void TestIsGraphics2D() {
  // Invalid / non-existent / non-Graphics2D resource -> false.
  EXPECT(PP_FALSE == PPBGraphics2D()->IsGraphics2D(kInvalidResource));
  EXPECT(PP_FALSE == PPBGraphics2D()->IsGraphics2D(kNotAResource));
  PP_Resource url_loader = PPBURLLoader()->Create(pp_instance());
  CHECK(url_loader != kInvalidResource);
  EXPECT(PP_FALSE == PPBGraphics2D()->IsGraphics2D(url_loader));
  PPBCore()->ReleaseResource(url_loader);

  // Current Graphics2D resource -> true.
  PP_Resource graphics2d = PPBGraphics2D()->Create(
      pp_instance(), &k30x30, kAlwaysOpaque);
  EXPECT(PP_TRUE == PPBGraphics2D()->IsGraphics2D(graphics2d));

  // Released Graphis2D resource -> false.
  PPBCore()->ReleaseResource(graphics2d);
  EXPECT(PP_FALSE == PPBGraphics2D()->IsGraphics2D(graphics2d));

  TEST_PASSED;
}

// Tests PPB_Graphics2D::Describe().
void TestDescribe() {
  PP_Resource graphics2d = kInvalidResource;
  const PPB_Graphics2D* ppb = PPBGraphics2D();
  struct PP_Size size = k90x90;
  PP_Bool is_always_opaque = PP_TRUE;

  // Valid resource -> output = configuration, true.
  graphics2d = ppb->Create(pp_instance(), &k30x30, kNotAlwaysOpaque);
  EXPECT(PP_TRUE == ppb->Describe(graphics2d, &size, &is_always_opaque));
  EXPECT(is_always_opaque == PP_FALSE && IsEqualSize(size, k30x30));
  PPBCore()->ReleaseResource(graphics2d);

  graphics2d = ppb->Create(pp_instance(), &k30x30, kAlwaysOpaque);
  EXPECT(PP_TRUE == ppb->Describe(graphics2d, &size, &is_always_opaque));
  EXPECT(is_always_opaque == PP_TRUE && IsEqualSize(size, k30x30));
  PPBCore()->ReleaseResource(graphics2d);

  // NULL outputs -> output = unchanged, false.
  EXPECT(PP_FALSE == ppb->Describe(graphics2d, NULL, &is_always_opaque));
  EXPECT(PP_FALSE == ppb->Describe(graphics2d, &size, NULL));
  EXPECT(is_always_opaque == PP_TRUE && IsEqualSize(size, k30x30));

  // Invalid / non-existent resource -> output = 0, false.
  EXPECT(PP_FALSE == ppb->Describe(kInvalidResource, &size, &is_always_opaque));
  EXPECT(is_always_opaque == PP_FALSE && IsEqualSize(size, kZeroPixels));

  is_always_opaque = PP_TRUE;
  size = k90x90;
  EXPECT(PP_FALSE == ppb->Describe(kNotAResource, &size, &is_always_opaque));
  EXPECT(is_always_opaque == PP_FALSE && IsEqualSize(size, kZeroPixels));

  TEST_PASSED;
}

// Tests PPB_Graphics2D::PaintImageData() with specified image rect.
// Draws a blue square in the top right corner.
// Requires a visual inspection.
void TestPaintImageData() {
  const PPB_Graphics2D* ppb = PPBGraphics2D();
  PP_Resource graphics2d = CreateGraphics2D_90x90();
  PP_Resource image_data = CreateImageData(k60x60, kSheerBlue);
  PP_Resource image_data_noop = CreateImageData(k60x60, kOpaqueBlack);
  PP_Rect src_rect = { PP_MakePoint(30, 30), k30x30 };
  PP_Point top_left = PP_MakePoint(30, -30);  // target origin = (60,  0);
  PP_Point clip_up = PP_MakePoint(0, -31);    // target origin = (30, -1)
  PP_Point clip_left = PP_MakePoint(-31, 0);  // target origin = (-1, 30)
  PP_Point clip_right = PP_MakePoint(31, 0);  // target origin = (61, 30)
  PP_Point clip_down = PP_MakePoint(0, 31);   // target origin = (30, 61)

  // Valid args -> copies to backing store and paints to screen after Flush().
  ppb->PaintImageData(graphics2d, image_data, &top_left, &src_rect);

  // Invalid args -> no effect, no crash.
  ppb->PaintImageData(kInvalidResource, image_data_noop, &top_left, &src_rect);
  ppb->PaintImageData(kNotAResource, image_data_noop, &top_left, &src_rect);
  ppb->PaintImageData(graphics2d, kInvalidResource, &top_left, &src_rect);
  ppb->PaintImageData(graphics2d, kNotAResource, &top_left, &src_rect);
  ppb->PaintImageData(graphics2d, image_data_noop, &clip_up, &src_rect);
  ppb->PaintImageData(graphics2d, image_data_noop, &clip_left, &src_rect);
  ppb->PaintImageData(graphics2d, image_data_noop, &clip_right, &src_rect);
  ppb->PaintImageData(graphics2d, image_data_noop, &clip_down, &src_rect);
  ppb->PaintImageData(graphics2d, image_data_noop, &kOrigin, &kBadRect);
  // NULL top_left - > Internal error in rpc method.
  ppb->PaintImageData(graphics2d, image_data_noop, NULL, &src_rect);
  ppb->PaintImageData(kInvalidResource, kNotAResource, NULL, &src_rect);

  // Paints backing store image to screen only after Flush().
  PP_Point top_right = PP_MakePoint(60, 0);
  EXPECT(!IsSquareOnScreen(graphics2d, top_right, k30x30, kSheerBlue));
  PP_CompletionCallback cc = MakeTestableFlushCallback(
      "PaintImageDataFlushCallback", graphics2d, image_data, 1);
  EXPECT(PP_OK_COMPLETIONPENDING == ppb->Flush(graphics2d, cc));
  EXPECT(IsSquareOnScreen(graphics2d, top_right, k30x30, kSheerBlue));

  // This should have no effect on Flush().
  ppb->PaintImageData(graphics2d, image_data_noop, &top_left, &src_rect);
  PPBCore()->ReleaseResource(image_data_noop);

  TEST_PASSED;
}

// Tests PPB_Graphics2D::PaintImageData() with default rect for entire image.
// Draws a yellow square in the bottom left corner.
// Requires a visual inspection.
void TestPaintImageDataEntire() {
  const PPB_Graphics2D* ppb = PPBGraphics2D();
  PP_Resource graphics2d = CreateGraphics2D_90x90();
  PP_Resource image_data = CreateImageData(k30x30, kOpaqueYellow);
  PP_Resource image_data_noop = CreateImageData(k30x30, kOpaqueBlack);
  PP_Point bottom_left = PP_MakePoint(0, 60);
  PP_Point clip_up = PP_MakePoint(0, -1);
  PP_Point clip_left = PP_MakePoint(-1, 0);
  PP_Point clip_right = PP_MakePoint(61, 0);
  PP_Point clip_down = PP_MakePoint(0, 61);

  // Valid args -> copies to backing store.
  ppb->PaintImageData(graphics2d, image_data, &bottom_left, kEntireImage);

  // Invalid args -> no effect, no crash.
  ppb->PaintImageData(
      kInvalidResource, image_data_noop, &bottom_left, kEntireImage);
  ppb->PaintImageData(
      kNotAResource, image_data_noop, &bottom_left, kEntireImage);
  ppb->PaintImageData(graphics2d, kInvalidResource, &bottom_left, kEntireImage);
  ppb->PaintImageData(graphics2d, kNotAResource, &bottom_left, kEntireImage);
  ppb->PaintImageData(graphics2d, image_data_noop, &clip_up, kEntireImage);
  ppb->PaintImageData(graphics2d, image_data_noop, &clip_left, kEntireImage);
  ppb->PaintImageData(graphics2d, image_data_noop, &clip_right, kEntireImage);
  ppb->PaintImageData(graphics2d, image_data_noop, &clip_down, kEntireImage);
  // NULL top_left - > Internal error in rpc method.
  ppb->PaintImageData(graphics2d, image_data_noop, NULL, kEntireImage);
  ppb->PaintImageData(kInvalidResource, kNotAResource, NULL, kEntireImage);

  // Paints backing store image to the screen only after Flush().
  EXPECT(!IsSquareOnScreen(graphics2d, bottom_left, k30x30, kOpaqueYellow));
  PP_CompletionCallback cc = MakeTestableFlushCallback(
      "PaintImageDataEntireFlushCallback", graphics2d, image_data, 1);
  EXPECT(PP_OK_COMPLETIONPENDING == ppb->Flush(graphics2d, cc));
  EXPECT(IsSquareOnScreen(graphics2d, bottom_left, k30x30, kOpaqueYellow));

  // This should have no effect on Flush().
  ppb->PaintImageData(graphics2d, image_data_noop, &bottom_left, kEntireImage);
  PPBCore()->ReleaseResource(image_data_noop);

  TEST_PASSED;
}

// Tests PPB_Graphics2D::Scroll() with specified image rect.
// Draws a white square at the top left, then in the middle.
// Requires a visual inspection.
void TestScroll() {
  const PPB_Graphics2D* ppb = PPBGraphics2D();
  PP_Resource graphics2d = CreateGraphics2D_90x90();
  PP_Resource image_data = CreateImageData(k30x30, kOpaqueWhite);
  PP_Rect src_rect = { kOrigin, k30x30 };
  PP_Rect clip_rect = { kOrigin, k60x60 };
  PP_Point middle = PP_MakePoint(30, 30);
  ppb->PaintImageData(graphics2d, image_data, &kOrigin, &src_rect);

  // Valid args -> scrolls backing store and paints to screen after Flush().
  ppb->Scroll(graphics2d, &clip_rect, &middle);

  // Invalid args -> no effect, no crash.
  ppb->Scroll(kInvalidResource, &clip_rect, &middle);
  ppb->Scroll(kNotAResource, &clip_rect, &middle);
  ppb->Scroll(graphics2d, &clip_rect, NULL);  // Internal error in rpc method.

  // Paints backing store image to the sreen only after Flush().
  EXPECT(!IsSquareOnScreen(graphics2d, middle, k30x30, kOpaqueWhite));
  PP_CompletionCallback cc = MakeTestableFlushCallback(
      "ScrollFlushCallback", graphics2d, image_data, 1);
  EXPECT(PP_OK_COMPLETIONPENDING == ppb->Flush(graphics2d, cc));
  EXPECT(IsSquareOnScreen(graphics2d, middle, k30x30, kOpaqueWhite));

  TEST_PASSED;
}

// Tests PPB_Graphics2D::Scroll() with default rect for entire image..
// Draws a green square in the top left, then bottom right.
// Requires a visual inspection.
void TestScrollEntire() {
  const PPB_Graphics2D* ppb = PPBGraphics2D();
  PP_Resource graphics2d = CreateGraphics2D_90x90();
  PP_Resource image_data = CreateImageData(k30x30, kOpaqueGreen);
  PP_Point bottom_right = PP_MakePoint(60, 60);
  ppb->PaintImageData(graphics2d, image_data, &kOrigin, kEntireImage);

  // Valid args -> scrolls backing store and paints to screen after Flush().
  ppb->Scroll(graphics2d, kEntireImage, &bottom_right);

  // Invalid args -> no crash.
  ppb->Scroll(kInvalidResource, kEntireImage, &bottom_right);
  ppb->Scroll(kNotAResource, kEntireImage, &bottom_right);
  ppb->Scroll(graphics2d, kEntireImage, NULL);  // Internal error in rpc method.

  // Paints backing store image to the screen only after Flush().
  EXPECT(!IsSquareOnScreen(graphics2d, bottom_right, k30x30, kOpaqueGreen));
  ppb->Scroll(graphics2d, kEntireImage, &bottom_right);
  PP_CompletionCallback cc = MakeTestableFlushCallback(
      "ScrollEntireFlushCallback", graphics2d, image_data, 1);
  EXPECT(PP_OK_COMPLETIONPENDING == ppb->Flush(graphics2d, cc));
  EXPECT(IsSquareOnScreen(graphics2d, bottom_right, k30x30, kOpaqueGreen));

  TEST_PASSED;
}

// Tests PPB_Graphics2D::ReplaceContents().
// Colors the entire graphics area gray.
// Requires a visual inspection.
void TestReplaceContents() {
  PP_Resource graphics2d = CreateGraphics2D_90x90();
  PP_Resource image_data = CreateImageData(k90x90, kSheerGray);
  PP_Resource image_data_noop = CreateImageData(k90x90, kOpaqueBlack);
  PP_Resource image_data_size_mismatch = CreateImageData(k30x30, kOpaqueBlack);

  // Valid args -> replaces backing store.
  PPBGraphics2D()->ReplaceContents(graphics2d, image_data);

  // Invalid args -> no effect, no crash.
  PPBGraphics2D()->ReplaceContents(kInvalidResource, image_data_noop);
  PPBGraphics2D()->ReplaceContents(kNotAResource, image_data_noop);
  PPBGraphics2D()->ReplaceContents(graphics2d, kInvalidResource);
  PPBGraphics2D()->ReplaceContents(graphics2d, kNotAResource);
  PPBGraphics2D()->ReplaceContents(kInvalidResource, kNotAResource);
  PPBGraphics2D()->ReplaceContents(graphics2d, image_data_size_mismatch);

  // Paints backing store image to the screen only after Flush().
  EXPECT(!IsSquareOnScreen(graphics2d, kOrigin, k90x90, kSheerGray));
  PP_CompletionCallback cc = MakeTestableFlushCallback(
      "ReplaceContentsFlushCallback", graphics2d, image_data, 1);
  EXPECT(PP_OK_COMPLETIONPENDING == PPBGraphics2D()->Flush(graphics2d, cc));
  EXPECT(IsSquareOnScreen(graphics2d, kOrigin, k90x90, kSheerGray));

  // This should have no effect on Flush().
  PPBGraphics2D()->ReplaceContents(graphics2d, image_data_noop);
  PPBCore()->ReleaseResource(image_data_noop);
  PPBCore()->ReleaseResource(image_data_size_mismatch);

  TEST_PASSED;
}

// Tests PPB_Graphics2D::Flush().
void TestFlush() {
  PP_Resource graphics2d = PPBGraphics2D()->Create(
      pp_instance(), &k90x90, kAlwaysOpaque);
  PP_CompletionCallback cc = MakeTestableFlushCallback(
      "FlushCallback", graphics2d, kInvalidResource, 1);

  // Invalid args -> PP_ERROR_BAD..., no callback.
  EXPECT(PP_ERROR_BADRESOURCE == PPBGraphics2D()->Flush(kInvalidResource, cc));
  EXPECT(PP_ERROR_BADRESOURCE == PPBGraphics2D()->Flush(kNotAResource, cc));
  EXPECT(PP_ERROR_BADARGUMENT ==
         PPBGraphics2D()->Flush(graphics2d, PP_BlockUntilComplete()));

  // Valid args -> PP_OK_COMPLETIONPENDING, expect callback.
  EXPECT(PP_OK_COMPLETIONPENDING == PPBGraphics2D()->Flush(graphics2d, cc));

  // Duplicate call -> PP_ERROR_INPROGRESS, no callback.
  EXPECT(PP_ERROR_INPROGRESS == PPBGraphics2D()->Flush(graphics2d, cc));

  TEST_PASSED;
}

// Tests continious Paint/Flush chaining.
void TestFlushAnimation() {
  PP_Resource graphics2d = CreateGraphics2D_90x90();
  PP_Resource image_data = CreateImageData(k30x30, kSheerRed);

  PPBGraphics2D()->PaintImageData(graphics2d, image_data, &kOrigin, NULL);
  PP_CompletionCallback cc = MakeTestableFlushCallback(
      "FlushAnimationCallback", graphics2d, image_data, 10);
  EXPECT(PP_OK_COMPLETIONPENDING == PPBGraphics2D()->Flush(graphics2d, cc));

  TEST_PASSED;
}

// Stress testing of a large number of resources.
void TestStress() {
  // TODO(nfullagar): Increase the number of resources once the cause of the
  // stress test flake is fixed.
  const int kManyResources = 100;
  PP_Resource graphics2d[kManyResources];
  const PPB_Graphics2D* ppb = PPBGraphics2D();

  for (int i = 0; i < kManyResources; i++) {
    graphics2d[i] = ppb->Create(pp_instance(), &k30x30, kAlwaysOpaque);
    EXPECT(graphics2d[i] != kInvalidResource);
    EXPECT(PP_TRUE == ppb->IsGraphics2D(graphics2d[i]));
  }
  for (int i = 0; i < kManyResources; i++) {
    PPBCore()->ReleaseResource(graphics2d[i]);
    EXPECT(PP_FALSE == PPBGraphics2D()->IsGraphics2D(graphics2d[i]));
  }

  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestCreate", TestCreate);
  RegisterTest("TestIsGraphics2D", TestIsGraphics2D);
  RegisterTest("TestDescribe", TestDescribe);
  RegisterTest("TestPaintImageData", TestPaintImageData);
  RegisterTest("TestPaintImageDataEntire", TestPaintImageDataEntire);
  RegisterTest("TestScroll", TestScroll);
  RegisterTest("TestScrollEntire", TestScrollEntire);
  RegisterTest("TestReplaceContents", TestReplaceContents);
  RegisterTest("TestFlush", TestFlush);
  RegisterTest("TestFlushAnimation", TestFlushAnimation);
  RegisterTest("TestStress", TestStress);
}

void SetupPluginInterfaces() {
  // none
}
