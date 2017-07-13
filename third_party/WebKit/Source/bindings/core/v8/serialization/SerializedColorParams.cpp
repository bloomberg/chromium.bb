// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/serialization/SerializedColorParams.h"

namespace blink {

SerializedColorParams::SerializedColorParams()
    : color_space_(SerializedColorSpace::kLegacy),
      pixel_format_(SerializedPixelFormat::kRGBA8),
      storage_format_(SerializedImageDataStorageFormat::kUint8Clamped) {}

SerializedColorParams::SerializedColorParams(CanvasColorParams color_params) {
  switch (color_params.color_space()) {
    case kLegacyCanvasColorSpace:
      color_space_ = SerializedColorSpace::kLegacy;
      break;
    case kSRGBCanvasColorSpace:
      color_space_ = SerializedColorSpace::kSRGB;
      break;
    case kRec2020CanvasColorSpace:
      color_space_ = SerializedColorSpace::kRec2020;
      break;
    case kP3CanvasColorSpace:
      color_space_ = SerializedColorSpace::kP3;
      break;
  }

  switch (color_params.pixel_format()) {
    case kRGBA8CanvasPixelFormat:
    case kRGB10A2CanvasPixelFormat:
    case kRGBA12CanvasPixelFormat:
      pixel_format_ = SerializedPixelFormat::kRGBA8;
      break;
    case kF16CanvasPixelFormat:
      pixel_format_ = SerializedPixelFormat::kF16;
      break;
  }
  storage_format_ = SerializedImageDataStorageFormat::kUint8Clamped;
}

SerializedColorParams::SerializedColorParams(
    CanvasColorParams color_params,
    ImageDataStorageFormat storage_format)
    : SerializedColorParams(color_params) {
  switch (storage_format) {
    case kUint8ClampedArrayStorageFormat:
      storage_format_ = SerializedImageDataStorageFormat::kUint8Clamped;
      break;
    case kUint16ArrayStorageFormat:
      storage_format_ = SerializedImageDataStorageFormat::kUint16;
      break;
    case kFloat32ArrayStorageFormat:
      storage_format_ = SerializedImageDataStorageFormat::kFloat32;
      break;
  }
}

SerializedColorParams::SerializedColorParams(
    SerializedColorSpace color_space,
    SerializedPixelFormat pixel_format,
    SerializedImageDataStorageFormat storage_format) {
  SetSerializedColorSpace(color_space);
  SetSerializedPixelFormat(pixel_format);
  SetSerializedImageDataStorageFormat(storage_format);
}

CanvasColorParams SerializedColorParams::GetCanvasColorParams() const {
  CanvasColorSpace color_space = kLegacyCanvasColorSpace;
  switch (color_space_) {
    case SerializedColorSpace::kLegacy:
      color_space = kLegacyCanvasColorSpace;
      break;
    case SerializedColorSpace::kSRGB:
      color_space = kSRGBCanvasColorSpace;
      break;
    case SerializedColorSpace::kRec2020:
      color_space = kRec2020CanvasColorSpace;
      break;
    case SerializedColorSpace::kP3:
      color_space = kP3CanvasColorSpace;
      break;
  }

  CanvasPixelFormat pixel_format = kRGBA8CanvasPixelFormat;
  if (pixel_format_ == SerializedPixelFormat::kF16)
    pixel_format = kF16CanvasPixelFormat;
  return CanvasColorParams(color_space, pixel_format);
}

CanvasColorSpace SerializedColorParams::GetColorSpace() const {
  return GetCanvasColorParams().color_space();
}

ImageDataStorageFormat SerializedColorParams::GetStorageFormat() const {
  switch (storage_format_) {
    case SerializedImageDataStorageFormat::kUint8Clamped:
      return kUint8ClampedArrayStorageFormat;
    case SerializedImageDataStorageFormat::kUint16:
      return kUint16ArrayStorageFormat;
    case SerializedImageDataStorageFormat::kFloat32:
      return kFloat32ArrayStorageFormat;
  }
  NOTREACHED();
  return kUint8ClampedArrayStorageFormat;
}

void SerializedColorParams::SetSerializedColorSpace(
    SerializedColorSpace color_space) {
  color_space_ = color_space;
}

void SerializedColorParams::SetSerializedPixelFormat(
    SerializedPixelFormat pixel_format) {
  pixel_format_ = pixel_format;
}

void SerializedColorParams::SetSerializedImageDataStorageFormat(
    SerializedImageDataStorageFormat storage_format) {
  storage_format_ = storage_format;
}

SerializedColorSpace SerializedColorParams::GetSerializedColorSpace() const {
  return color_space_;
}

SerializedPixelFormat SerializedColorParams::GetSerializedPixelFormat() const {
  return pixel_format_;
}

SerializedImageDataStorageFormat
SerializedColorParams::GetSerializedImageDataStorageFormat() const {
  return storage_format_;
}

}  // namespace blink
