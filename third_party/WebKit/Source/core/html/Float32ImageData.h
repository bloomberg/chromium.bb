// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Float32ImageData_h
#define Float32ImageData_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/DOMTypedArray.h"
#include "core/html/ImageData.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "wtf/Compiler.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT Float32ImageData final
    : public GarbageCollected<Float32ImageData>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Float32ImageData* create(const IntSize&);
  static Float32ImageData* create(const IntSize&, DOMFloat32Array*);
  static Float32ImageData* create(unsigned width,
                                  unsigned height,
                                  ExceptionState&);
  static Float32ImageData* create(unsigned width,
                                  unsigned height,
                                  String colorSpace,
                                  ExceptionState&);
  static Float32ImageData* create(DOMFloat32Array*,
                                  unsigned width,
                                  ExceptionState&);
  static Float32ImageData* create(DOMFloat32Array*,
                                  unsigned width,
                                  String colorSpace,
                                  ExceptionState&);
  static Float32ImageData* create(DOMFloat32Array*,
                                  unsigned width,
                                  unsigned height,
                                  ExceptionState&);
  static Float32ImageData* create(DOMFloat32Array*,
                                  unsigned width,
                                  unsigned height,
                                  String colorSpace,
                                  ExceptionState&);

  IntSize size() const { return m_size; }
  int width() const { return m_size.width(); }
  int height() const { return m_size.height(); }
  String colorSpace() const {
    return ImageData::getImageDataColorSpaceName(m_colorSpace);
  }
  ImageDataColorSpace imageDataColorSpace() { return m_colorSpace; }
  const DOMFloat32Array* data() const { return m_data; }
  DOMFloat32Array* data() { return m_data; }

  DEFINE_INLINE_TRACE() { visitor->trace(m_data); }

  WARN_UNUSED_RESULT v8::Local<v8::Object> associateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) override;

 private:
  Float32ImageData(const IntSize&,
                   DOMFloat32Array*,
                   String = kLinearRGBImageDataColorSpaceName);

  IntSize m_size;
  ImageDataColorSpace m_colorSpace;
  Member<DOMFloat32Array> m_data;

  static bool validateConstructorArguments(const unsigned&,
                                           const IntSize* = nullptr,
                                           const unsigned& = 0,
                                           const unsigned& = 0,
                                           const DOMFloat32Array* = nullptr,
                                           const String* = nullptr,
                                           ExceptionState* = nullptr);

  static DOMFloat32Array* allocateAndValidateFloat32Array(
      const unsigned&,
      ExceptionState* = nullptr);
};

}  // namespace blink

#endif  // Float32ImageData_h
