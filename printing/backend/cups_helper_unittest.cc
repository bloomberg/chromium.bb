// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_helper.h"
#include "printing/backend/print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PrintBackendCupsHelperTest, TestPpdParsingNoColorDuplexLongEdge) {
  std::string test_ppd_data;
  test_ppd_data.append(
      "*PPD-Adobe: \"4.3\"\n\n"
      "*OpenGroup: General/General\n\n"
      "*OpenUI *ColorModel/Color Model: PickOne\n"
      "*DefaultColorModel: Gray\n"
      "*ColorModel Gray/Grayscale: \""
      "<</cupsColorSpace 0/cupsColorOrder 0>>"
      "setpagedevice\"\n"
      "*ColorModel Black/Inverted Grayscale: \""
      "<</cupsColorSpace 3/cupsColorOrder 0>>"
      "setpagedevice\"\n"
      "*CloseUI: *ColorModel\n"
      "*OpenUI *Duplex/2-Sided Printing: PickOne\n"
      "*DefaultDuplex: DuplexTumble\n"
      "*Duplex None/Off: \"<</Duplex false>>"
      "setpagedevice\"\n"
      "*Duplex DuplexNoTumble/LongEdge: \""
      "<</Duplex true/Tumble false>>setpagedevice\"\n"
      "*Duplex DuplexTumble/ShortEdge: \""
      "<</Duplex true/Tumble true>>setpagedevice\"\n"
      "*CloseUI: *Duplex\n\n"
      "*CloseGroup: General\n");

  printing::PrinterSemanticCapsAndDefaults caps;
  EXPECT_TRUE(printing::parsePpdCapabilities("test", test_ppd_data, &caps));
  EXPECT_FALSE(caps.color_capable);
  EXPECT_FALSE(caps.color_default);
  EXPECT_TRUE(caps.duplex_capable);
  EXPECT_EQ(caps.duplex_default, printing::LONG_EDGE);
}

// Test duplex detection code, which regressed in http://crbug.com/103999.
TEST(PrintBackendCupsHelperTest, TestPpdParsingNoColorDuplexSimples) {
  std::string test_ppd_data;
  test_ppd_data.append(
      "*PPD-Adobe: \"4.3\"\n\n"
      "*OpenGroup: General/General\n\n"
      "*OpenUI *Duplex/Double-Sided Printing: PickOne\n"
      "*DefaultDuplex: None\n"
      "*Duplex None/Off: "
      "\"<</Duplex false>>setpagedevice\"\n"
      "*Duplex DuplexNoTumble/Long Edge (Standard): "
      "\"<</Duplex true/Tumble false>>setpagedevice\"\n"
      "*Duplex DuplexTumble/Short Edge (Flip): "
      "\"<</Duplex true/Tumble true>>setpagedevice\"\n"
      "*CloseUI: *Duplex\n\n"
      "*CloseGroup: General\n");

  printing::PrinterSemanticCapsAndDefaults caps;
  EXPECT_TRUE(printing::parsePpdCapabilities("test", test_ppd_data, &caps));
  EXPECT_FALSE(caps.color_capable);
  EXPECT_FALSE(caps.color_default);
  EXPECT_TRUE(caps.duplex_capable);
  EXPECT_EQ(caps.duplex_default, printing::SIMPLEX);
}

TEST(PrintBackendCupsHelperTest, TestPpdParsingNoColorNoDuplex) {
  std::string test_ppd_data;
  test_ppd_data.append(
      "*PPD-Adobe: \"4.3\"\n\n"
      "*OpenGroup: General/General\n\n"
      "*OpenUI *ColorModel/Color Model: PickOne\n"
      "*DefaultColorModel: Gray\n"
      "*ColorModel Gray/Grayscale: \""
      "<</cupsColorSpace 0/cupsColorOrder 0>>"
      "setpagedevice\"\n"
      "*ColorModel Black/Inverted Grayscale: \""
      "<</cupsColorSpace 3/cupsColorOrder 0>>"
      "setpagedevice\"\n"
      "*CloseUI: *ColorModel\n"
      "*CloseGroup: General\n");

  printing::PrinterSemanticCapsAndDefaults caps;
  EXPECT_TRUE(printing::parsePpdCapabilities("test", test_ppd_data, &caps));
  EXPECT_FALSE(caps.color_capable);
  EXPECT_FALSE(caps.color_default);
  EXPECT_FALSE(caps.duplex_capable);
  EXPECT_EQ(caps.duplex_default, printing::UNKNOWN_DUPLEX_MODE);
}

TEST(PrintBackendCupsHelperTest, TestPpdParsingColorTrueDuplexLongEdge) {
  std::string test_ppd_data;
  test_ppd_data.append(
      "*PPD-Adobe: \"4.3\"\n\n"
      "*ColorDevice: True\n"
      "*DefaultColorSpace: CMYK\n\n"
      "*OpenGroup: General/General\n\n"
      "*OpenUI *ColorModel/Color Model: PickOne\n"
      "*DefaultColorModel: CMYK\n"
      "*ColorModel CMYK/Color: "
      "\"(cmyk) RCsetdevicecolor\"\n"
      "*ColorModel Gray/Black and White: "
      "\"(gray) RCsetdevicecolor\"\n"
      "*CloseUI: *ColorModel\n"
      "*OpenUI *Duplex/2-Sided Printing: PickOne\n"
      "*DefaultDuplex: DuplexTumble\n"
      "*Duplex None/Off: \"<</Duplex false>>"
      "setpagedevice\"\n"
      "*Duplex DuplexNoTumble/LongEdge: \""
      "<</Duplex true/Tumble false>>setpagedevice\"\n"
      "*Duplex DuplexTumble/ShortEdge: \""
      "<</Duplex true/Tumble true>>setpagedevice\"\n"
      "*CloseUI: *Duplex\n\n"
      "*CloseGroup: General\n");

  printing::PrinterSemanticCapsAndDefaults caps;
  EXPECT_TRUE(printing::parsePpdCapabilities("test", test_ppd_data, &caps));
  EXPECT_TRUE(caps.color_capable);
  EXPECT_TRUE(caps.color_default);
  EXPECT_TRUE(caps.duplex_capable);
  EXPECT_EQ(caps.duplex_default, printing::LONG_EDGE);
}

TEST(PrintBackendCupsHelperTest, TestPpdParsingColorFalseDuplexLongEdge) {
  std::string test_ppd_data;
  test_ppd_data.append(
      "*PPD-Adobe: \"4.3\"\n\n"
      "*ColorDevice: True\n"
      "*DefaultColorSpace: CMYK\n\n"
      "*OpenGroup: General/General\n\n"
      "*OpenUI *ColorModel/Color Model: PickOne\n"
      "*DefaultColorModel: Grayscale\n"
      "*ColorModel Color/Color: "
      "\"%% FoomaticRIPOptionSetting: ColorModel=Color\"\n"
      "*FoomaticRIPOptionSetting ColorModel=Color: "
      "\"JCLDatamode=Color GSCmdLine=Color\"\n"
      "*ColorModel Grayscale/Grayscale: "
      "\"%% FoomaticRIPOptionSetting: ColorModel=Grayscale\"\n"
      "*FoomaticRIPOptionSetting ColorModel=Grayscale: "
      "\"JCLDatamode=Grayscale GSCmdLine=Grayscale\"\n"
      "*CloseUI: *ColorModel\n"
      "*OpenUI *Duplex/2-Sided Printing: PickOne\n"
      "*DefaultDuplex: DuplexTumble\n"
      "*Duplex None/Off: \"<</Duplex false>>"
      "setpagedevice\"\n"
      "*Duplex DuplexNoTumble/LongEdge: \""
      "<</Duplex true/Tumble false>>setpagedevice\"\n"
      "*Duplex DuplexTumble/ShortEdge: \""
      "<</Duplex true/Tumble true>>setpagedevice\"\n"
      "*CloseUI: *Duplex\n\n"
      "*CloseGroup: General\n");

  printing::PrinterSemanticCapsAndDefaults caps;
  EXPECT_TRUE(printing::parsePpdCapabilities("test", test_ppd_data, &caps));
  EXPECT_TRUE(caps.color_capable);
  EXPECT_FALSE(caps.color_default);
  EXPECT_TRUE(caps.duplex_capable);
  EXPECT_EQ(caps.duplex_default, printing::LONG_EDGE);
}
