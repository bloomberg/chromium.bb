// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/ImageData.h"

#include "core/dom/ExceptionCode.h"
#include "platform/geometry/IntSize.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpaceXform.h"

namespace blink {
namespace {

class ImageDataTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // Save the state of experimental canvas features and color correct
    // rendering flags to restore them on teardown.
    experimental_canvas_features =
        RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled();
    color_correct_rendering =
        RuntimeEnabledFeatures::colorCorrectRenderingEnabled();
    RuntimeEnabledFeatures::setExperimentalCanvasFeaturesEnabled(true);
    RuntimeEnabledFeatures::setColorCorrectRenderingEnabled(true);
  }

  virtual void TearDown() {
    RuntimeEnabledFeatures::setExperimentalCanvasFeaturesEnabled(
        experimental_canvas_features);
    RuntimeEnabledFeatures::setColorCorrectRenderingEnabled(
        color_correct_rendering);
  }

  bool experimental_canvas_features;
  bool color_correct_rendering;
};

TEST_F(ImageDataTest, NegativeAndZeroIntSizeTest) {
  ImageData* image_data;

  image_data = ImageData::Create(IntSize(0, 10));
  EXPECT_EQ(image_data, nullptr);

  image_data = ImageData::Create(IntSize(10, 0));
  EXPECT_EQ(image_data, nullptr);

  image_data = ImageData::Create(IntSize(0, 0));
  EXPECT_EQ(image_data, nullptr);

  image_data = ImageData::Create(IntSize(-1, 10));
  EXPECT_EQ(image_data, nullptr);

  image_data = ImageData::Create(IntSize(10, -1));
  EXPECT_EQ(image_data, nullptr);

  image_data = ImageData::Create(IntSize(-1, -1));
  EXPECT_EQ(image_data, nullptr);
}

// Under asan_clang_phone, the test crashes after the memory allocation
// is not successful. It is probably related to the value of
// allocator_may_return_null on trybots, which in this case causes ASAN
// to terminate the process instead of returning null.
// crbug.com/704948
#if defined(ADDRESS_SANITIZER)
#define MAYBE_CreateImageDataTooBig DISABLED_CreateImageDataTooBig
#else
#define MAYBE_CreateImageDataTooBig CreateImageDataTooBig
#endif

// This test passes if it does not crash. If the required memory is not
// allocated to the ImageData, then an exception must raise.
TEST_F(ImageDataTest, MAYBE_CreateImageDataTooBig) {
  DummyExceptionStateForTesting exception_state;
  ImageData* too_big_image_data =
      ImageData::Create(32767, 32767, exception_state);
  if (!too_big_image_data) {
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.Code(), kV8RangeError);
  }
}

// Skia conversion does not guarantee to be exact, se we need to do approximate
// comparisons.
static inline bool IsNearlyTheSame(float f, float g) {
  static const float kEpsilonScale = 0.01f;
  return std::abs(f - g) <
         kEpsilonScale *
             std::max(std::max(std::abs(f), std::abs(g)), kEpsilonScale);
}

// This test verifies the correct behavior of ImageData member function used
// to convert pixels data from canvas pixel format to image data storage
// format. This function is used in BaseRenderingContext2D::getImageData.
TEST_F(ImageDataTest,
       TestConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat) {
  // Source pixels in RGBA32
  unsigned char rgba32_pixels[] = {255, 0,   0,   255,  // Red
                                   0,   0,   0,   0,    // Transparent
                                   255, 192, 128, 64,   // Decreasing values
                                   93,  117, 205, 11};  // Random values
  const unsigned kNumColorComponents = 16;
  // Source pixels in F16
  unsigned char f16_pixels[kNumColorComponents * 2];

  // Filling F16 source pixels
  std::unique_ptr<SkColorSpaceXform> xform =
      SkColorSpaceXform::New(SkColorSpace::MakeSRGBLinear().get(),
                             SkColorSpace::MakeSRGBLinear().get());
  xform->apply(SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat,
               f16_pixels,
               SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat,
               rgba32_pixels, 4, SkAlphaType::kUnpremul_SkAlphaType);

  // Creating ArrayBufferContents objects. We need two buffers for RGBA32 data
  // because kRGBA8CanvasPixelFormat->kUint8ClampedArrayStorageFormat consumes
  // the input data parameter.
  WTF::ArrayBufferContents contents_rgba32(
      kNumColorComponents, 1, WTF::ArrayBufferContents::kNotShared,
      WTF::ArrayBufferContents::kDontInitialize);
  std::memcpy(contents_rgba32.Data(), rgba32_pixels, kNumColorComponents);

  WTF::ArrayBufferContents contents2rgba32(
      kNumColorComponents, 1, WTF::ArrayBufferContents::kNotShared,
      WTF::ArrayBufferContents::kDontInitialize);
  std::memcpy(contents2rgba32.Data(), rgba32_pixels, kNumColorComponents);

  WTF::ArrayBufferContents contents_f16(
      kNumColorComponents * 2, 1, WTF::ArrayBufferContents::kNotShared,
      WTF::ArrayBufferContents::kDontInitialize);
  std::memcpy(contents_f16.Data(), f16_pixels, kNumColorComponents * 2);

  // Uint16 is not supported as the storage format for ImageData created from a
  // canvas, so this conversion is neither implemented nor tested here.
  bool test_passed = true;
  DOMArrayBufferView* data = nullptr;
  DOMUint8ClampedArray* data_u8 = nullptr;
  DOMFloat32Array* data_f32 = nullptr;

  // Testing kRGBA8CanvasPixelFormat -> kUint8ClampedArrayStorageFormat
  data = ImageData::ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      contents_rgba32, kRGBA8CanvasPixelFormat,
      kUint8ClampedArrayStorageFormat);
  DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeUint8Clamped);
  data_u8 = const_cast<DOMUint8ClampedArray*>(
      static_cast<const DOMUint8ClampedArray*>(data));
  DCHECK(data_u8);
  for (unsigned i = 0; i < kNumColorComponents; i++) {
    if (data_u8->Item(i) != rgba32_pixels[i]) {
      test_passed = false;
      break;
    }
  }
  EXPECT_TRUE(test_passed);

  // Testing kRGBA8CanvasPixelFormat -> kFloat32ArrayStorageFormat
  data = ImageData::ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      contents2rgba32, kRGBA8CanvasPixelFormat, kFloat32ArrayStorageFormat);
  DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeFloat32);
  data_f32 =
      const_cast<DOMFloat32Array*>(static_cast<const DOMFloat32Array*>(data));
  DCHECK(data_f32);
  for (unsigned i = 0; i < kNumColorComponents; i++) {
    if (!IsNearlyTheSame(data_f32->Item(i), rgba32_pixels[i] / 255.0)) {
      test_passed = false;
      break;
    }
  }
  EXPECT_TRUE(test_passed);

  // Testing kF16CanvasPixelFormat -> kUint8ClampedArrayStorageFormat
  data = ImageData::ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      contents_f16, kF16CanvasPixelFormat, kUint8ClampedArrayStorageFormat);
  DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeUint8Clamped);
  data_u8 = const_cast<DOMUint8ClampedArray*>(
      static_cast<const DOMUint8ClampedArray*>(data));
  DCHECK(data_u8);
  for (unsigned i = 0; i < kNumColorComponents; i++) {
    if (!IsNearlyTheSame(data_u8->Item(i), rgba32_pixels[i])) {
      test_passed = false;
      break;
    }
  }
  EXPECT_TRUE(test_passed);

  // Testing kF16CanvasPixelFormat -> kFloat32ArrayStorageFormat
  data = ImageData::ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      contents_f16, kF16CanvasPixelFormat, kFloat32ArrayStorageFormat);
  DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeFloat32);
  data_f32 =
      const_cast<DOMFloat32Array*>(static_cast<const DOMFloat32Array*>(data));
  DCHECK(data_f32);
  for (unsigned i = 0; i < kNumColorComponents; i++) {
    if (!IsNearlyTheSame(data_f32->Item(i), rgba32_pixels[i] / 255.0)) {
      test_passed = false;
      break;
    }
  }
  EXPECT_TRUE(test_passed);
}

bool ConvertPixelsToColorSpaceAndPixelFormatForTest(
    DOMArrayBufferView* data_array,
    CanvasColorSpace src_color_space,
    CanvasColorSpace dst_color_space,
    CanvasPixelFormat dst_pixel_format,
    std::unique_ptr<uint8_t[]>& converted_pixels) {
  // Setting SkColorSpaceXform::apply parameters
  SkColorSpaceXform::ColorFormat src_color_format =
      SkColorSpaceXform::kRGBA_8888_ColorFormat;

  unsigned data_length = data_array->byteLength() / data_array->TypeSize();
  unsigned num_pixels = data_length / 4;
  DOMUint8ClampedArray* u8_array = nullptr;
  DOMUint16Array* u16_array = nullptr;
  DOMFloat32Array* f32_array = nullptr;
  void* src_data = nullptr;

  switch (data_array->GetType()) {
    case ArrayBufferView::ViewType::kTypeUint8Clamped:
      u8_array = const_cast<DOMUint8ClampedArray*>(
          static_cast<const DOMUint8ClampedArray*>(data_array));
      src_data = static_cast<void*>(u8_array->Data());
      break;

    case ArrayBufferView::ViewType::kTypeUint16:
      u16_array = const_cast<DOMUint16Array*>(
          static_cast<const DOMUint16Array*>(data_array));
      src_color_format =
          SkColorSpaceXform::ColorFormat::kRGBA_U16_BE_ColorFormat;
      src_data = static_cast<void*>(u16_array->Data());
      break;

    case ArrayBufferView::ViewType::kTypeFloat32:
      f32_array = const_cast<DOMFloat32Array*>(
          static_cast<const DOMFloat32Array*>(data_array));
      src_color_format = SkColorSpaceXform::kRGBA_F32_ColorFormat;
      src_data = static_cast<void*>(f32_array->Data());
      break;
    default:
      NOTREACHED();
      return false;
  }

  SkColorSpaceXform::ColorFormat dst_color_format =
      SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
  if (dst_pixel_format == kF16CanvasPixelFormat)
    dst_color_format = SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat;

  sk_sp<SkColorSpace> src_sk_color_space = nullptr;
  if (u8_array) {
    src_sk_color_space = ImageData::GetSkColorSpaceForTest(
        src_color_space, kRGBA8CanvasPixelFormat);
  } else {
    src_sk_color_space = ImageData::GetSkColorSpaceForTest(
        src_color_space, kF16CanvasPixelFormat);
  }

  sk_sp<SkColorSpace> dst_sk_color_space =
      ImageData::GetSkColorSpaceForTest(dst_color_space, dst_pixel_format);

  // When the input dataArray is in Uint16, we normally should convert the
  // values from Little Endian to Big Endian before passing the buffer to
  // SkColorSpaceXform::apply. However, in this test scenario we are creating
  // the Uin16 dataArray by multiplying a Uint8Clamped array members by 257,
  // hence the Big Endian and Little Endian representations are the same.

  std::unique_ptr<SkColorSpaceXform> xform = SkColorSpaceXform::New(
      src_sk_color_space.get(), dst_sk_color_space.get());

  if (!xform->apply(dst_color_format, converted_pixels.get(), src_color_format,
                    src_data, num_pixels, kUnpremul_SkAlphaType))
    return false;
  return true;
}

// This test verifies the correct behavior of ImageData member function used
// to convert image data from image data storage format to canvas pixel format.
// This function is used in BaseRenderingContext2D::putImageData.
TEST_F(ImageDataTest, TestGetImageDataInCanvasColorSettings) {
  unsigned num_image_data_color_spaces = 3;
  CanvasColorSpace image_data_color_spaces[] = {
      kSRGBCanvasColorSpace, kRec2020CanvasColorSpace, kP3CanvasColorSpace,
  };

  unsigned num_image_data_storage_formats = 3;
  ImageDataStorageFormat image_data_storage_formats[] = {
      kUint8ClampedArrayStorageFormat, kUint16ArrayStorageFormat,
      kFloat32ArrayStorageFormat,
  };

  unsigned num_canvas_color_settings = 4;
  CanvasColorSpace canvas_color_spaces[] = {
      kSRGBCanvasColorSpace, kSRGBCanvasColorSpace, kRec2020CanvasColorSpace,
      kP3CanvasColorSpace,
  };
  CanvasPixelFormat canvas_pixel_formats[] = {
      kRGBA8CanvasPixelFormat, kF16CanvasPixelFormat, kF16CanvasPixelFormat,
      kF16CanvasPixelFormat,
  };

  // As cross checking the output of Skia color space covnersion API is not in
  // the scope of this unit test, we do turn-around tests here. To do so, we
  // create an ImageData with the selected color space and storage format,
  // convert it to the target canvas color space and pixel format by calling
  // ImageData::imageDataInCanvasColorSettings(), and then convert it back
  // to the source image data color space and Float32 storage format by calling
  // ImageData::convertPixelsFromCanvasPixelFormatToImageDataStorageFormat().
  // We expect to get the same image data as we started with.

  // Source pixels in RGBA32
  uint8_t u8_pixels[] = {255, 0,   0,   255,  // Red
                         0,   0,   0,   0,    // Transparent
                         255, 192, 128, 64,   // Decreasing values
                         93,  117, 205, 11};  // Random values
  unsigned data_length = 16;

  uint16_t* u16_pixels = new uint16_t[data_length];
  for (unsigned i = 0; i < data_length; i++)
    u16_pixels[i] = u8_pixels[i] * 257;

  float* f32_pixels = new float[data_length];
  for (unsigned i = 0; i < data_length; i++)
    f32_pixels[i] = u8_pixels[i] / 255.0;

  DOMArrayBufferView* data_array = nullptr;

  DOMUint8ClampedArray* data_u8 =
      DOMUint8ClampedArray::Create(u8_pixels, data_length);
  DCHECK(data_u8);
  EXPECT_EQ(data_length, data_u8->length());
  DOMUint16Array* data_u16 = DOMUint16Array::Create(u16_pixels, data_length);
  DCHECK(data_u16);
  EXPECT_EQ(data_length, data_u16->length());
  DOMFloat32Array* data_f32 = DOMFloat32Array::Create(f32_pixels, data_length);
  DCHECK(data_f32);
  EXPECT_EQ(data_length, data_f32->length());

  ImageData* image_data = nullptr;
  ImageDataColorSettings color_settings;

  // At most two bytes are needed for output per color component.
  std::unique_ptr<uint8_t[]> pixels_converted_manually(
      new uint8_t[data_length * 2]());
  std::unique_ptr<uint8_t[]> pixels_converted_in_image_data(
      new uint8_t[data_length * 2]());

  // Loop through different possible combinations of image data color space and
  // storage formats and create the respective test image data objects.
  for (unsigned i = 0; i < num_image_data_color_spaces; i++) {
    color_settings.setColorSpace(
        ImageData::CanvasColorSpaceName(image_data_color_spaces[i]));

    for (unsigned j = 0; j < num_image_data_storage_formats; j++) {
      switch (image_data_storage_formats[j]) {
        case kUint8ClampedArrayStorageFormat:
          data_array = static_cast<DOMArrayBufferView*>(data_u8);
          color_settings.setStorageFormat(kUint8ClampedArrayStorageFormatName);
          break;
        case kUint16ArrayStorageFormat:
          data_array = static_cast<DOMArrayBufferView*>(data_u16);
          color_settings.setStorageFormat(kUint16ArrayStorageFormatName);
          break;
        case kFloat32ArrayStorageFormat:
          data_array = static_cast<DOMArrayBufferView*>(data_f32);
          color_settings.setStorageFormat(kFloat32ArrayStorageFormatName);
          break;
        default:
          NOTREACHED();
      }

      image_data =
          ImageData::CreateForTest(IntSize(2, 2), data_array, &color_settings);

      for (unsigned k = 0; k < num_canvas_color_settings; k++) {
        unsigned output_length =
            (canvas_pixel_formats[k] == kRGBA8CanvasPixelFormat)
                ? data_length
                : data_length * 2;
        // Convert the original data used to create ImageData to the
        // canvas color space and canvas pixel format.
        EXPECT_TRUE(ConvertPixelsToColorSpaceAndPixelFormatForTest(
            data_array, image_data_color_spaces[i], canvas_color_spaces[k],
            canvas_pixel_formats[k], pixels_converted_manually));

        // Convert the image data to the color settings of the canvas.
        EXPECT_TRUE(image_data->ImageDataInCanvasColorSettings(
            canvas_color_spaces[k], canvas_pixel_formats[k],
            pixels_converted_in_image_data));

        // Compare the converted pixels
        EXPECT_EQ(0,
                  memcmp(pixels_converted_manually.get(),
                         pixels_converted_in_image_data.get(), output_length));
      }
    }
  }
  delete[] u16_pixels;
  delete[] f32_pixels;
}

}  // namspace
}  // namespace blink
