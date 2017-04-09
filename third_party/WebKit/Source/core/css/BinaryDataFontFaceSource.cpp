// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/BinaryDataFontFaceSource.h"

#include "platform/SharedBuffer.h"
#include "platform/fonts/FontCustomPlatformData.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"

namespace blink {

BinaryDataFontFaceSource::BinaryDataFontFaceSource(SharedBuffer* data,
                                                   String& ots_parse_message)
    : custom_platform_data_(
          FontCustomPlatformData::Create(data, ots_parse_message)) {}

BinaryDataFontFaceSource::~BinaryDataFontFaceSource() {}

bool BinaryDataFontFaceSource::IsValid() const {
  return custom_platform_data_.Get();
}

PassRefPtr<SimpleFontData> BinaryDataFontFaceSource::CreateFontData(
    const FontDescription& font_description) {
  return SimpleFontData::Create(
      custom_platform_data_->GetFontPlatformData(
          font_description.EffectiveFontSize(),
          font_description.IsSyntheticBold(),
          font_description.IsSyntheticItalic(), font_description.Orientation(),
          font_description.VariationSettings()),
      CustomFontData::Create());
}

}  // namespace blink
