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
#include "core/imagebitmap/ImageBitmap.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/ColorBehavior.h"
#include "third_party/skia/include/core/SkColorSpaceXform.h"

namespace blink {

bool RaiseDOMExceptionAndReturnFalse(ExceptionState* exception_state,
                                     ExceptionCode exception_code,
                                     const char* message) {
  if (exception_state)
    exception_state->ThrowDOMException(exception_code, message);
  return false;
}

bool ImageData::ValidateConstructorArguments(
    const unsigned& param_flags,
    const IntSize* size,
    const unsigned& width,
    const unsigned& height,
    const DOMArrayBufferView* data,
    const ImageDataColorSettings* color_settings,
    ExceptionState* exception_state) {
  // We accept all the combinations of colorSpace and storageFormat in an
  // ImageDataColorSettings to be stored in an ImageData. Therefore, we don't
  // check the color settings in this function.

  if ((param_flags & kParamWidth) && !width) {
    return RaiseDOMExceptionAndReturnFalse(
        exception_state, kIndexSizeError,
        "The source width is zero or not a number.");
  }

  if ((param_flags & kParamHeight) && !height) {
    return RaiseDOMExceptionAndReturnFalse(
        exception_state, kIndexSizeError,
        "The source height is zero or not a number.");
  }

  if (param_flags & (kParamWidth | kParamHeight)) {
    CheckedNumeric<unsigned> data_size = 4;
    if (color_settings) {
      data_size *=
          ImageData::StorageFormatDataSize(color_settings->storageFormat());
    }
    data_size *= width;
    data_size *= height;
    if (!data_size.IsValid())
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, kIndexSizeError,
          "The requested image size exceeds the supported range.");
  }

  unsigned data_length = 0;
  if (param_flags & kParamData) {
    DCHECK(data);
    if (data->GetType() != DOMArrayBufferView::ViewType::kTypeUint8Clamped &&
        data->GetType() != DOMArrayBufferView::ViewType::kTypeUint16 &&
        data->GetType() != DOMArrayBufferView::ViewType::kTypeFloat32) {
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, kNotSupportedError,
          "The input data type is not supported.");
    }

    if (!data->byteLength()) {
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, kIndexSizeError,
          "The input data has zero elements.");
    }

    data_length = data->byteLength() / data->TypeSize();
    if (data_length % 4) {
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, kIndexSizeError,
          "The input data length is not a multiple of 4.");
    }

    if ((param_flags & kParamWidth) && (data_length / 4) % width) {
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, kIndexSizeError,
          "The input data length is not a multiple of (4 * width).");
    }

    if ((param_flags & kParamWidth) && (param_flags & kParamHeight) &&
        height != data_length / (4 * width))
      return RaiseDOMExceptionAndReturnFalse(
          exception_state, kIndexSizeError,
          "The input data length is not equal to (4 * width * height).");
  }

  if (param_flags & kParamSize) {
    if (size->Width() <= 0 || size->Height() <= 0)
      return false;
    CheckedNumeric<unsigned> data_size = 4;
    data_size *= size->Width();
    data_size *= size->Height();
    if (!data_size.IsValid())
      return false;
    if (param_flags & kParamData) {
      if (data_size.ValueOrDie() > data_length)
        return false;
    }
  }

  return true;
}

DOMArrayBufferView* ImageData::AllocateAndValidateDataArray(
    const unsigned& length,
    ImageDataStorageFormat storage_format,
    ExceptionState* exception_state) {
  if (!length)
    return nullptr;

  DOMArrayBufferView* data_array = nullptr;
  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat:
      data_array = DOMUint8ClampedArray::CreateOrNull(length);
      break;
    case kUint16ArrayStorageFormat:
      data_array = DOMUint16Array::CreateOrNull(length);
      break;
    case kFloat32ArrayStorageFormat:
      data_array = DOMFloat32Array::CreateOrNull(length);
      break;
    default:
      NOTREACHED();
  }

  if (!data_array ||
      length != data_array->byteLength() / data_array->TypeSize()) {
    if (exception_state)
      exception_state->ThrowDOMException(kV8RangeError,
                                         "Out of memory at ImageData creation");
    return nullptr;
  }

  return data_array;
}

DOMUint8ClampedArray* ImageData::AllocateAndValidateUint8ClampedArray(
    const unsigned& length,
    ExceptionState* exception_state) {
  DOMArrayBufferView* buffer_view = AllocateAndValidateDataArray(
      length, kUint8ClampedArrayStorageFormat, exception_state);
  if (!buffer_view)
    return nullptr;
  DOMUint8ClampedArray* u8_array = const_cast<DOMUint8ClampedArray*>(
      static_cast<const DOMUint8ClampedArray*>(buffer_view));
  DCHECK(u8_array);
  return u8_array;
}

DOMUint16Array* ImageData::AllocateAndValidateUint16Array(
    const unsigned& length,
    ExceptionState* exception_state) {
  DOMArrayBufferView* buffer_view = AllocateAndValidateDataArray(
      length, kUint16ArrayStorageFormat, exception_state);
  if (!buffer_view)
    return nullptr;
  DOMUint16Array* u16_array = const_cast<DOMUint16Array*>(
      static_cast<const DOMUint16Array*>(buffer_view));
  DCHECK(u16_array);
  return u16_array;
}

DOMFloat32Array* ImageData::AllocateAndValidateFloat32Array(
    const unsigned& length,
    ExceptionState* exception_state) {
  DOMArrayBufferView* buffer_view = AllocateAndValidateDataArray(
      length, kFloat32ArrayStorageFormat, exception_state);
  if (!buffer_view)
    return nullptr;
  DOMFloat32Array* f32_array = const_cast<DOMFloat32Array*>(
      static_cast<const DOMFloat32Array*>(buffer_view));
  DCHECK(f32_array);
  return f32_array;
}

ImageData* ImageData::Create(const IntSize& size,
                             const ImageDataColorSettings* color_settings) {
  if (!ImageData::ValidateConstructorArguments(kParamSize, &size, 0, 0, nullptr,
                                               color_settings))
    return nullptr;
  ImageDataStorageFormat storage_format = kUint8ClampedArrayStorageFormat;
  if (color_settings) {
    storage_format =
        ImageData::GetImageDataStorageFormat(color_settings->storageFormat());
  }
  DOMArrayBufferView* data_array =
      AllocateAndValidateDataArray(4 * static_cast<unsigned>(size.Width()) *
                                       static_cast<unsigned>(size.Height()),
                                   storage_format);
  return data_array ? new ImageData(size, data_array, color_settings) : nullptr;
}

ImageData* ImageData::Create(const IntSize& size,
                             NotShared<DOMArrayBufferView> data_array,
                             const ImageDataColorSettings* color_settings) {
  if (!ImageData::ValidateConstructorArguments(kParamSize | kParamData, &size,
                                               0, 0, data_array.View(),
                                               color_settings))
    return nullptr;
  return new ImageData(size, data_array.View(), color_settings);
}

ImageData* ImageData::Create(unsigned width,
                             unsigned height,
                             ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(kParamWidth | kParamHeight,
                                               nullptr, width, height, nullptr,
                                               nullptr, &exception_state))
    return nullptr;

  DOMArrayBufferView* byte_array = AllocateAndValidateDataArray(
      4 * width * height, kUint8ClampedArrayStorageFormat, &exception_state);
  return byte_array ? new ImageData(IntSize(width, height), byte_array)
                    : nullptr;
}

ImageData* ImageData::Create(NotShared<DOMUint8ClampedArray> data,
                             unsigned width,
                             ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(kParamData | kParamWidth,
                                               nullptr, width, 0, data.View(),
                                               nullptr, &exception_state))
    return nullptr;

  unsigned height = data.View()->length() / (width * 4);
  return new ImageData(IntSize(width, height), data.View());
}

ImageData* ImageData::Create(NotShared<DOMUint8ClampedArray> data,
                             unsigned width,
                             unsigned height,
                             ExceptionState& exception_state) {
  if (!ImageData::ValidateConstructorArguments(
          kParamData | kParamWidth | kParamHeight, nullptr, width, height,
          data.View(), nullptr, &exception_state))
    return nullptr;

  return new ImageData(IntSize(width, height), data.View());
}

ImageData* ImageData::CreateImageData(
    unsigned width,
    unsigned height,
    const ImageDataColorSettings& color_settings,
    ExceptionState& exception_state) {
  if (!RuntimeEnabledFeatures::ColorCanvasExtensionsEnabled())
    return nullptr;

  if (!ImageData::ValidateConstructorArguments(
          kParamWidth | kParamHeight, nullptr, width, height, nullptr,
          &color_settings, &exception_state))
    return nullptr;

  ImageDataStorageFormat storage_format =
      ImageData::GetImageDataStorageFormat(color_settings.storageFormat());
  DOMArrayBufferView* buffer_view = AllocateAndValidateDataArray(
      4 * width * height, storage_format, &exception_state);

  if (!buffer_view)
    return nullptr;

  return new ImageData(IntSize(width, height), buffer_view, &color_settings);
}

ImageData* ImageData::CreateImageData(ImageDataArray& data,
                                      unsigned width,
                                      unsigned height,
                                      ImageDataColorSettings& color_settings,
                                      ExceptionState& exception_state) {
  if (!RuntimeEnabledFeatures::ColorCanvasExtensionsEnabled())
    return nullptr;

  DOMArrayBufferView* buffer_view = nullptr;

  // When pixels data is provided, we need to override the storage format of
  // ImageDataColorSettings with the one that matches the data type of the
  // pixels.
  String storage_format_name;

  if (data.isUint8ClampedArray()) {
    buffer_view = data.getAsUint8ClampedArray().View();
    storage_format_name = kUint8ClampedArrayStorageFormatName;
  } else if (data.isUint16Array()) {
    buffer_view = data.getAsUint16Array().View();
    storage_format_name = kUint16ArrayStorageFormatName;
  } else if (data.isFloat32Array()) {
    buffer_view = data.getAsFloat32Array().View();
    storage_format_name = kFloat32ArrayStorageFormatName;
  } else {
    NOTREACHED();
  }

  if (storage_format_name != color_settings.storageFormat())
    color_settings.setStorageFormat(storage_format_name);

  if (!ImageData::ValidateConstructorArguments(
          kParamData | kParamWidth | kParamHeight, nullptr, width, height,
          buffer_view, &color_settings, &exception_state))
    return nullptr;

  return new ImageData(IntSize(width, height), buffer_view, &color_settings);
}

// This function accepts size (0, 0) and always returns the ImageData in
// "srgb" color space and "uint8" storage format.
ImageData* ImageData::CreateForTest(const IntSize& size) {
  CheckedNumeric<unsigned> data_size = 4;
  data_size *= size.Width();
  data_size *= size.Height();
  if (!data_size.IsValid())
    return nullptr;

  DOMUint8ClampedArray* byte_array =
      DOMUint8ClampedArray::CreateOrNull(data_size.ValueOrDie());
  if (!byte_array)
    return nullptr;

  return new ImageData(size, byte_array);
}

// This function is called from unit tests, and all the parameters are supposed
// to be validated on the call site.
ImageData* ImageData::CreateForTest(
    const IntSize& size,
    DOMArrayBufferView* buffer_view,
    const ImageDataColorSettings* color_settings) {
  return new ImageData(size, buffer_view, color_settings);
}

// Crops ImageData to the intersect of its size and the given rectangle. If the
// intersection is empty or it cannot create the cropped ImageData it returns
// nullptr. This function leaves the source ImageData intact. When crop_rect
// covers all the ImageData, a copy of the ImageData is returned.
ImageData* ImageData::CropRect(const IntRect& crop_rect, bool flip_y) {
  IntRect src_rect(IntPoint(), size_);
  const IntRect dst_rect = Intersection(src_rect, crop_rect);
  if (dst_rect.IsEmpty())
    return nullptr;

  unsigned data_size = 4 * dst_rect.Width() * dst_rect.Height();
  DOMArrayBufferView* buffer_view = AllocateAndValidateDataArray(
      data_size,
      ImageData::GetImageDataStorageFormat(color_settings_.storageFormat()));
  if (!buffer_view)
    return nullptr;

  if (src_rect == dst_rect && !flip_y) {
    std::memcpy(buffer_view->BufferBase()->Data(), BufferBase()->Data(),
                data_size * buffer_view->TypeSize());
  } else {
    unsigned data_type_size =
        ImageData::StorageFormatDataSize(color_settings_.storageFormat());
    int src_index = (dst_rect.X() * src_rect.Width() + dst_rect.Y()) * 4;
    int dst_index = 0;
    if (flip_y)
      dst_index = (dst_rect.Height() - 1) * dst_rect.Width() * 4;
    int src_row_stride = src_rect.Width() * 4;
    int dst_row_stride = flip_y ? -dst_rect.Width() * 4 : dst_rect.Width() * 4;
    for (int i = 0; i < dst_rect.Height(); i++) {
      std::memcpy(
          static_cast<char*>(buffer_view->BufferBase()->Data()) +
              dst_index * data_type_size,
          static_cast<char*>(BufferBase()->Data()) + src_index * data_type_size,
          dst_rect.Width() * 4 * data_type_size);
      src_index += src_row_stride;
      dst_index += dst_row_stride;
    }
  }
  return new ImageData(dst_rect.Size(), buffer_view, &color_settings_);
}

ScriptPromise ImageData::CreateImageBitmap(ScriptState* script_state,
                                           EventTarget& event_target,
                                           Optional<IntRect> crop_rect,
                                           const ImageBitmapOptions& options,
                                           ExceptionState& exception_state) {
  if ((crop_rect &&
       !ImageBitmap::IsSourceSizeValid(crop_rect->Width(), crop_rect->Height(),
                                       exception_state)) ||
      !ImageBitmap::IsSourceSizeValid(BitmapSourceSize().Width(),
                                      BitmapSourceSize().Height(),
                                      exception_state))
    return ScriptPromise();
  if (data()->BufferBase()->IsNeutered()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "The source data has been neutered.");
    return ScriptPromise();
  }
  if (!ImageBitmap::IsResizeOptionValid(options, exception_state))
    return ScriptPromise();
  return ImageBitmapSource::FulfillImageBitmap(
      script_state, ImageBitmap::Create(this, crop_rect, options));
}

v8::Local<v8::Object> ImageData::AssociateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type,
    v8::Local<v8::Object> wrapper) {
  wrapper =
      ScriptWrappable::AssociateWithWrapper(isolate, wrapper_type, wrapper);

  if (!wrapper.IsEmpty() && data_) {
    // Create a V8 Uint8ClampedArray object and set the "data" property
    // of the ImageData object to the created v8 object, eliminating the
    // C++ callback when accessing the "data" property.
    v8::Local<v8::Value> pixel_array = ToV8(data_.Get(), wrapper, isolate);
    if (pixel_array.IsEmpty() ||
        !V8CallBoolean(wrapper->DefineOwnProperty(
            isolate->GetCurrentContext(), V8AtomicString(isolate, "data"),
            pixel_array, v8::ReadOnly)))
      return v8::Local<v8::Object>();
  }
  return wrapper;
}

const DOMUint8ClampedArray* ImageData::data() const {
  if (color_settings_.storageFormat() == kUint8ClampedArrayStorageFormatName)
    return data_.Get();
  return nullptr;
}

DOMUint8ClampedArray* ImageData::data() {
  if (color_settings_.storageFormat() == kUint8ClampedArrayStorageFormatName)
    return data_.Get();
  return nullptr;
}

CanvasColorSpace ImageData::GetCanvasColorSpace(
    const String& color_space_name) {
  if (color_space_name == kLegacyCanvasColorSpaceName)
    return kLegacyCanvasColorSpace;
  if (color_space_name == kSRGBCanvasColorSpaceName)
    return kSRGBCanvasColorSpace;
  if (color_space_name == kRec2020CanvasColorSpaceName)
    return kRec2020CanvasColorSpace;
  if (color_space_name == kP3CanvasColorSpaceName)
    return kP3CanvasColorSpace;
  NOTREACHED();
  return kSRGBCanvasColorSpace;
}

String ImageData::CanvasColorSpaceName(const CanvasColorSpace& color_space) {
  switch (color_space) {
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

ImageDataStorageFormat ImageData::GetImageDataStorageFormat(
    const String& storage_format_name) {
  if (storage_format_name == kUint8ClampedArrayStorageFormatName)
    return kUint8ClampedArrayStorageFormat;
  if (storage_format_name == kUint16ArrayStorageFormatName)
    return kUint16ArrayStorageFormat;
  if (storage_format_name == kFloat32ArrayStorageFormatName)
    return kFloat32ArrayStorageFormat;
  NOTREACHED();
  return kUint8ClampedArrayStorageFormat;
}

unsigned ImageData::StorageFormatDataSize(const String& storage_format_name) {
  if (storage_format_name == kUint8ClampedArrayStorageFormatName)
    return 1;
  if (storage_format_name == kUint16ArrayStorageFormatName)
    return 2;
  if (storage_format_name == kFloat32ArrayStorageFormatName)
    return 4;
  NOTREACHED();
  return 1;
}

DOMFloat32Array* ImageData::ConvertFloat16ArrayToFloat32Array(
    const uint16_t* f16_array,
    unsigned array_length) {
  if (!f16_array || array_length <= 0)
    return nullptr;

  DOMFloat32Array* f32_array = AllocateAndValidateFloat32Array(array_length);
  if (!f32_array)
    return nullptr;

  std::unique_ptr<SkColorSpaceXform> xform =
      SkColorSpaceXform::New(SkColorSpace::MakeSRGBLinear().get(),
                             SkColorSpace::MakeSRGBLinear().get());
  xform->apply(SkColorSpaceXform::ColorFormat::kRGBA_F32_ColorFormat,
               f32_array->Data(),
               SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat, f16_array,
               array_length, SkAlphaType::kUnpremul_SkAlphaType);
  return f32_array;
}

DOMArrayBufferView*
ImageData::ConvertPixelsFromCanvasPixelFormatToImageDataStorageFormat(
    WTF::ArrayBufferContents& content,
    CanvasPixelFormat pixel_format,
    ImageDataStorageFormat storage_format) {
  if (!content.SizeInBytes())
    return nullptr;

  // Uint16 is not supported as the storage format for ImageData created from a
  // canvas
  if (storage_format == kUint16ArrayStorageFormat)
    return nullptr;

  unsigned num_pixels = 0;
  DOMArrayBuffer* array_buffer = nullptr;
  DOMUint8ClampedArray* u8_array = nullptr;
  DOMFloat32Array* f32_array = nullptr;

  SkColorSpaceXform::ColorFormat src_color_format =
      SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
  SkColorSpaceXform::ColorFormat dst_color_format =
      SkColorSpaceXform::ColorFormat::kRGBA_F32_ColorFormat;
  std::unique_ptr<SkColorSpaceXform> xform =
      SkColorSpaceXform::New(SkColorSpace::MakeSRGBLinear().get(),
                             SkColorSpace::MakeSRGBLinear().get());

  // To speed up the conversion process, we use SkColorSpaceXform::apply()
  // wherever appropriate.
  switch (pixel_format) {
    case kRGBA8CanvasPixelFormat:
      num_pixels = content.SizeInBytes() / 4;
      switch (storage_format) {
        case kUint8ClampedArrayStorageFormat:
          array_buffer = DOMArrayBuffer::Create(content);
          return DOMUint8ClampedArray::Create(array_buffer, 0,
                                              array_buffer->ByteLength());
          break;
        case kFloat32ArrayStorageFormat:
          f32_array = AllocateAndValidateFloat32Array(num_pixels * 4);
          if (!f32_array)
            return nullptr;
          xform->apply(dst_color_format, f32_array->Data(), src_color_format,
                       content.Data(), num_pixels,
                       SkAlphaType::kUnpremul_SkAlphaType);
          return f32_array;
          break;
        default:
          NOTREACHED();
      }
      break;

    case kF16CanvasPixelFormat:
      num_pixels = content.SizeInBytes() / 8;
      src_color_format = SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat;

      switch (storage_format) {
        case kUint8ClampedArrayStorageFormat:
          u8_array = AllocateAndValidateUint8ClampedArray(num_pixels * 4);
          if (!u8_array)
            return nullptr;
          dst_color_format =
              SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
          xform->apply(dst_color_format, u8_array->Data(), src_color_format,
                       content.Data(), num_pixels,
                       SkAlphaType::kUnpremul_SkAlphaType);
          return u8_array;
          break;
        case kFloat32ArrayStorageFormat:
          f32_array = AllocateAndValidateFloat32Array(num_pixels * 4);
          if (!f32_array)
            return nullptr;
          dst_color_format =
              SkColorSpaceXform::ColorFormat::kRGBA_F32_ColorFormat;
          xform->apply(dst_color_format, f32_array->Data(), src_color_format,
                       content.Data(), num_pixels,
                       SkAlphaType::kUnpremul_SkAlphaType);
          return f32_array;
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

DOMArrayBufferBase* ImageData::BufferBase() const {
  if (data_)
    return data_->BufferBase();
  if (data_u16_)
    return data_u16_->BufferBase();
  if (data_f32_)
    return data_f32_->BufferBase();
  return nullptr;
}

CanvasColorParams ImageData::GetCanvasColorParams() {
  if (!RuntimeEnabledFeatures::ColorCanvasExtensionsEnabled())
    return CanvasColorParams();
  CanvasColorSpace color_space =
      ImageData::GetCanvasColorSpace(color_settings_.colorSpace());
  CanvasPixelFormat pixel_format = kRGBA8CanvasPixelFormat;
  if (color_settings_.storageFormat() != kUint8ClampedArrayStorageFormatName)
    pixel_format = kF16CanvasPixelFormat;
  return CanvasColorParams(color_space, pixel_format);
}

bool ImageData::ImageDataInCanvasColorSettings(
    const CanvasColorSpace& canvas_color_space,
    const CanvasPixelFormat& canvas_pixel_format,
    std::unique_ptr<uint8_t[]>& converted_pixels) {
  if (!data_ && !data_u16_ && !data_f32_)
    return false;

  // If canvas and image data are both in the same color space and pixel format
  // is 8-8-8-8, just return the embedded data.
  CanvasColorSpace image_data_color_space =
      ImageData::GetCanvasColorSpace(color_settings_.colorSpace());
  if (canvas_pixel_format == kRGBA8CanvasPixelFormat &&
      color_settings_.storageFormat() == kUint8ClampedArrayStorageFormatName) {
    if ((canvas_color_space == kLegacyCanvasColorSpace ||
         canvas_color_space == kSRGBCanvasColorSpace) &&
        (image_data_color_space == kLegacyCanvasColorSpace ||
         image_data_color_space == kSRGBCanvasColorSpace)) {
      memcpy(converted_pixels.get(), data_->Data(), data_->length());
      return true;
    }
  }

  // Otherwise, color convert the pixels.
  unsigned num_pixels = size_.Width() * size_.Height();
  unsigned data_length = num_pixels * 4;
  void* src_data = nullptr;
  std::unique_ptr<uint16_t[]> le_data;
  SkColorSpaceXform::ColorFormat src_color_format =
      SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
  if (data_) {
    src_data = static_cast<void*>(data_->Data());
    DCHECK(src_data);
  } else if (data_u16_) {
    src_data = static_cast<void*>(data_u16_->Data());
    DCHECK(src_data);
    // SkColorSpaceXform::apply expects U16 data to be in Big Endian byte
    // order, while srcData is always Little Endian. As we cannot consume
    // ImageData here, we change the byte order in a copy.
    le_data.reset(new uint16_t[data_length]());
    memcpy(le_data.get(), src_data, data_length * 2);
    uint16_t swap_value = 0;
    for (unsigned i = 0; i < data_length; i++) {
      swap_value = le_data[i];
      le_data[i] = swap_value >> 8 | swap_value << 8;
    }
    src_data = static_cast<void*>(le_data.get());
    DCHECK(src_data);
    src_color_format = SkColorSpaceXform::ColorFormat::kRGBA_U16_BE_ColorFormat;
  } else if (data_f32_) {
    src_data = static_cast<void*>(data_f32_->Data());
    DCHECK(src_data);
    src_color_format = SkColorSpaceXform::ColorFormat::kRGBA_F32_ColorFormat;
  } else {
    NOTREACHED();
  }

  sk_sp<SkColorSpace> src_color_space = nullptr;
  if (data_) {
    src_color_space =
        CanvasColorParams(image_data_color_space, kRGBA8CanvasPixelFormat)
            .GetSkColorSpaceForSkSurfaces();
  } else {
    src_color_space =
        CanvasColorParams(image_data_color_space, kF16CanvasPixelFormat)
            .GetSkColorSpaceForSkSurfaces();
  }

  sk_sp<SkColorSpace> dst_color_space =
      CanvasColorParams(canvas_color_space, canvas_pixel_format)
          .GetSkColorSpaceForSkSurfaces();
  SkColorSpaceXform::ColorFormat dst_color_format =
      SkColorSpaceXform::ColorFormat::kRGBA_8888_ColorFormat;
  if (canvas_pixel_format == kF16CanvasPixelFormat)
    dst_color_format = SkColorSpaceXform::ColorFormat::kRGBA_F16_ColorFormat;

  if (SkColorSpace::Equals(src_color_space.get(), dst_color_space.get()) &&
      src_color_format == dst_color_format)
    return static_cast<unsigned char*>(src_data);

  std::unique_ptr<SkColorSpaceXform> xform =
      SkColorSpaceXform::New(src_color_space.get(), dst_color_space.get());

  if (!xform->apply(dst_color_format, converted_pixels.get(), src_color_format,
                    src_data, num_pixels, SkAlphaType::kUnpremul_SkAlphaType))
    return false;
  return true;
}

bool ImageData::ImageDataInCanvasColorSettings(
    const CanvasColorParams& canvas_color_params,
    std::unique_ptr<uint8_t[]>& converted_pixels) {
  return ImageDataInCanvasColorSettings(canvas_color_params.color_space(),
                                        canvas_color_params.pixel_format(),
                                        converted_pixels);
}

void ImageData::Trace(Visitor* visitor) {
  visitor->Trace(data_);
  visitor->Trace(data_u16_);
  visitor->Trace(data_f32_);
  visitor->Trace(data_union_);
}

ImageData::ImageData(const IntSize& size,
                     DOMArrayBufferView* data,
                     const ImageDataColorSettings* color_settings)
    : size_(size) {
  DCHECK_GE(size.Width(), 0);
  DCHECK_GE(size.Height(), 0);
  DCHECK(data);

  data_ = nullptr;
  data_u16_ = nullptr;
  data_f32_ = nullptr;

  if (color_settings) {
    color_settings_.setColorSpace(color_settings->colorSpace());
    color_settings_.setStorageFormat(color_settings->storageFormat());
  }

  ImageDataStorageFormat storage_format =
      GetImageDataStorageFormat(color_settings_.storageFormat());

  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat:
      DCHECK(data->GetType() ==
             DOMArrayBufferView::ViewType::kTypeUint8Clamped);
      data_ = const_cast<DOMUint8ClampedArray*>(
          static_cast<const DOMUint8ClampedArray*>(data));
      DCHECK(data_);
      data_union_.setUint8ClampedArray(data_);
      SECURITY_CHECK(static_cast<unsigned>(size.Width() * size.Height() * 4) <=
                     data_->length());
      break;

    case kUint16ArrayStorageFormat:
      DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeUint16);
      data_u16_ =
          const_cast<DOMUint16Array*>(static_cast<const DOMUint16Array*>(data));
      DCHECK(data_u16_);
      data_union_.setUint16Array(data_u16_);
      SECURITY_CHECK(static_cast<unsigned>(size.Width() * size.Height() * 4) <=
                     data_u16_->length());
      break;

    case kFloat32ArrayStorageFormat:
      DCHECK(data->GetType() == DOMArrayBufferView::ViewType::kTypeFloat32);
      data_f32_ = const_cast<DOMFloat32Array*>(
          static_cast<const DOMFloat32Array*>(data));
      DCHECK(data_f32_);
      data_union_.setFloat32Array(data_f32_);
      SECURITY_CHECK(static_cast<unsigned>(size.Width() * size.Height() * 4) <=
                     data_f32_->length());
      break;

    default:
      NOTREACHED();
  }
}

}  // namespace blink
