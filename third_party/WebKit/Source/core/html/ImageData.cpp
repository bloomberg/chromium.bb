/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/ImageData.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Uint8ClampedArray.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/ImageBitmap.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/ColorBehavior.h"
#include "third_party/skia/include/core/SkColorSpaceXform.h"

namespace blink {

bool RaiseDOMExceptionAndReturnFalse(ExceptionState* exceptionState,
                                     ExceptionCode exceptionCode,
                                     const char* message) {
  if (exceptionState)
    exceptionState->throwDOMException(exceptionCode, message);
  return false;
}

bool ImageData::validateConstructorArguments(
    const unsigned& paramFlags,
    const IntSize* size,
    const unsigned& width,
    const unsigned& height,
    const DOMArrayBufferView* data,
    const ImageDataColorSettings* colorSettings,
    ExceptionState* exceptionState) {
  // We accept all the combinations of colorSpace and storageFormat in an
  // ImageDataColorSettings to be stored in an ImageData. Therefore, we don't
  // check the color settings in this function.

  if ((paramFlags & kParamWidth) && !width) {
    return RaiseDOMExceptionAndReturnFalse(
        exceptionState, IndexSizeError,
        "The source width is zero or not a number.");
  }

  if ((paramFlags & kParamHeight) && !height) {
    return RaiseDOMExceptionAndReturnFalse(
        exceptionState, IndexSizeError,
        "The source height is zero or not a number.");
  }

  if (paramFlags & (kParamWidth | kParamHeight)) {
    CheckedNumeric<unsigned> dataSize = 4;
    if (colorSettings) {
      dataSize *=
          ImageData::storageFormatDataSize(colorSettings->storageFormat());
    }
    dataSize *= width;
    dataSize *= height;
    if (!dataSize.IsValid())
      return RaiseDOMExceptionAndReturnFalse(
          exceptionState, IndexSizeError,
          "The requested image size exceeds the supported range.");
  }

  unsigned dataLength = 0;
  if (paramFlags & kParamData) {
    DCHECK(data);
    if (data->type() != DOMArrayBufferView::ViewType::TypeUint8Clamped &&
        data->type() != DOMArrayBufferView::ViewType::TypeUint16 &&
        data->type() != DOMArrayBufferView::ViewType::TypeFloat32) {
      return RaiseDOMExceptionAndReturnFalse(
          exceptionState, NotSupportedError,
          "The input data type is not supported.");
    }

    if (!data->byteLength()) {
      return RaiseDOMExceptionAndReturnFalse(
          exceptionState, IndexSizeError, "The input data has zero elements.");
    }

    dataLength = data->byteLength() / data->typeSize();
    if (dataLength % 4) {
      return RaiseDOMExceptionAndReturnFalse(
          exceptionState, IndexSizeError,
          "The input data length is not a multiple of 4.");
    }

    if ((paramFlags & kParamWidth) && (dataLength / 4) % width) {
      return RaiseDOMExceptionAndReturnFalse(
          exceptionState, IndexSizeError,
          "The input data length is not a multiple of (4 * width).");
    }

    if ((paramFlags & kParamWidth) && (paramFlags & kParamHeight) &&
        height != dataLength / (4 * width))
      return RaiseDOMExceptionAndReturnFalse(
          exceptionState, IndexSizeError,
          "The input data length is not equal to (4 * width * height).");
  }

  if (paramFlags & kParamSize) {
    if (size->width() <= 0 || size->height() <= 0)
      return false;
    CheckedNumeric<unsigned> dataSize = 4;
    dataSize *= size->width();
    dataSize *= size->height();
    if (!dataSize.IsValid())
      return false;
    if (paramFlags & kParamData) {
      if (dataSize.ValueOrDie() > dataLength)
        return false;
    }
  }

  return true;
}

DOMArrayBufferView* ImageData::allocateAndValidateDataArray(
    const unsigned& length,
    ImageDataStorageFormat storageFormat,
    ExceptionState* exceptionState) {
  if (!length)
    return nullptr;

  DOMArrayBufferView* dataArray = nullptr;
  switch (storageFormat) {
    case kUint8ClampedArrayStorageFormat:
      dataArray = DOMUint8ClampedArray::createOrNull(length);
      break;
    case kUint16ArrayStorageFormat:
      dataArray = DOMUint16Array::createOrNull(length);
      break;
    case kFloat32ArrayStorageFormat:
      dataArray = DOMFloat32Array::createOrNull(length);
      break;
    default:
      NOTREACHED();
  }

  if (!dataArray || length != dataArray->byteLength() / dataArray->typeSize()) {
    if (exceptionState)
      exceptionState->throwDOMException(V8RangeError,
                                        "Out of memory at ImageData creation");
    return nullptr;
  }

  return dataArray;
}

DOMUint8ClampedArray* ImageData::allocateAndValidateUint8ClampedArray(
    const unsigned& length,
    ExceptionState* exceptionState) {
  DOMArrayBufferView* bufferView = allocateAndValidateDataArray(
      length, kUint8ClampedArrayStorageFormat, exceptionState);
  if (!bufferView)
    return nullptr;
  DOMUint8ClampedArray* u8Array = const_cast<DOMUint8ClampedArray*>(
      static_cast<const DOMUint8ClampedArray*>(bufferView));
  DCHECK(u8Array);
  return u8Array;
}

DOMUint16Array* ImageData::allocateAndValidateUint16Array(
    const unsigned& length,
    ExceptionState* exceptionState) {
  DOMArrayBufferView* bufferView = allocateAndValidateDataArray(
      length, kUint16ArrayStorageFormat, exceptionState);
  if (!bufferView)
    return nullptr;
  DOMUint16Array* u16Array = const_cast<DOMUint16Array*>(
      static_cast<const DOMUint16Array*>(bufferView));
  DCHECK(u16Array);
  return u16Array;
}

DOMFloat32Array* ImageData::allocateAndValidateFloat32Array(
    const unsigned& length,
    ExceptionState* exceptionState) {
  DOMArrayBufferView* bufferView = allocateAndValidateDataArray(
      length, kFloat32ArrayStorageFormat, exceptionState);
  if (!bufferView)
    return nullptr;
  DOMFloat32Array* f32Array = const_cast<DOMFloat32Array*>(
      static_cast<const DOMFloat32Array*>(bufferView));
  DCHECK(f32Array);
  return f32Array;
}

ImageData* ImageData::create(const IntSize& size,
                             const ImageDataColorSettings* colorSettings) {
  if (!ImageData::validateConstructorArguments(kParamSize, &size, 0, 0, nullptr,
                                               colorSettings))
    return nullptr;
  ImageDataStorageFormat storageFormat = kUint8ClampedArrayStorageFormat;
  if (colorSettings) {
    storageFormat =
        ImageData::imageDataStorageFormat(colorSettings->storageFormat());
  }
  DOMArrayBufferView* dataArray =
      allocateAndValidateDataArray(4 * static_cast<unsigned>(size.width()) *
                                       static_cast<unsigned>(size.height()),
                                   storageFormat);
  return dataArray ? new ImageData(size, dataArray, colorSettings) : nullptr;
}

ImageData* ImageData::create(const IntSize& size,
                             DOMArrayBufferView* dataArray,
                             const ImageDataColorSettings* colorSettings) {
  if (!ImageData::validateConstructorArguments(kParamSize | kParamData, &size,
                                               0, 0, dataArray, colorSettings))
    return nullptr;
  return new ImageData(size, dataArray, colorSettings);
}

ImageData* ImageData::create(unsigned width,
                             unsigned height,
                             ExceptionState& exceptionState) {
  if (!ImageData::validateConstructorArguments(kParamWidth | kParamHeight,
                                               nullptr, width, height, nullptr,
                                               nullptr, &exceptionState))
    return nullptr;

  DOMArrayBufferView* byteArray = allocateAndValidateDataArray(
      4 * width * height, kUint8ClampedArrayStorageFormat, &exceptionState);
  return byteArray ? new ImageData(IntSize(width, height), byteArray) : nullptr;
}

ImageData* ImageData::create(DOMUint8ClampedArray* data,
                             unsigned width,
                             ExceptionState& exceptionState) {
  if (!ImageData::validateConstructorArguments(kParamData | kParamWidth,
                                               nullptr, width, 0, data, nullptr,
                                               &exceptionState))
    return nullptr;

  unsigned height = data->length() / (width * 4);
  return new ImageData(IntSize(width, height), data);
}

ImageData* ImageData::create(DOMUint8ClampedArray* data,
                             unsigned width,
                             unsigned height,
                             ExceptionState& exceptionState) {
  if (!ImageData::validateConstructorArguments(
          kParamData | kParamWidth | kParamHeight, nullptr, width, height, data,
          nullptr, &exceptionState))
    return nullptr;

  return new ImageData(IntSize(width, height), data);
}

ImageData* ImageData::createImageData(
    unsigned width,
    unsigned height,
    const ImageDataColorSettings& colorSettings,
    ExceptionState& exceptionState) {
  if (!ImageData::validateConstructorArguments(kParamWidth | kParamHeight,
                                               nullptr, width, height, nullptr,
                                               &colorSettings, &exceptionState))
    return nullptr;

  ImageDataStorageFormat storageFormat =
      ImageData::imageDataStorageFormat(colorSettings.storageFormat());
  DOMArrayBufferView* bufferView = allocateAndValidateDataArray(
      4 * width * height, storageFormat, &exceptionState);

  if (!bufferView)
    return nullptr;

  return new ImageData(IntSize(width, height), bufferView, &colorSettings);
}

ImageData* ImageData::createImageData(ImageDataArray& data,
                                      unsigned width,
                                      unsigned height,
                                      ImageDataColorSettings& colorSettings,
                                      ExceptionState& exceptionState) {
  DOMArrayBufferView* bufferView = nullptr;
  // When pixels data is provided, we need to override the storage format of
  // ImageDataColorSettings with the one that matches the data type of the
  // pixels.
  String storageFormatName;

  if (data.isUint8ClampedArray()) {
    bufferView = data.getAsUint8ClampedArray();
    storageFormatName = kUint8ClampedArrayStorageFormatName;
  } else if (data.isUint16Array()) {
    bufferView = data.getAsUint16Array();
    storageFormatName = kUint16ArrayStorageFormatName;
  } else if (data.isFloat32Array()) {
    bufferView = data.getAsFloat32Array();
    storageFormatName = kFloat32ArrayStorageFormatName;
  } else {
    NOTREACHED();
  }

  if (storageFormatName != colorSettings.storageFormat())
    colorSettings.setStorageFormat(storageFormatName);

  if (!ImageData::validateConstructorArguments(
          kParamData | kParamWidth | kParamHeight, nullptr, width, height,
          bufferView, &colorSettings, &exceptionState))
    return nullptr;

  return new ImageData(IntSize(width, height), bufferView, &colorSettings);
}

// This function accepts size (0, 0) and always returns the ImageData in
// "srgb" color space and "uint8" storage format.
ImageData* ImageData::createForTest(const IntSize& size) {
  CheckedNumeric<unsigned> dataSize = 4;
  dataSize *= size.width();
  dataSize *= size.height();
  if (!dataSize.IsValid())
    return nullptr;

  DOMUint8ClampedArray* byteArray =
      DOMUint8ClampedArray::createOrNull(dataSize.ValueOrDie());
  if (!byteArray)
    return nullptr;

  return new ImageData(size, byteArray);
}

// This function is called from unit tests, and all the parameters are supposed
// to be validated on the call site.
ImageData* ImageData::createForTest(
    const IntSize& size,
    DOMArrayBufferView* bufferView,
    const ImageDataColorSettings* colorSettings) {
  return new ImageData(size, bufferView, colorSettings);
}

ScriptPromise ImageData::createImageBitmap(ScriptState* scriptState,
                                           EventTarget& eventTarget,
                                           Optional<IntRect> cropRect,
                                           const ImageBitmapOptions& options,
                                           ExceptionState& exceptionState) {
  if ((cropRect &&
       !ImageBitmap::isSourceSizeValid(cropRect->width(), cropRect->height(),
                                       exceptionState)) ||
      !ImageBitmap::isSourceSizeValid(bitmapSourceSize().width(),
                                      bitmapSourceSize().height(),
                                      exceptionState))
    return ScriptPromise();
  if (data()->bufferBase()->isNeutered()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "The source data has been neutered.");
    return ScriptPromise();
  }
  if (!ImageBitmap::isResizeOptionValid(options, exceptionState))
    return ScriptPromise();
  return ImageBitmapSource::fulfillImageBitmap(
      scriptState, ImageBitmap::create(this, cropRect, options));
}

v8::Local<v8::Object> ImageData::associateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapperType,
    v8::Local<v8::Object> wrapper) {
  wrapper =
      ScriptWrappable::associateWithWrapper(isolate, wrapperType, wrapper);

  if (!wrapper.IsEmpty() && m_data) {
    // Create a V8 Uint8ClampedArray object and set the "data" property
    // of the ImageData object to the created v8 object, eliminating the
    // C++ callback when accessing the "data" property.
    v8::Local<v8::Value> pixelArray = ToV8(m_data.get(), wrapper, isolate);
    if (pixelArray.IsEmpty() ||
        !v8CallBoolean(wrapper->DefineOwnProperty(
            isolate->GetCurrentContext(), v8AtomicString(isolate, "data"),
            pixelArray, v8::ReadOnly)))
      return v8::Local<v8::Object>();
  }
  return wrapper;
}

const DOMUint8ClampedArray* ImageData::data() const {
  if (m_colorSettings.storageFormat() == kUint8ClampedArrayStorageFormatName)
    return m_data.get();
  return nullptr;
}

DOMUint8ClampedArray* ImageData::data() {
  if (m_colorSettings.storageFormat() == kUint8ClampedArrayStorageFormatName)
    return m_data.get();
  return nullptr;
}

CanvasColorSpace ImageData::canvasColorSpace(const String& colorSpaceName) {
  if (colorSpaceName == kLegacyCanvasColorSpaceName)
    return kLegacyCanvasColorSpace;
  if (colorSpaceName == kSRGBCanvasColorSpaceName)
    return kSRGBCanvasColorSpace;
  if (colorSpaceName == kRec2020CanvasColorSpaceName)
    return kRec2020CanvasColorSpace;
  if (colorSpaceName == kP3CanvasColorSpaceName)
    return kP3CanvasColorSpace;
  NOTREACHED();
  return kSRGBCanvasColorSpace;
}

String ImageData::canvasColorSpaceName(const CanvasColorSpace& colorSpace) {
  switch (colorSpace) {
    case kSRGBCanvasColorSpace:
      return kSRGBCanvasColorSpaceName;
    case kRec2020CanvasColorSpace:
      return kRec2020CanvasColorSpaceName;
    case kP3CanvasColorSpace:
      return kP3CanvasColorSpaceName;
    default:
      NOTREACHED();
  }
  return kSRGBCanvasColorSpaceName;
}

ImageDataStorageFormat ImageData::imageDataStorageFormat(
    const String& storageFormatName) {
  if (storageFormatName == kUint8ClampedArrayStorageFormatName)
    return kUint8ClampedArrayStorageFormat;
  if (storageFormatName == kUint16ArrayStorageFormatName)
    return kUint16ArrayStorageFormat;
  if (storageFormatName == kFloat32ArrayStorageFormatName)
    return kFloat32ArrayStorageFormat;
  NOTREACHED();
  return kUint8ClampedArrayStorageFormat;
}

unsigned ImageData::storageFormatDataSize(const String& storageFormatName) {
  if (storageFormatName == kUint8ClampedArrayStorageFormatName)
    return 1;
  if (storageFormatName == kUint16ArrayStorageFormatName)
    return 2;
  if (storageFormatName == kFloat32ArrayStorageFormatName)
    return 4;
  NOTREACHED();
  return 1;
}

DOMFloat32Array* ImageData::convertFloat16ArrayToFloat32Array(
    const uint16_t* f16Array,
    unsigned arrayLength) {
  if (!f16Array || arrayLength <= 0)
    return nullptr;

  DOMFloat32Array* f32Array = allocateAndValidateFloat32Array(arrayLength);
  if (!f32Array)
    return nullptr;

  std::unique_ptr<SkColorSpaceXform> xform =
      SkColorSpaceXform::New(SkColorSpace::MakeSRGBLinear().get(),
                             SkColorSpace::MakeSRGBLinear().get());
  xform->apply(SkColorSpaceXform::ColorFormat::kRGBA_F32_ColorFormat,
               f32Array->data(),
               SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat, f16Array,
               arrayLength, SkAlphaType::kUnpremul_SkAlphaType);
  return f32Array;
}

DOMArrayBufferView*
ImageData::convertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
    WTF::ArrayBufferContents& content,
    CanvasPixelFormat pixelFormat,
    ImageDataStorageFormat storageFormat) {
  if (!content.sizeInBytes())
    return nullptr;

  // Uint16 is not supported as the storage format for ImageData created from a
  // canvas
  if (storageFormat == kUint16ArrayStorageFormat)
    return nullptr;

  unsigned numPixels = 0;
  DOMArrayBuffer* arrayBuffer = nullptr;
  DOMUint8ClampedArray* u8Array = nullptr;
  DOMFloat32Array* f32Array = nullptr;

  SkColorSpaceXform::ColorFormat srcColorFormat =
      SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
  SkColorSpaceXform::ColorFormat dstColorFormat =
      SkColorSpaceXform::ColorFormat::kRGBA_F32_ColorFormat;
  std::unique_ptr<SkColorSpaceXform> xform =
      SkColorSpaceXform::New(SkColorSpace::MakeSRGBLinear().get(),
                             SkColorSpace::MakeSRGBLinear().get());

  // To speed up the conversion process, we use SkColorSpaceXform::apply()
  // wherever appropriate.
  switch (pixelFormat) {
    case kRGBA8CanvasPixelFormat:
      numPixels = content.sizeInBytes() / 4;
      switch (storageFormat) {
        case kUint8ClampedArrayStorageFormat:
          arrayBuffer = DOMArrayBuffer::create(content);
          return DOMUint8ClampedArray::create(arrayBuffer, 0,
                                              arrayBuffer->byteLength());
          break;
        case kFloat32ArrayStorageFormat:
          f32Array = allocateAndValidateFloat32Array(numPixels * 4);
          if (!f32Array)
            return nullptr;
          xform->apply(dstColorFormat, f32Array->data(), srcColorFormat,
                       content.data(), numPixels,
                       SkAlphaType::kUnpremul_SkAlphaType);
          return f32Array;
          break;
        default:
          NOTREACHED();
      }
      break;

    case kF16CanvasPixelFormat:
      numPixels = content.sizeInBytes() / 8;
      srcColorFormat = SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat;

      switch (storageFormat) {
        case kUint8ClampedArrayStorageFormat:
          u8Array = allocateAndValidateUint8ClampedArray(numPixels * 4);
          if (!u8Array)
            return nullptr;
          dstColorFormat =
              SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
          xform->apply(dstColorFormat, u8Array->data(), srcColorFormat,
                       content.data(), numPixels,
                       SkAlphaType::kUnpremul_SkAlphaType);
          return u8Array;
          break;
        case kFloat32ArrayStorageFormat:
          f32Array = allocateAndValidateFloat32Array(numPixels * 4);
          if (!f32Array)
            return nullptr;
          dstColorFormat =
              SkColorSpaceXform::ColorFormat::kRGBA_F32_ColorFormat;
          xform->apply(dstColorFormat, f32Array->data(), srcColorFormat,
                       content.data(), numPixels,
                       SkAlphaType::kUnpremul_SkAlphaType);
          return f32Array;
          break;
        default:
          NOTREACHED();
      }
      break;

    default:
      NOTREACHED();
  }
  return nullptr;
}

// For ImageData, the color space is only specified by color settings.
// It cannot have a SkColorSpace. This doesn't mean anything. Fix this.
sk_sp<SkColorSpace> ImageData::skColorSpace() {
  if (!RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled() ||
      !RuntimeEnabledFeatures::colorCorrectRenderingEnabled())
    return nullptr;

  return SkColorSpace::MakeSRGB();
}

// This function returns the proper SkColorSpace to color correct the pixels
// stored in ImageData before copying to the canvas. For now, it assumes that
// both ImageData and canvas use a linear gamma curve.
sk_sp<SkColorSpace> ImageData::getSkColorSpace(
    const CanvasColorSpace& colorSpace,
    const CanvasPixelFormat& pixelFormat) {
  switch (colorSpace) {
    case kLegacyCanvasColorSpace:
      return (gfx::ColorSpace::CreateSRGB()).ToSkColorSpace();
    case kSRGBCanvasColorSpace:
      if (pixelFormat == kF16CanvasPixelFormat)
        return (gfx::ColorSpace::CreateSCRGBLinear()).ToSkColorSpace();
      return (gfx::ColorSpace::CreateSRGB()).ToSkColorSpace();
    case kRec2020CanvasColorSpace:
      return (gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                              gfx::ColorSpace::TransferID::LINEAR))
          .ToSkColorSpace();
    case kP3CanvasColorSpace:
      return (gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTEST432_1,
                              gfx::ColorSpace::TransferID::LINEAR))
          .ToSkColorSpace();
  }
  NOTREACHED();
  return nullptr;
}

sk_sp<SkColorSpace> ImageData::getSkColorSpaceForTest(
    const CanvasColorSpace& colorSpace,
    const CanvasPixelFormat& pixelFormat) {
  return getSkColorSpace(colorSpace, pixelFormat);
}

bool ImageData::imageDataInCanvasColorSettings(
    const CanvasColorSpace& canvasColorSpace,
    const CanvasPixelFormat& canvasPixelFormat,
    std::unique_ptr<uint8_t[]>& convertedPixels) {
  if (!m_data && !m_dataU16 && !m_dataF32)
    return false;

  // If canvas and image data are both in the same color space and pixel format
  // is 8-8-8-8, just return the embedded data.
  CanvasColorSpace imageDataColorSpace =
      ImageData::canvasColorSpace(m_colorSettings.colorSpace());
  if (canvasPixelFormat == kRGBA8CanvasPixelFormat &&
      m_colorSettings.storageFormat() == kUint8ClampedArrayStorageFormatName) {
    if ((canvasColorSpace == kLegacyCanvasColorSpace ||
         canvasColorSpace == kSRGBCanvasColorSpace) &&
        (imageDataColorSpace == kLegacyCanvasColorSpace ||
         imageDataColorSpace == kSRGBCanvasColorSpace)) {
      memcpy(convertedPixels.get(), m_data->data(), m_data->length());
      return true;
    }
  }

  // Otherwise, color convert the pixels.
  unsigned numPixels = m_size.width() * m_size.height();
  unsigned dataLength = numPixels * 4;
  void* srcData = nullptr;
  std::unique_ptr<uint16_t[]> leData;
  SkColorSpaceXform::ColorFormat srcColorFormat =
      SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
  if (m_data) {
    srcData = static_cast<void*>(m_data->data());
    DCHECK(srcData);
  } else if (m_dataU16) {
    srcData = static_cast<void*>(m_dataU16->data());
    DCHECK(srcData);
    // SkColorSpaceXform::apply expects U16 data to be in Big Endian byte
    // order, while srcData is always Little Endian. As we cannot consume
    // ImageData here, we change the byte order in a copy.
    leData.reset(new uint16_t[dataLength]());
    memcpy(leData.get(), srcData, dataLength * 2);
    uint16_t swapValue = 0;
    for (unsigned i = 0; i < dataLength; i++) {
      swapValue = leData[i];
      leData[i] = swapValue >> 8 | swapValue << 8;
    }
    srcData = static_cast<void*>(leData.get());
    DCHECK(srcData);
    srcColorFormat = SkColorSpaceXform::ColorFormat::kRGBA_U16_BE_ColorFormat;
  } else if (m_dataF32) {
    srcData = static_cast<void*>(m_dataF32->data());
    DCHECK(srcData);
    srcColorFormat = SkColorSpaceXform::ColorFormat::kRGBA_F32_ColorFormat;
  } else {
    NOTREACHED();
  }

  sk_sp<SkColorSpace> srcColorSpace = nullptr;
  if (m_data) {
    srcColorSpace = ImageData::getSkColorSpace(imageDataColorSpace,
                                               kRGBA8CanvasPixelFormat);
  } else {
    srcColorSpace =
        ImageData::getSkColorSpace(imageDataColorSpace, kF16CanvasPixelFormat);
  }

  sk_sp<SkColorSpace> dstColorSpace =
      ImageData::getSkColorSpace(canvasColorSpace, canvasPixelFormat);

  SkColorSpaceXform::ColorFormat dstColorFormat =
      SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
  if (canvasPixelFormat == kF16CanvasPixelFormat)
    dstColorFormat = SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat;

  if (SkColorSpace::Equals(srcColorSpace.get(), dstColorSpace.get()) &&
      srcColorFormat == dstColorFormat)
    return static_cast<unsigned char*>(srcData);

  std::unique_ptr<SkColorSpaceXform> xform =
      SkColorSpaceXform::New(srcColorSpace.get(), dstColorSpace.get());

  if (!xform->apply(dstColorFormat, convertedPixels.get(), srcColorFormat,
                    srcData, numPixels, SkAlphaType::kUnpremul_SkAlphaType))
    return false;
  return true;
}

void ImageData::trace(Visitor* visitor) {
  visitor->trace(m_data);
  visitor->trace(m_dataU16);
  visitor->trace(m_dataF32);
  visitor->trace(m_dataUnion);
}

ImageData::ImageData(const IntSize& size,
                     DOMArrayBufferView* data,
                     const ImageDataColorSettings* colorSettings)
    : m_size(size) {
  DCHECK_GE(size.width(), 0);
  DCHECK_GE(size.height(), 0);
  DCHECK(data);

  m_data = nullptr;
  m_dataU16 = nullptr;
  m_dataF32 = nullptr;

  if (colorSettings) {
    m_colorSettings.setColorSpace(colorSettings->colorSpace());
    m_colorSettings.setStorageFormat(colorSettings->storageFormat());
  }

  ImageDataStorageFormat storageFormat =
      imageDataStorageFormat(m_colorSettings.storageFormat());

  switch (storageFormat) {
    case kUint8ClampedArrayStorageFormat:
      DCHECK(data->type() == DOMArrayBufferView::ViewType::TypeUint8Clamped);
      m_data = const_cast<DOMUint8ClampedArray*>(
          static_cast<const DOMUint8ClampedArray*>(data));
      DCHECK(m_data);
      m_dataUnion.setUint8ClampedArray(m_data);
      SECURITY_CHECK(static_cast<unsigned>(size.width() * size.height() * 4) <=
                     m_data->length());
      break;

    case kUint16ArrayStorageFormat:
      DCHECK(data->type() == DOMArrayBufferView::ViewType::TypeUint16);
      m_dataU16 =
          const_cast<DOMUint16Array*>(static_cast<const DOMUint16Array*>(data));
      DCHECK(m_dataU16);
      m_dataUnion.setUint16Array(m_dataU16);
      SECURITY_CHECK(static_cast<unsigned>(size.width() * size.height() * 4) <=
                     m_dataU16->length());
      break;

    case kFloat32ArrayStorageFormat:
      DCHECK(data->type() == DOMArrayBufferView::ViewType::TypeFloat32);
      m_dataF32 = const_cast<DOMFloat32Array*>(
          static_cast<const DOMFloat32Array*>(data));
      DCHECK(m_dataF32);
      m_dataUnion.setFloat32Array(m_dataF32);
      SECURITY_CHECK(static_cast<unsigned>(size.width() * size.height() * 4) <=
                     m_dataF32->length());
      break;

    default:
      NOTREACHED();
  }
}

}  // namespace blink
