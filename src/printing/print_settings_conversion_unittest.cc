// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_conversion.h"

#include "base/containers/contains.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "printing/print_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

const char kPrinterSettings[] = R"({
  "headerFooterEnabled": true,
  "title": "Test Doc",
  "url": "http://localhost/",
  "shouldPrintBackgrounds": false,
  "shouldPrintSelectionOnly": false,
  "mediaSize": {
    "height_microns": 297000,
    "width_microns": 210000
  },
  "marginsType": 0,
  "pageRange": [{
    "from": 1,
    "to": 1
  }],
  "collate": false,
  "copies": 1,
  "color": 2,
  "duplex": 0,
  "landscape": false,
  "deviceName": "printer",
  "scaleFactor": 100,
  "rasterizePDF": false,
  "rasterizePdfDpi": 150,
  "pagesPerSheet": 1,
  "dpiHorizontal": 300,
  "dpiVertical": 300,
  "previewModifiable": true,
  "sendUserInfo": true,
  "username": "username@domain.net",
  "pinValue": "0000"
})";

}  // namespace

TEST(PrintSettingsConversionTest, InvalidSettings) {
  base::Value value = base::test::ParseJson("{}");
  ASSERT_TRUE(value.is_dict());
  EXPECT_FALSE(PrintSettingsFromJobSettings(value.GetDict()));
}

TEST(PrintSettingsConversionTest, ConversionTest) {
  base::Value value = base::test::ParseJson(kPrinterSettings);
  ASSERT_TRUE(value.is_dict());
  auto& dict = value.GetDict();
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(settings->send_user_info());
  EXPECT_EQ("username@domain.net", settings->username());
  EXPECT_EQ("0000", settings->pin_value());
#endif
  EXPECT_EQ(settings->dpi_horizontal(), 300);
  EXPECT_EQ(settings->dpi_vertical(), 300);
  dict.Set("dpiVertical", 600);
  settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_EQ(settings->rasterize_pdf_dpi(), 150);
  EXPECT_EQ(settings->dpi_horizontal(), 300);
  EXPECT_EQ(settings->dpi_vertical(), 600);
  EXPECT_TRUE(dict.Remove("dpiVertical"));
  settings = PrintSettingsFromJobSettings(dict);
  EXPECT_FALSE(settings);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(PrintSettingsConversionTest, DontSendUsername) {
  base::Value value = base::test::ParseJson(kPrinterSettings);
  ASSERT_TRUE(value.is_dict());
  auto& dict = value.GetDict();
  dict.Set(kSettingSendUserInfo, false);
  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_FALSE(settings->send_user_info());
  EXPECT_EQ("", settings->username());
}
#endif

#if BUILDFLAG(IS_CHROMEOS) || (BUILDFLAG(IS_LINUX) && defined(USE_CUPS))
TEST(PrintSettingsConversionTest, FilterNonJobSettings) {
  base::Value value = base::test::ParseJson(kPrinterSettings);
  ASSERT_TRUE(value.is_dict());
  auto& dict = value.GetDict();

  {
    base::Value::Dict advanced_attributes;
    advanced_attributes.Set("printer-info", "yada");
    advanced_attributes.Set("printer-make-and-model", "yada");
    advanced_attributes.Set("system_driverinfo", "yada");
    advanced_attributes.Set("Foo", "Bar");
    dict.Set(kSettingAdvancedSettings, std::move(advanced_attributes));
  }

  std::unique_ptr<PrintSettings> settings = PrintSettingsFromJobSettings(dict);
  ASSERT_TRUE(settings);
  EXPECT_EQ(settings->advanced_settings().size(), 1u);
  ASSERT_TRUE(base::Contains(settings->advanced_settings(), "Foo"));
  EXPECT_EQ(settings->advanced_settings().at("Foo"), base::Value("Bar"));
}
#endif  // BUILDFLAG(IS_CHROMEOS) || (BUILDFLAG(IS_LINUX) && defined(USE_CUPS))

}  // namespace printing
