// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/Float32ImageData.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Float32Array.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/ImageBitmap.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/CheckedNumeric.h"

namespace blink {

bool Float32ImageData::validateConstructorArguments(
    const unsigned& paramFlags,
    const IntSize* size,
    const unsigned& width,
    const unsigned& height,
    const DOMFloat32Array* data,
    const String* colorSpace,
    ExceptionState* exceptionState) {
  return ImageData::validateConstructorArguments(
      paramFlags, size, width, height, data, colorSpace, exceptionState,
      kFloat32ImageData);
}

DOMFloat32Array* Float32ImageData::allocateAndValidateFloat32Array(
    const unsigned& length,
    ExceptionState* exceptionState) {
  if (!length)
    return nullptr;
  DOMFloat32Array* dataArray = DOMFloat32Array::createOrNull(length);
  if (!dataArray || length != dataArray->length()) {
    if (exceptionState) {
      exceptionState->throwDOMException(
          V8RangeError, "Out of memory at Float32ImageData creation");
    }
    return nullptr;
  }
  return dataArray;
}

Float32ImageData* Float32ImageData::create(const IntSize& size) {
  if (!Float32ImageData::validateConstructorArguments(kParamSize, &size))
    return nullptr;
  DOMFloat32Array* dataArray =
      Float32ImageData::allocateAndValidateFloat32Array(4 * size.width() *
                                                        size.height());
  return dataArray ? new Float32ImageData(size, dataArray) : nullptr;
}

Float32ImageData* Float32ImageData::create(const IntSize& size,
                                           DOMFloat32Array* dataArray) {
  if (!Float32ImageData::validateConstructorArguments(kParamSize | kParamData,
                                                      &size, 0, 0, dataArray))
    return nullptr;
  return new Float32ImageData(size, dataArray);
}

Float32ImageData* Float32ImageData::create(unsigned width,
                                           unsigned height,
                                           ExceptionState& exceptionState) {
  if (!Float32ImageData::validateConstructorArguments(
          kParamWidth | kParamHeight, nullptr, width, height, nullptr, nullptr,
          &exceptionState))
    return nullptr;
  DOMFloat32Array* dataArray =
      Float32ImageData::allocateAndValidateFloat32Array(4 * width * height,
                                                        &exceptionState);
  return dataArray ? new Float32ImageData(IntSize(width, height), dataArray)
                   : nullptr;
}

Float32ImageData* Float32ImageData::create(DOMFloat32Array* data,
                                           unsigned width,
                                           ExceptionState& exceptionState) {
  if (!Float32ImageData::validateConstructorArguments(kParamData | kParamWidth,
                                                      nullptr, width, 0, data,
                                                      nullptr, &exceptionState))
    return nullptr;
  unsigned height = data->length() / (width * 4);
  return new Float32ImageData(IntSize(width, height), data);
}

Float32ImageData* Float32ImageData::create(DOMFloat32Array* data,
                                           unsigned width,
                                           unsigned height,
                                           ExceptionState& exceptionState) {
  if (!Float32ImageData::validateConstructorArguments(
          kParamData | kParamWidth | kParamHeight, nullptr, width, height, data,
          nullptr, &exceptionState))
    return nullptr;
  return new Float32ImageData(IntSize(width, height), data);
}

Float32ImageData* Float32ImageData::create(unsigned width,
                                           unsigned height,
                                           String colorSpace,
                                           ExceptionState& exceptionState) {
  if (!Float32ImageData::validateConstructorArguments(
          kParamWidth | kParamHeight | kParamColorSpace, nullptr, width, height,
          nullptr, &colorSpace, &exceptionState))
    return nullptr;

  DOMFloat32Array* dataArray =
      Float32ImageData::allocateAndValidateFloat32Array(4 * width * height,
                                                        &exceptionState);
  return dataArray ? new Float32ImageData(IntSize(width, height), dataArray,
                                          colorSpace)
                   : nullptr;
}

Float32ImageData* Float32ImageData::create(DOMFloat32Array* data,
                                           unsigned width,
                                           String colorSpace,
                                           ExceptionState& exceptionState) {
  if (!Float32ImageData::validateConstructorArguments(
          kParamData | kParamWidth | kParamColorSpace, nullptr, width, 0, data,
          &colorSpace, &exceptionState))
    return nullptr;
  unsigned height = data->length() / (width * 4);
  return new Float32ImageData(IntSize(width, height), data, colorSpace);
}

Float32ImageData* Float32ImageData::create(DOMFloat32Array* data,
                                           unsigned width,
                                           unsigned height,
                                           String colorSpace,
                                           ExceptionState& exceptionState) {
  if (!Float32ImageData::validateConstructorArguments(
          kParamData | kParamWidth | kParamHeight | kParamColorSpace, nullptr,
          width, height, data, &colorSpace, &exceptionState))
    return nullptr;
  return new Float32ImageData(IntSize(width, height), data, colorSpace);
}

v8::Local<v8::Object> Float32ImageData::associateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapperType,
    v8::Local<v8::Object> wrapper) {
  wrapper =
      ScriptWrappable::associateWithWrapper(isolate, wrapperType, wrapper);

  if (!wrapper.IsEmpty() && m_data.get()) {
    // Create a V8 Float32Array object and set the "data" property
    // of the Float32ImageData object to the created v8 object, eliminating the
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

Float32ImageData::Float32ImageData(const IntSize& size,
                                   DOMFloat32Array* dataArray,
                                   String colorSpaceName)
    : m_size(size),
      m_colorSpace(ImageData::getImageDataColorSpace(colorSpaceName)),
      m_data(dataArray) {
  DCHECK_GE(size.width(), 0);
  DCHECK_GE(size.height(), 0);
  SECURITY_CHECK(static_cast<unsigned>(size.width() * size.height() * 4) <=
                 m_data->length());
}

}  // namespace blink
