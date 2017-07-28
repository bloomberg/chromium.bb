// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/FontTestHelpers.h"

#include "platform/SharedBuffer.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontCustomPlatformData.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontSelector.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"

namespace blink {
namespace testing {

namespace {

class TestFontSelector : public FontSelector {
 public:
  static TestFontSelector* Create(const String& path) {
    RefPtr<SharedBuffer> font_buffer = testing::ReadFromFile(path);
    String ots_parse_message;
    return new TestFontSelector(
        FontCustomPlatformData::Create(font_buffer.Get(), ots_parse_message));
  }

  ~TestFontSelector() override {}

  PassRefPtr<FontData> GetFontData(const FontDescription& font_description,
                                   const AtomicString& family_name) override {
    FontPlatformData platform_data = custom_platform_data_->GetFontPlatformData(
        font_description.EffectiveFontSize(),
        font_description.IsSyntheticBold(),
        font_description.IsSyntheticItalic(), font_description.Orientation());
    return SimpleFontData::Create(platform_data, CustomFontData::Create());
  }

  void WillUseFontData(const FontDescription&,
                       const AtomicString& family_name,
                       const String& text) override {}
  void WillUseRange(const FontDescription&,
                    const AtomicString& family_name,
                    const FontDataForRangeSet&) override {}

  unsigned Version() const override { return 0; }
  void FontCacheInvalidated() override {}
  void ReportNotDefGlyph() const override {}

 private:
  TestFontSelector(PassRefPtr<FontCustomPlatformData> custom_platform_data)
      : custom_platform_data_(std::move(custom_platform_data)) {
    DCHECK(custom_platform_data_);
  }

  RefPtr<FontCustomPlatformData> custom_platform_data_;
};

}  // namespace

Font CreateTestFont(const AtomicString& family_name,
                    const String& font_path,
                    float size) {
  FontFamily family;
  family.SetFamily(family_name);

  FontDescription font_description;
  font_description.SetFamily(family);
  font_description.SetSpecifiedSize(size);
  font_description.SetComputedSize(size);

  Font font(font_description);
  font.Update(TestFontSelector::Create(font_path));
  return font;
}

}  // namespace testing
}  // namespace blink
