// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BinaryDataFontFaceSource_h
#define BinaryDataFontFaceSource_h

#include "base/memory/scoped_refptr.h"
#include "core/css/CSSFontFaceSource.h"

namespace blink {

class FontCustomPlatformData;
class SharedBuffer;

class BinaryDataFontFaceSource final : public CSSFontFaceSource {
 public:
  BinaryDataFontFaceSource(SharedBuffer*, String&);
  ~BinaryDataFontFaceSource() override;
  bool IsValid() const override;

 private:
  scoped_refptr<SimpleFontData> CreateFontData(
      const FontDescription&,
      const FontSelectionCapabilities&) override;

  scoped_refptr<FontCustomPlatformData> custom_platform_data_;
};

}  // namespace blink

#endif
