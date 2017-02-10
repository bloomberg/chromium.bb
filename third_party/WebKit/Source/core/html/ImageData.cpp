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

namespace blink {

bool ImageData::validateConstructorArguments(const unsigned& paramFlags,
                                             const IntSize* size,
                                             const unsigned& width,
                                             const unsigned& height,
                                             const DOMArrayBufferView* data,
                                             const String* colorSpace,
                                             ExceptionState* exceptionState,
                                             ImageDataType imageDataType) {
  if (paramFlags & kParamData) {
    if (data->type() != DOMArrayBufferView::ViewType::TypeUint8Clamped &&
        data->type() != DOMArrayBufferView::ViewType::TypeFloat32)
      return false;
    if (data->type() == DOMArrayBufferView::ViewType::TypeUint8Clamped &&
        imageDataType != kUint8ClampedImageData)
      imageDataType = kFloat32ImageData;
  }

  // ImageData::create parameters without ExceptionState
  if (paramFlags & kParamSize) {
    if (!size->width() || !size->height())
      return false;
    CheckedNumeric<unsigned> dataSize = 4;
    dataSize *= size->width();
    dataSize *= size->height();
    if (!dataSize.IsValid())
      return false;
    if (paramFlags & kParamData) {
      DCHECK(data);
      unsigned length =
          data->type() == DOMArrayBufferView::ViewType::TypeUint8Clamped
              ? (const_cast<DOMUint8ClampedArray*>(
                     static_cast<const DOMUint8ClampedArray*>(data)))
                    ->length()
              : (const_cast<DOMFloat32Array*>(
                     static_cast<const DOMFloat32Array*>(data)))
                    ->length();
      if (dataSize.ValueOrDie() > length)
        return false;
    }
    return true;
  }

  // ImageData::create parameters with ExceptionState
  if ((paramFlags & kParamWidth) && !width) {
    exceptionState->throwDOMException(
        IndexSizeError, "The source width is zero or not a number.");
    return false;
  }
  if ((paramFlags & kParamHeight) && !height) {
    exceptionState->throwDOMException(
        IndexSizeError, "The source height is zero or not a number.");
    return false;
  }
  if (paramFlags & (kParamWidth | kParamHeight)) {
    CheckedNumeric<unsigned> dataSize = 4;
    dataSize *= width;
    dataSize *= height;
    if (!dataSize.IsValid()) {
      exceptionState->throwDOMException(
          IndexSizeError,
          "The requested image size exceeds the supported range.");
      return false;
    }
  }
  if (paramFlags & kParamData) {
    DCHECK(data);
    unsigned length =
        data->type() == DOMArrayBufferView::ViewType::TypeUint8Clamped
            ? (const_cast<DOMUint8ClampedArray*>(
                   static_cast<const DOMUint8ClampedArray*>(data)))
                  ->length()
            : (const_cast<DOMFloat32Array*>(
                   static_cast<const DOMFloat32Array*>(data)))
                  ->length();
    if (!length) {
      exceptionState->throwDOMException(IndexSizeError,
                                        "The input data has zero elements.");
      return false;
    }
    if (length % 4) {
      exceptionState->throwDOMException(
          IndexSizeError, "The input data length is not a multiple of 4.");
      return false;
    }
    length /= 4;
    if (length % width) {
      exceptionState->throwDOMException(
          IndexSizeError,
          "The input data length is not a multiple of (4 * width).");
      return false;
    }
    if ((paramFlags & kParamHeight) && height != length / width) {
      exceptionState->throwDOMException(
          IndexSizeError,
          "The input data length is not equal to (4 * width * height).");
      return false;
    }
  }
  if (paramFlags & kParamColorSpace) {
    if (!colorSpace || colorSpace->length() == 0) {
      exceptionState->throwDOMException(
          NotSupportedError, "The source color space is not defined.");
      return false;
    }
    if (imageDataType == kUint8ClampedImageData &&
        *colorSpace != kLegacyImageDataColorSpaceName &&
        *colorSpace != kSRGBImageDataColorSpaceName) {
      exceptionState->throwDOMException(NotSupportedError,
                                        "The input color space is not "
                                        "supported in "
                                        "Uint8ClampedArray-backed ImageData.");
      return false;
    }
    if (imageDataType == kFloat32ImageData &&
        *colorSpace != kLinearRGBImageDataColorSpaceName) {
      exceptionState->throwDOMException(NotSupportedError,
                                        "The input color space is not "
                                        "supported in "
                                        "Float32Array-backed ImageData.");
      return false;
    }
  }
  return true;
}

DOMUint8ClampedArray* ImageData::allocateAndValidateUint8ClampedArray(
    const unsigned& length,
    ExceptionState* exceptionState) {
  if (!length)
    return nullptr;
  DOMUint8ClampedArray* dataArray = DOMUint8ClampedArray::createOrNull(length);
  if (!dataArray || length != dataArray->length()) {
    if (exceptionState) {
      exceptionState->throwDOMException(V8RangeError,
                                        "Out of memory at ImageData creation");
    }
    return nullptr;
  }
  return dataArray;
}

ImageData* ImageData::create(const IntSize& size) {
  if (!ImageData::validateConstructorArguments(kParamSize, &size))
    return nullptr;
  DOMUint8ClampedArray* byteArray =
      ImageData::allocateAndValidateUint8ClampedArray(4 * size.width() *
                                                      size.height());
  if (!byteArray)
    return nullptr;
  return new ImageData(size, byteArray);
}

// This function accepts size (0, 0).
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

ImageData* ImageData::create(const IntSize& size,
                             DOMUint8ClampedArray* byteArray) {
  if (!ImageData::validateConstructorArguments(kParamSize | kParamData, &size,
                                               0, 0, byteArray))
    return nullptr;
  return new ImageData(size, byteArray);
}

ImageData* ImageData::create(const IntSize& size,
                             DOMUint8ClampedArray* byteArray,
                             const String& colorSpace) {
  if (!ImageData::validateConstructorArguments(
          kParamSize | kParamData | kParamColorSpace, &size, 0, 0, byteArray,
          &colorSpace))
    return nullptr;
  return new ImageData(size, byteArray, colorSpace);
}

ImageData* ImageData::create(unsigned width,
                             unsigned height,
                             ExceptionState& exceptionState) {
  if (!ImageData::validateConstructorArguments(kParamWidth | kParamHeight,
                                               nullptr, width, height, nullptr,
                                               nullptr, &exceptionState))
    return nullptr;
  DOMUint8ClampedArray* byteArray =
      ImageData::allocateAndValidateUint8ClampedArray(4 * width * height,
                                                      &exceptionState);
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

ImageData* ImageData::createImageData(unsigned width,
                                      unsigned height,
                                      String colorSpace,
                                      ExceptionState& exceptionState) {
  if (!ImageData::validateConstructorArguments(
          kParamWidth | kParamHeight | kParamColorSpace, nullptr, width, height,
          nullptr, &colorSpace, &exceptionState))
    return nullptr;

  DOMUint8ClampedArray* byteArray =
      ImageData::allocateAndValidateUint8ClampedArray(4 * width * height,
                                                      &exceptionState);
  return byteArray
             ? new ImageData(IntSize(width, height), byteArray, colorSpace)
             : nullptr;
}

ImageData* ImageData::createImageData(DOMUint8ClampedArray* data,
                                      unsigned width,
                                      String colorSpace,
                                      ExceptionState& exceptionState) {
  if (!ImageData::validateConstructorArguments(
          kParamData | kParamWidth | kParamColorSpace, nullptr, width, 0, data,
          &colorSpace, &exceptionState))
    return nullptr;
  unsigned height = data->length() / (width * 4);
  return new ImageData(IntSize(width, height), data, colorSpace);
}

ImageData* ImageData::createImageData(DOMUint8ClampedArray* data,
                                      unsigned width,
                                      unsigned height,
                                      String colorSpace,
                                      ExceptionState& exceptionState) {
  if (!ImageData::validateConstructorArguments(
          kParamData | kParamWidth | kParamHeight | kParamColorSpace, nullptr,
          width, height, data, &colorSpace, &exceptionState))
    return nullptr;
  return new ImageData(IntSize(width, height), data, colorSpace);
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

  if (!wrapper.IsEmpty() && m_data.get()) {
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

ImageDataColorSpace ImageData::getImageDataColorSpace(String colorSpaceName) {
  if (colorSpaceName == kLegacyImageDataColorSpaceName)
    return kLegacyImageDataColorSpace;
  if (colorSpaceName == kSRGBImageDataColorSpaceName)
    return kSRGBImageDataColorSpace;
  if (colorSpaceName == kLinearRGBImageDataColorSpaceName)
    return kLinearRGBImageDataColorSpace;
  NOTREACHED();
  return kLegacyImageDataColorSpace;
}

String ImageData::getImageDataColorSpaceName(ImageDataColorSpace colorSpace) {
  switch (colorSpace) {
    case kLegacyImageDataColorSpace:
      return kLegacyImageDataColorSpaceName;
    case kSRGBImageDataColorSpace:
      return kSRGBImageDataColorSpaceName;
    case kLinearRGBImageDataColorSpace:
      return kLinearRGBImageDataColorSpaceName;
  }
  NOTREACHED();
  return String();
}

sk_sp<SkColorSpace> ImageData::imageDataColorSpaceToSkColorSpace(
    ImageDataColorSpace colorSpace) {
  switch (colorSpace) {
    case kLegacyImageDataColorSpace:
      return ColorBehavior::globalTargetColorSpace().ToSkColorSpace();
    case kSRGBImageDataColorSpace:
      return SkColorSpace::MakeSRGB();
    case kLinearRGBImageDataColorSpace:
      return SkColorSpace::MakeSRGBLinear();
  }
  NOTREACHED();
  return nullptr;
}

sk_sp<SkColorSpace> ImageData::getSkColorSpace() {
  if (!RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled() ||
      !RuntimeEnabledFeatures::colorCorrectRenderingEnabled())
    return nullptr;
  return ImageData::imageDataColorSpaceToSkColorSpace(m_colorSpace);
}

ImageData::ImageData(const IntSize& size,
                     DOMUint8ClampedArray* byteArray,
                     String colorSpaceName)
    : m_size(size),
      m_colorSpace(getImageDataColorSpace(colorSpaceName)),
      m_data(byteArray) {
  DCHECK_GE(size.width(), 0);
  DCHECK_GE(size.height(), 0);
  SECURITY_CHECK(static_cast<unsigned>(size.width() * size.height() * 4) <=
                 m_data->length());
}

}  // namespace blink
