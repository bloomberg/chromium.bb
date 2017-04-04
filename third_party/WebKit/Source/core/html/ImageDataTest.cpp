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
    experimentalCanvasFeatures =
        RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled();
    colorCorrectRendering =
        RuntimeEnabledFeatures::colorCorrectRenderingEnabled();
    RuntimeEnabledFeatures::setExperimentalCanvasFeaturesEnabled(true);
    RuntimeEnabledFeatures::setColorCorrectRenderingEnabled(true);
  }

  virtual void TearDown() {
    RuntimeEnabledFeatures::setExperimentalCanvasFeaturesEnabled(
        experimentalCanvasFeatures);
    RuntimeEnabledFeatures::setColorCorrectRenderingEnabled(
        colorCorrectRendering);
  }

  bool experimentalCanvasFeatures;
  bool colorCorrectRendering;
};

TEST_F(ImageDataTest, NegativeAndZeroIntSizeTest) {
  ImageData* imageData;

  imageData = ImageData::create(IntSize(0, 10));
  EXPECT_EQ(imageData, nullptr);

  imageData = ImageData::create(IntSize(10, 0));
  EXPECT_EQ(imageData, nullptr);

  imageData = ImageData::create(IntSize(0, 0));
  EXPECT_EQ(imageData, nullptr);

  imageData = ImageData::create(IntSize(-1, 10));
  EXPECT_EQ(imageData, nullptr);

  imageData = ImageData::create(IntSize(10, -1));
  EXPECT_EQ(imageData, nullptr);

  imageData = ImageData::create(IntSize(-1, -1));
  EXPECT_EQ(imageData, nullptr);
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
  DummyExceptionStateForTesting exceptionState;
  ImageData* tooBigImageData = ImageData::create(32767, 32767, exceptionState);
  if (!tooBigImageData) {
    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_EQ(exceptionState.code(), V8RangeError);
  }
}

// Skia conversion does not guarantee to be exact, se we need to do approximate
// comparisons.
static inline bool IsNearlyTheSame(float f, float g) {
  static const float epsilonScale = 0.01f;
  return std::abs(f - g) <
         epsilonScale *
             std::max(std::max(std::abs(f), std::abs(g)), epsilonScale);
}

// This test verifies the correct behavior of ImageData member function used
// to convert pixels data from canvas pixel format to image data storage
// format. This function is used in BaseRenderingContext2D::getImageData.
TEST_F(ImageDataTest,
       TestConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat) {
  // Source pixels in RGBA32
  unsigned char rgba32Pixels[] = {255, 0,   0,   255,  // Red
                                  0,   0,   0,   0,    // Transparent
                                  255, 192, 128, 64,   // Decreasing values
                                  93,  117, 205, 11};  // Random values
  const unsigned numColorComponents = 16;
  // Source pixels in F16
  unsigned char f16Pixels[numColorComponents * 2];

  // Filling F16 source pixels
  std::unique_ptr<SkColorSpaceXform> xform =
      SkColorSpaceXform::New(SkColorSpace::MakeSRGBLinear().get(),
                             SkColorSpace::MakeSRGBLinear().get());
  xform->apply(SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat, f16Pixels,
               SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat,
               rgba32Pixels, 4, SkAlphaType::kUnpremul_SkAlphaType);

  // Creating ArrayBufferContents objects. We need two buffers for RGBA32 data
  // because kRGBA8CanvasPixelFormat->kUint8ClampedArrayStorageFormat consumes
  // the input data parameter.
  WTF::ArrayBufferContents contentsRGBA32(
      numColorComponents, 1, WTF::ArrayBufferContents::NotShared,
      WTF::ArrayBufferContents::DontInitialize);
  std::memcpy(contentsRGBA32.data(), rgba32Pixels, numColorComponents);

  WTF::ArrayBufferContents contents2RGBA32(
      numColorComponents, 1, WTF::ArrayBufferContents::NotShared,
      WTF::ArrayBufferContents::DontInitialize);
  std::memcpy(contents2RGBA32.data(), rgba32Pixels, numColorComponents);

  WTF::ArrayBufferContents contentsF16(
      numColorComponents * 2, 1, WTF::ArrayBufferContents::NotShared,
      WTF::ArrayBufferContents::DontInitialize);
  std::memcpy(contentsF16.data(), f16Pixels, numColorComponents * 2);

  // Uint16 is not supported as the storage format for ImageData created from a
  // canvas, so this conversion is neither implemented nor tested here.
  bool testPassed = true;
  DOMArrayBufferView* data = nullptr;
  DOMUint8ClampedArray* dataU8 = nullptr;
  DOMFloat32Array* dataF32 = nullptr;

  // Testing kRGBA8CanvasPixelFormat -> kUint8ClampedArrayStorageFormat
  data = ImageData::convertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      contentsRGBA32, kRGBA8CanvasPixelFormat, kUint8ClampedArrayStorageFormat);
  DCHECK(data->type() == DOMArrayBufferView::ViewType::TypeUint8Clamped);
  dataU8 = const_cast<DOMUint8ClampedArray*>(
      static_cast<const DOMUint8ClampedArray*>(data));
  DCHECK(dataU8);
  for (unsigned i = 0; i < numColorComponents; i++) {
    if (dataU8->item(i) != rgba32Pixels[i]) {
      testPassed = false;
      break;
    }
  }
  EXPECT_TRUE(testPassed);

  // Testing kRGBA8CanvasPixelFormat -> kFloat32ArrayStorageFormat
  data = ImageData::convertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      contents2RGBA32, kRGBA8CanvasPixelFormat, kFloat32ArrayStorageFormat);
  DCHECK(data->type() == DOMArrayBufferView::ViewType::TypeFloat32);
  dataF32 =
      const_cast<DOMFloat32Array*>(static_cast<const DOMFloat32Array*>(data));
  DCHECK(dataF32);
  for (unsigned i = 0; i < numColorComponents; i++) {
    if (!IsNearlyTheSame(dataF32->item(i), rgba32Pixels[i] / 255.0)) {
      testPassed = false;
      break;
    }
  }
  EXPECT_TRUE(testPassed);

  // Testing kF16CanvasPixelFormat -> kUint8ClampedArrayStorageFormat
  data = ImageData::convertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      contentsF16, kF16CanvasPixelFormat, kUint8ClampedArrayStorageFormat);
  DCHECK(data->type() == DOMArrayBufferView::ViewType::TypeUint8Clamped);
  dataU8 = const_cast<DOMUint8ClampedArray*>(
      static_cast<const DOMUint8ClampedArray*>(data));
  DCHECK(dataU8);
  for (unsigned i = 0; i < numColorComponents; i++) {
    if (!IsNearlyTheSame(dataU8->item(i), rgba32Pixels[i])) {
      testPassed = false;
      break;
    }
  }
  EXPECT_TRUE(testPassed);

  // Testing kF16CanvasPixelFormat -> kFloat32ArrayStorageFormat
  data = ImageData::convertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      contentsF16, kF16CanvasPixelFormat, kFloat32ArrayStorageFormat);
  DCHECK(data->type() == DOMArrayBufferView::ViewType::TypeFloat32);
  dataF32 =
      const_cast<DOMFloat32Array*>(static_cast<const DOMFloat32Array*>(data));
  DCHECK(dataF32);
  for (unsigned i = 0; i < numColorComponents; i++) {
    if (!IsNearlyTheSame(dataF32->item(i), rgba32Pixels[i] / 255.0)) {
      testPassed = false;
      break;
    }
  }
  EXPECT_TRUE(testPassed);
}

bool convertPixelsToColorSpaceAndPixelFormatForTest(
    DOMArrayBufferView* dataArray,
    CanvasColorSpace srcColorSpace,
    CanvasColorSpace dstColorSpace,
    CanvasPixelFormat dstPixelFormat,
    std::unique_ptr<uint8_t[]>& convertedPixels) {
  // Setting SkColorSpaceXform::apply parameters
  SkColorSpaceXform::ColorFormat srcColorFormat =
      SkColorSpaceXform::kRGBA_8888_ColorFormat;

  unsigned dataLength = dataArray->byteLength() / dataArray->typeSize();
  unsigned numPixels = dataLength / 4;
  DOMUint8ClampedArray* u8Array = nullptr;
  DOMUint16Array* u16Array = nullptr;
  DOMFloat32Array* f32Array = nullptr;
  void* srcData = nullptr;

  switch (dataArray->type()) {
    case ArrayBufferView::ViewType::TypeUint8Clamped:
      u8Array = const_cast<DOMUint8ClampedArray*>(
          static_cast<const DOMUint8ClampedArray*>(dataArray));
      srcData = static_cast<void*>(u8Array->data());
      break;

    case ArrayBufferView::ViewType::TypeUint16:
      u16Array = const_cast<DOMUint16Array*>(
          static_cast<const DOMUint16Array*>(dataArray));
      srcColorFormat = SkColorSpaceXform::ColorFormat::kRGBA_U16_BE_ColorFormat;
      srcData = static_cast<void*>(u16Array->data());
      break;

    case ArrayBufferView::ViewType::TypeFloat32:
      f32Array = const_cast<DOMFloat32Array*>(
          static_cast<const DOMFloat32Array*>(dataArray));
      srcColorFormat = SkColorSpaceXform::kRGBA_F32_ColorFormat;
      srcData = static_cast<void*>(f32Array->data());
      break;
    default:
      NOTREACHED();
      return false;
  }

  SkColorSpaceXform::ColorFormat dstColorFormat =
      SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
  if (dstPixelFormat == kF16CanvasPixelFormat)
    dstColorFormat = SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat;

  sk_sp<SkColorSpace> srcSkColorSpace = nullptr;
  if (u8Array) {
    srcSkColorSpace = ImageData::getSkColorSpaceForTest(
        srcColorSpace, kRGBA8CanvasPixelFormat);
  } else {
    srcSkColorSpace =
        ImageData::getSkColorSpaceForTest(srcColorSpace, kF16CanvasPixelFormat);
  }

  sk_sp<SkColorSpace> dstSkColorSpace =
      ImageData::getSkColorSpaceForTest(dstColorSpace, dstPixelFormat);

  // When the input dataArray is in Uint16, we normally should convert the
  // values from Little Endian to Big Endian before passing the buffer to
  // SkColorSpaceXform::apply. However, in this test scenario we are creating
  // the Uin16 dataArray by multiplying a Uint8Clamped array members by 257,
  // hence the Big Endian and Little Endian representations are the same.

  std::unique_ptr<SkColorSpaceXform> xform =
      SkColorSpaceXform::New(srcSkColorSpace.get(), dstSkColorSpace.get());

  if (!xform->apply(dstColorFormat, convertedPixels.get(), srcColorFormat,
                    srcData, numPixels, kUnpremul_SkAlphaType))
    return false;
  return true;
}

// This test verifies the correct behavior of ImageData member function used
// to convert image data from image data storage format to canvas pixel format.
// This function is used in BaseRenderingContext2D::putImageData.
TEST_F(ImageDataTest, TestGetImageDataInCanvasColorSettings) {
  unsigned numImageDataColorSpaces = 3;
  CanvasColorSpace imageDataColorSpaces[] = {
      kSRGBCanvasColorSpace, kRec2020CanvasColorSpace, kP3CanvasColorSpace,
  };

  unsigned numImageDataStorageFormats = 3;
  ImageDataStorageFormat imageDataStorageFormats[] = {
      kUint8ClampedArrayStorageFormat, kUint16ArrayStorageFormat,
      kFloat32ArrayStorageFormat,
  };

  unsigned numCanvasColorSettings = 4;
  CanvasColorSpace canvasColorSpaces[] = {
      kSRGBCanvasColorSpace, kSRGBCanvasColorSpace, kRec2020CanvasColorSpace,
      kP3CanvasColorSpace,
  };
  CanvasPixelFormat canvasPixelFormats[] = {
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
  uint8_t u8Pixels[] = {255, 0,   0,   255,  // Red
                        0,   0,   0,   0,    // Transparent
                        255, 192, 128, 64,   // Decreasing values
                        93,  117, 205, 11};  // Random values
  unsigned dataLength = 16;

  uint16_t* u16Pixels = new uint16_t[dataLength];
  for (unsigned i = 0; i < dataLength; i++)
    u16Pixels[i] = u8Pixels[i] * 257;

  float* f32Pixels = new float[dataLength];
  for (unsigned i = 0; i < dataLength; i++)
    f32Pixels[i] = u8Pixels[i] / 255.0;

  DOMArrayBufferView* dataArray = nullptr;

  DOMUint8ClampedArray* dataU8 =
      DOMUint8ClampedArray::create(u8Pixels, dataLength);
  DCHECK(dataU8);
  EXPECT_EQ(dataLength, dataU8->length());
  DOMUint16Array* dataU16 = DOMUint16Array::create(u16Pixels, dataLength);
  DCHECK(dataU16);
  EXPECT_EQ(dataLength, dataU16->length());
  DOMFloat32Array* dataF32 = DOMFloat32Array::create(f32Pixels, dataLength);
  DCHECK(dataF32);
  EXPECT_EQ(dataLength, dataF32->length());

  ImageData* imageData = nullptr;
  ImageDataColorSettings colorSettings;

  // At most two bytes are needed for output per color component.
  std::unique_ptr<uint8_t[]> pixelsConvertedManually(
      new uint8_t[dataLength * 2]());
  std::unique_ptr<uint8_t[]> pixelsConvertedInImageData(
      new uint8_t[dataLength * 2]());

  // Loop through different possible combinations of image data color space and
  // storage formats and create the respective test image data objects.
  for (unsigned i = 0; i < numImageDataColorSpaces; i++) {
    colorSettings.setColorSpace(
        ImageData::canvasColorSpaceName(imageDataColorSpaces[i]));

    for (unsigned j = 0; j < numImageDataStorageFormats; j++) {
      switch (imageDataStorageFormats[j]) {
        case kUint8ClampedArrayStorageFormat:
          dataArray = static_cast<DOMArrayBufferView*>(dataU8);
          colorSettings.setStorageFormat(kUint8ClampedArrayStorageFormatName);
          break;
        case kUint16ArrayStorageFormat:
          dataArray = static_cast<DOMArrayBufferView*>(dataU16);
          colorSettings.setStorageFormat(kUint16ArrayStorageFormatName);
          break;
        case kFloat32ArrayStorageFormat:
          dataArray = static_cast<DOMArrayBufferView*>(dataF32);
          colorSettings.setStorageFormat(kFloat32ArrayStorageFormatName);
          break;
        default:
          NOTREACHED();
      }

      imageData =
          ImageData::createForTest(IntSize(2, 2), dataArray, &colorSettings);

      for (unsigned k = 0; k < numCanvasColorSettings; k++) {
        unsigned outputLength =
            (canvasPixelFormats[k] == kRGBA8CanvasPixelFormat) ? dataLength
                                                               : dataLength * 2;
        // Convert the original data used to create ImageData to the
        // canvas color space and canvas pixel format.
        EXPECT_TRUE(convertPixelsToColorSpaceAndPixelFormatForTest(
            dataArray, imageDataColorSpaces[i], canvasColorSpaces[k],
            canvasPixelFormats[k], pixelsConvertedManually));

        // Convert the image data to the color settings of the canvas.
        EXPECT_TRUE(imageData->imageDataInCanvasColorSettings(
            canvasColorSpaces[k], canvasPixelFormats[k],
            pixelsConvertedInImageData));

        // Compare the converted pixels
        EXPECT_EQ(0, memcmp(pixelsConvertedManually.get(),
                            pixelsConvertedInImageData.get(), outputLength));
      }
    }
  }
  delete[] u16Pixels;
  delete[] f32Pixels;
}

}  // namspace
}  // namespace blink
