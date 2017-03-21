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

namespace blink {

bool RaiseDOMExceptionAndReturnFalse(ExceptionState* exceptionState,
                                     ExceptionCode exceptionCode,
                                     const char* message) {
  if (exceptionState)
    exceptionState->throwDOMException(exceptionCode, message);
  return false;
}

bool ImageData::validateConstructorArguments(const unsigned& paramFlags,
                                             const IntSize* size,
                                             const unsigned& width,
                                             const unsigned& height,
                                             const DOMArrayBufferView* data,
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
    switch (data->type()) {
      case DOMArrayBufferView::ViewType::TypeUint8Clamped:
        dataLength = data->view()->byteLength();
        break;
      case DOMArrayBufferView::ViewType::TypeUint16:
        dataLength = data->view()->byteLength() / 2;
        break;
      case DOMArrayBufferView::ViewType::TypeFloat32:
        dataLength = data->view()->byteLength() / 4;
        break;
      default:
        return RaiseDOMExceptionAndReturnFalse(
            exceptionState, NotSupportedError,
            "The input data type is not supported.");
    }

    if (!dataLength) {
      return RaiseDOMExceptionAndReturnFalse(
          exceptionState, IndexSizeError, "The input data has zero elements.");
    }

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

    if ((paramFlags & kParamHeight & kParamWidth) &&
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
  unsigned dataLength = 0;
  unsigned dataItemLength = 1;
  switch (storageFormat) {
    case kUint8ClampedArrayStorageFormat:
      dataArray = DOMUint8ClampedArray::createOrNull(length);
      break;
    case kUint16ArrayStorageFormat:
      dataArray = DOMUint16Array::createOrNull(length);
      dataItemLength = 2;
      break;
    case kFloat32ArrayStorageFormat:
      dataArray = DOMFloat32Array::createOrNull(length);
      dataItemLength = 4;
      break;
    default:
      NOTREACHED();
  }

  if (dataArray)
    dataLength = dataArray->view()->byteLength() / dataItemLength;

  if (!dataArray || length != dataLength) {
    if (exceptionState)
      exceptionState->throwDOMException(V8RangeError,
                                        "Out of memory at ImageData creation");
    return nullptr;
  }

  return dataArray;
}

ImageData* ImageData::create(const IntSize& size) {
  if (!ImageData::validateConstructorArguments(kParamSize, &size))
    return nullptr;
  DOMArrayBufferView* byteArray =
      allocateAndValidateDataArray(4 * static_cast<unsigned>(size.width()) *
                                       static_cast<unsigned>(size.height()),
                                   kUint8ClampedArrayStorageFormat);
  return byteArray ? new ImageData(size, byteArray) : nullptr;
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

ImageData* ImageData::create(const IntSize& size,
                             DOMUint8ClampedArray* byteArray) {
  if (!ImageData::validateConstructorArguments(kParamSize | kParamData, &size,
                                               0, 0, byteArray))
    return nullptr;

  return new ImageData(size, byteArray);
}

ImageData* ImageData::create(unsigned width,
                             unsigned height,
                             ExceptionState& exceptionState) {
  if (!ImageData::validateConstructorArguments(kParamWidth | kParamHeight,
                                               nullptr, width, height, nullptr,
                                               &exceptionState))
    return nullptr;

  DOMArrayBufferView* byteArray = allocateAndValidateDataArray(
      4 * width * height, kUint8ClampedArrayStorageFormat, &exceptionState);
  return byteArray ? new ImageData(IntSize(width, height), byteArray) : nullptr;
}

ImageData* ImageData::create(DOMUint8ClampedArray* data,
                             unsigned width,
                             ExceptionState& exceptionState) {
  if (!ImageData::validateConstructorArguments(
          kParamData | kParamWidth, nullptr, width, 0, data, &exceptionState))
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
          &exceptionState))
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
                                               &exceptionState))
    return nullptr;

  ImageDataStorageFormat storageFormat =
      ImageData::getImageDataStorageFormat(colorSettings.storageFormat());
  DOMArrayBufferView* bufferView = allocateAndValidateDataArray(
      4 * width * height, storageFormat, &exceptionState);

  if (!bufferView)
    return nullptr;

  return new ImageData(IntSize(width, height), bufferView, &colorSettings);
}

ImageData* ImageData::createImageData(
    ImageDataArray& data,
    unsigned width,
    unsigned height,
    const ImageDataColorSettings& colorSettings,
    ExceptionState& exceptionState) {
  DOMArrayBufferView* bufferView = nullptr;
  if (data.isUint8ClampedArray())
    bufferView = data.getAsUint8ClampedArray();
  else if (data.isUint16Array())
    bufferView = data.getAsUint16Array();
  else if (data.isFloat32Array())
    bufferView = data.getAsFloat32Array();
  else
    NOTREACHED();

  if (!ImageData::validateConstructorArguments(
          kParamData | kParamWidth | kParamHeight, nullptr, width, height,
          bufferView, &exceptionState))
    return nullptr;

  return new ImageData(IntSize(width, height), bufferView, &colorSettings,
                       kStorageFormatFromBufferType);
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

ImageDataStorageFormat ImageData::getImageDataStorageFormat(
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

// For ImageData, the color space is only specified by color settings.
// It cannot have a SkColorSpace. This doesn't mean anything. Fix this.
sk_sp<SkColorSpace> ImageData::getSkColorSpace() {
  if (!RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled() ||
      !RuntimeEnabledFeatures::colorCorrectRenderingEnabled())
    return nullptr;

  return SkColorSpace::MakeSRGB();
}

void ImageData::trace(Visitor* visitor) {
  visitor->trace(m_data);
  visitor->trace(m_dataU16);
  visitor->trace(m_dataF32);
  visitor->trace(m_dataUnion);
}

ImageData::ImageData(const IntSize& size,
                     DOMArrayBufferView* data,
                     const ImageDataColorSettings* colorSettings,
                     StorageFormatSource storageFormatSource)
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

  // if data is provided through the JS call, override the storage format.
  if (storageFormatSource == kStorageFormatFromBufferType) {
    switch (data->type()) {
      case DOMArrayBufferView::ViewType::TypeUint8Clamped:
        m_colorSettings.setStorageFormat(kUint8ClampedArrayStorageFormatName);
        break;
      case DOMArrayBufferView::ViewType::TypeUint16:
        m_colorSettings.setStorageFormat(kUint16ArrayStorageFormatName);
        break;
      case DOMArrayBufferView::ViewType::TypeFloat32:
        m_colorSettings.setStorageFormat(kFloat32ArrayStorageFormatName);
        break;
      default:
        NOTREACHED();
    }
  }

  ImageDataStorageFormat storageFormat =
      getImageDataStorageFormat(m_colorSettings.storageFormat());

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
