/*
 * Copyright (C) 2016 Google Inc. All rights reserved.
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
    : public GarbageCollectedFinalized<Float32ImageData>,
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
  const DOMFloat32Array* data() const { return m_data.get(); }
  DOMFloat32Array* data() { return m_data.get(); }

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
