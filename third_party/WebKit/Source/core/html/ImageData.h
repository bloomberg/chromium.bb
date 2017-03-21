/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef ImageData_h
#define ImageData_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/Uint8ClampedArrayOrUint16ArrayOrFloat32Array.h"
#include "core/CoreExport.h"
#include "core/dom/DOMTypedArray.h"
#include "core/html/ImageDataColorSettings.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "wtf/CheckedNumeric.h"
#include "wtf/Compiler.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ImageBitmapOptions;

typedef Uint8ClampedArrayOrUint16ArrayOrFloat32Array ImageDataArray;

enum ConstructorParams {
  kParamSize = 1,
  kParamWidth = 1 << 1,
  kParamHeight = 1 << 2,
  kParamData = 1 << 3,
};

enum ImageDataStorageFormat {
  kUint8ClampedArrayStorageFormat,
  kUint16ArrayStorageFormat,
  kFloat32ArrayStorageFormat,
};

enum StorageFormatSource {
  kStorageFormatFromColorSettings,
  kStorageFormatFromBufferType,
};

constexpr const char* kUint8ClampedArrayStorageFormatName = "uint8";
constexpr const char* kUint16ArrayStorageFormatName = "uint16";
constexpr const char* kFloat32ArrayStorageFormatName = "float32";

class CORE_EXPORT ImageData final : public GarbageCollectedFinalized<ImageData>,
                                    public ScriptWrappable,
                                    public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ImageData* create(const IntSize&);
  static ImageData* create(const IntSize&, DOMUint8ClampedArray*);

  static ImageData* create(unsigned width, unsigned height, ExceptionState&);
  static ImageData* create(DOMUint8ClampedArray*,
                           unsigned width,
                           ExceptionState&);
  static ImageData* create(DOMUint8ClampedArray*,
                           unsigned width,
                           unsigned height,
                           ExceptionState&);

  static ImageData* createForTest(const IntSize&);

  ImageData* createImageData(unsigned width,
                             unsigned height,
                             const ImageDataColorSettings&,
                             ExceptionState&);
  ImageData* createImageData(ImageDataArray&,
                             unsigned width,
                             unsigned height,
                             const ImageDataColorSettings&,
                             ExceptionState&);

  static ImageDataStorageFormat getImageDataStorageFormat(const String&);

  IntSize size() const { return m_size; }
  int width() const { return m_size.width(); }
  int height() const { return m_size.height(); }

  DOMUint8ClampedArray* data();
  const DOMUint8ClampedArray* data() const;
  ImageDataArray& dataUnion() { return m_dataUnion; }
  const ImageDataArray& dataUnion() const { return m_dataUnion; }
  void dataUnion(ImageDataArray& result) { result = m_dataUnion; };
  const ImageDataColorSettings& colorSettings() const {
    return m_colorSettings;
  }
  void colorSettings(ImageDataColorSettings& result) {
    result = m_colorSettings;
  };

  sk_sp<SkColorSpace> getSkColorSpace();

  // ImageBitmapSource implementation
  IntSize bitmapSourceSize() const override { return m_size; }
  ScriptPromise createImageBitmap(ScriptState*,
                                  EventTarget&,
                                  Optional<IntRect> cropRect,
                                  const ImageBitmapOptions&,
                                  ExceptionState&) override;

  void trace(Visitor*);

  WARN_UNUSED_RESULT v8::Local<v8::Object> associateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) override;

  static bool validateConstructorArguments(const unsigned&,
                                           const IntSize* = nullptr,
                                           const unsigned& = 0,
                                           const unsigned& = 0,
                                           const DOMArrayBufferView* = nullptr,
                                           ExceptionState* = nullptr);

 private:
  ImageData(const IntSize&,
            DOMArrayBufferView*,
            const ImageDataColorSettings* = nullptr,
            StorageFormatSource = kStorageFormatFromColorSettings);

  IntSize m_size;
  ImageDataColorSettings m_colorSettings;
  ImageDataArray m_dataUnion;
  Member<DOMUint8ClampedArray> m_data;
  Member<DOMUint16Array> m_dataU16;
  Member<DOMFloat32Array> m_dataF32;

  static DOMArrayBufferView* allocateAndValidateDataArray(
      const unsigned&,
      ImageDataStorageFormat,
      ExceptionState* = nullptr);
};

}  // namespace blink

#endif  // ImageData_h
