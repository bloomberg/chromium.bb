// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SerializedColorParams_h
#define SerializedColorParams_h

#include "core/html/ImageData.h"
#include "platform/graphics/CanvasColorParams.h"

namespace blink {

// This enumeration specifies the extra tags used to specify the color settings
// of the serialized/deserialized ImageData and ImageBitmap objects.
enum class ImageSerializationTag : uint32_t {
  kEndTag = 0,
  // followed by a SerializedColorSpace enum.
  kCanvasColorSpaceTag = 1,
  // followed by a SerializedPixelFormat enum, used only for ImageBitmap.
  kCanvasPixelFormatTag = 2,
  // followed by a SerializedImageDataStorageFormat enum, used only for
  // ImageData.
  kImageDataStorageFormatTag = 3,
  // followed by 1 if the image is origin clean and zero otherwise.
  kOriginClean = 4,
  // followed by 1 if the image is premultiplied and zero otherwise.
  kIsPremultiplied = 5,
  kLast = kIsPremultiplied,
};

// This enumeration specifies the values used to serialize CanvasColorSpace.
enum class SerializedColorSpace : uint32_t {
  kLegacy = 0,
  kSRGB = 1,
  kRec2020 = 2,
  kP3 = 3,
  kLast = kP3,
};

// This enumeration specifies the values used to serialize CanvasPixelFormat.
enum class SerializedPixelFormat : uint32_t {
  kRGBA8 = 0,
  kRGB10A2 = 1,
  kRGBA12 = 2,
  kF16 = 3,
  kLast = kF16,
};

// This enumeration specifies the values used to serialize
// ImageDataStorageFormat.
enum class SerializedImageDataStorageFormat : uint32_t {
  kUint8Clamped = 0,
  kUint16 = 1,
  kFloat32 = 2,
  kLast = kFloat32,
};

class SerializedColorParams {
 public:
  SerializedColorParams();
  SerializedColorParams(CanvasColorParams);
  SerializedColorParams(CanvasColorParams, ImageDataStorageFormat);
  SerializedColorParams(SerializedColorSpace,
                        SerializedPixelFormat,
                        SerializedImageDataStorageFormat);

  CanvasColorParams GetCanvasColorParams() const;
  CanvasColorSpace GetColorSpace() const;
  ImageDataStorageFormat GetStorageFormat() const;

  void SetSerializedColorSpace(SerializedColorSpace);
  void SetSerializedPixelFormat(SerializedPixelFormat);
  void SetSerializedImageDataStorageFormat(SerializedImageDataStorageFormat);

  SerializedColorSpace GetSerializedColorSpace() const;
  SerializedPixelFormat GetSerializedPixelFormat() const;
  SerializedImageDataStorageFormat GetSerializedImageDataStorageFormat() const;

 private:
  SerializedColorSpace color_space_;
  SerializedPixelFormat pixel_format_;
  SerializedImageDataStorageFormat storage_format_;
};

}  // namespace blink

#endif
