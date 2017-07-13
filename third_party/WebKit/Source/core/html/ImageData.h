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

#include "bindings/core/v8/Uint8ClampedArrayOrUint16ArrayOrFloat32Array.h"
#include "core/CoreExport.h"
#include "core/dom/ArrayBufferViewHelpers.h"
#include "core/dom/DOMTypedArray.h"
#include "core/html/ImageDataColorSettings.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/CanvasColorParams.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/CheckedNumeric.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/text/WTFString.h"
#include "third_party/skia/include/core/SkColorSpace.h"

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

constexpr const char* kUint8ClampedArrayStorageFormatName = "uint8";
constexpr const char* kUint16ArrayStorageFormatName = "uint16";
constexpr const char* kFloat32ArrayStorageFormatName = "float32";

class CORE_EXPORT ImageData final : public GarbageCollectedFinalized<ImageData>,
                                    public ScriptWrappable,
                                    public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ImageData* Create(const IntSize&,
                           const ImageDataColorSettings* = nullptr);
  static ImageData* Create(const IntSize&,
                           CanvasColorSpace,
                           ImageDataStorageFormat);
  static ImageData* Create(const IntSize&,
                           NotShared<DOMArrayBufferView>,
                           const ImageDataColorSettings* = nullptr);

  static ImageData* Create(unsigned width, unsigned height, ExceptionState&);
  static ImageData* Create(NotShared<DOMUint8ClampedArray>,
                           unsigned width,
                           ExceptionState&);
  static ImageData* Create(NotShared<DOMUint8ClampedArray>,
                           unsigned width,
                           unsigned height,
                           ExceptionState&);

  static ImageData* CreateImageData(unsigned width,
                                    unsigned height,
                                    const ImageDataColorSettings&,
                                    ExceptionState&);
  static ImageData* CreateImageData(ImageDataArray&,
                                    unsigned width,
                                    unsigned height,
                                    ImageDataColorSettings&,
                                    ExceptionState&);

  void getColorSettings(ImageDataColorSettings& result) {
    result = color_settings_;
  }

  static ImageData* CreateForTest(const IntSize&);
  static ImageData* CreateForTest(const IntSize&,
                                  DOMArrayBufferView*,
                                  const ImageDataColorSettings* = nullptr);

  ImageData* CropRect(const IntRect&, bool = false);

  ImageDataStorageFormat GetImageDataStorageFormat();
  static CanvasColorSpace GetCanvasColorSpace(const String&);
  static String CanvasColorSpaceName(CanvasColorSpace);
  static ImageDataStorageFormat GetImageDataStorageFormat(const String&);
  static unsigned StorageFormatDataSize(const String&);
  static unsigned StorageFormatDataSize(ImageDataStorageFormat);
  static DOMArrayBufferView*
  ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
      WTF::ArrayBufferContents&,
      CanvasPixelFormat,
      ImageDataStorageFormat);

  IntSize Size() const { return size_; }
  int width() const { return size_.Width(); }
  int height() const { return size_.Height(); }

  DOMUint8ClampedArray* data();
  const DOMUint8ClampedArray* data() const;
  ImageDataArray& dataUnion() { return data_union_; }
  const ImageDataArray& dataUnion() const { return data_union_; }
  void dataUnion(ImageDataArray& result) { result = data_union_; };

  DOMArrayBufferBase* BufferBase() const;
  CanvasColorParams GetCanvasColorParams();
  bool ImageDataInCanvasColorSettings(CanvasColorSpace,
                                      CanvasPixelFormat,
                                      std::unique_ptr<uint8_t[]>&);
  bool ImageDataInCanvasColorSettings(const CanvasColorParams&,
                                      std::unique_ptr<uint8_t[]>&);

  // ImageBitmapSource implementation
  IntSize BitmapSourceSize() const override { return size_; }
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  EventTarget&,
                                  Optional<IntRect> crop_rect,
                                  const ImageBitmapOptions&,
                                  ExceptionState&) override;

  void Trace(Visitor*);

  WARN_UNUSED_RESULT v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) override;

  static bool ValidateConstructorArguments(
      const unsigned&,
      const IntSize* = nullptr,
      const unsigned& = 0,
      const unsigned& = 0,
      const DOMArrayBufferView* = nullptr,
      const ImageDataColorSettings* = nullptr,
      ExceptionState* = nullptr);

 private:
  ImageData(const IntSize&,
            DOMArrayBufferView*,
            const ImageDataColorSettings* = nullptr);

  IntSize size_;
  ImageDataColorSettings color_settings_;
  ImageDataArray data_union_;
  Member<DOMUint8ClampedArray> data_;
  Member<DOMUint16Array> data_u16_;
  Member<DOMFloat32Array> data_f32_;

  static DOMArrayBufferView* AllocateAndValidateDataArray(
      const unsigned&,
      ImageDataStorageFormat,
      ExceptionState* = nullptr);

  static DOMUint8ClampedArray* AllocateAndValidateUint8ClampedArray(
      const unsigned&,
      ExceptionState* = nullptr);

  static DOMUint16Array* AllocateAndValidateUint16Array(
      const unsigned&,
      ExceptionState* = nullptr);

  static DOMFloat32Array* AllocateAndValidateFloat32Array(
      const unsigned&,
      ExceptionState* = nullptr);

  static DOMFloat32Array* ConvertFloat16ArrayToFloat32Array(const uint16_t*,
                                                            unsigned);
};

}  // namespace blink

#endif  // ImageData_h
