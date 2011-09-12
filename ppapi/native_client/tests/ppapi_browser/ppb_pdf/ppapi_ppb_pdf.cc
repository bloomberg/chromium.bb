// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/c/private/ppb_pdf.h"

namespace {

const bool kCaseSensitive = true;
const bool kCaseInsensitive = false;

const PPB_PDF* PPBPDF() {
  return reinterpret_cast<const PPB_PDF*>(
      GetBrowserInterface(PPB_PDF_INTERFACE));
}

void TestGetLocalizedString() {
  // Create a vertical scrollbar.
  PP_Var string = PPBPDF()->GetLocalizedString(
      pp_instance(),
      PP_RESOURCESTRING_PDFGETPASSWORD);
  EXPECT(string.type == PP_VARTYPE_STRING);

  TEST_PASSED;
}

void TestGetResourceImage() {
  // Get an image.
  PP_Resource image = PPBPDF()->GetResourceImage(
      pp_instance(),
      PP_RESOURCEIMAGE_PDF_BUTTON_FTP);
  EXPECT(image != kInvalidResource);
  EXPECT(PPBImageData()->IsImageData(image));

  PPBCore()->ReleaseResource(image);

  return TEST_PASSED;
}

void TestGetFontFileWithFallback() {
  PP_FontDescription_Dev description;
  PP_Resource font_file = PPBPDF()->GetFontFileWithFallback(
      pp_instance(),
      &description,
      PP_PRIVATEFONTCHARSET_ANSI);
  EXPECT(font_file == kInvalidResource);

  TEST_PASSED;
}

void TestGetFontTableForPrivateFontFile() {
  uint32_t output_length;
  bool success = PPBPDF()->GetFontTableForPrivateFontFile(
      kInvalidResource,  // font
      0,                 // uint32_t table
      NULL,              // void* output
      &output_length);
  EXPECT(!success);

  return TEST_PASSED;
}

typedef std::basic_string<unsigned short> Utf16String;

Utf16String CreateWString(const std::string& string) {
  Utf16String utf16String = Utf16String();
  for (std::string::const_iterator it = string.begin();
      it != string.end(); ++it) {
    utf16String.push_back(static_cast<unsigned short>(*it));
  }
  return utf16String;
}

void TestSearchString() {
  int count;
  PP_PrivateFindResult* results;
  Utf16String string = CreateWString("Hello World");
  {
    Utf16String term = CreateWString("Hello");
    PPBPDF()->SearchString(
        pp_instance(),
        string.c_str(),
        term.c_str(),
        kCaseSensitive,
        &results,
        &count);
    EXPECT(count == 1);
    EXPECT(results[0].start_index == 0 && results[0].length == 5);
    PPBMemoryDev()->MemFree(results);
  }
  {
    Utf16String term = CreateWString("l");
    PPBPDF()->SearchString(
        pp_instance(),
        string.c_str(),
        term.c_str(),
        kCaseSensitive,
        &results,
        &count);
    EXPECT(count == 3);
    EXPECT(results[0].start_index == 2 && results[0].length == 1);
    EXPECT(results[1].start_index == 3 && results[1].length == 1);
    EXPECT(results[2].start_index == 9 && results[2].length == 1);
    PPBMemoryDev()->MemFree(results);
  }
  {
    Utf16String term = CreateWString("Hel");
    PPBPDF()->SearchString(
        pp_instance(),
        string.c_str(),
        term.c_str(),
        kCaseSensitive,
        &results,
        &count);
    EXPECT(count == 1);
    EXPECT(results[0].start_index == 0 && results[0].length == 3);
    PPBMemoryDev()->MemFree(results);
  }
  {
    Utf16String term = CreateWString("h");
    PPBPDF()->SearchString(
        pp_instance(),
        string.c_str(),
        term.c_str(),
        kCaseInsensitive,
        &results,
        &count);
    EXPECT(count == 1);
    EXPECT(results[0].start_index == 0 && results[0].length == 1);
    PPBMemoryDev()->MemFree(results);
  }
  {
    Utf16String term = CreateWString("h");
    PPBPDF()->SearchString(
        pp_instance(),
        string.c_str(),
        term.c_str(),
        kCaseSensitive,
        &results,
        &count);
    EXPECT(count == 0);
    PPBMemoryDev()->MemFree(results);
  }
  {
    Utf16String term = CreateWString("");
    PPBPDF()->SearchString(
        pp_instance(),
        string.c_str(),
        term.c_str(),
        kCaseInsensitive,
        &results,
        &count);
    EXPECT(count == 0);
    PPBMemoryDev()->MemFree(results);
  }

  TEST_PASSED;
}

void TestDidStartLoading() {
  PPBPDF()->DidStartLoading(pp_instance());

  return TEST_PASSED;
}

void TestDidStopLoading() {
  PPBPDF()->DidStopLoading(pp_instance());

  return TEST_PASSED;
}

void TestSetContentRestriction() {
  PPBPDF()->SetContentRestriction(pp_instance(), 0);

  return TEST_PASSED;
}

void TestHistogramPDFPageCount() {
  PPBPDF()->HistogramPDFPageCount(1);

  return TEST_PASSED;
}

void TestUserMetricsRecordAction() {
  const char* kString = "Action!";
  PP_Var action = PPBVar()->VarFromUtf8(pp_module(),
                                        kString,
                                        strlen(kString));
  PPBPDF()->UserMetricsRecordAction(action);

  PPBVar()->Release(action);

  return TEST_PASSED;
}

void TestHasUnsupportedFeature() {
  PPBPDF()->HasUnsupportedFeature(pp_instance());

  return TEST_PASSED;
}

void TestSaveAs() {
  // This test pops up a file 'SaveAs' dialog, which will cause
  // automated browser tests to hang. Un-comment to test manually.
/*
  PPBPDF()->SaveAs(pp_instance());
*/

  return TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestGetLocalizedString",
               TestGetLocalizedString);
  RegisterTest("TestGetResourceImage",
               TestGetResourceImage);
  RegisterTest("TestGetFontFileWithFallback",
               TestGetFontFileWithFallback);
  RegisterTest("TestGetFontTableForPrivateFontFile",
               TestGetFontTableForPrivateFontFile);
  RegisterTest("TestSearchString",
               TestSearchString);
  RegisterTest("TestDidStartLoading",
               TestDidStartLoading);
  RegisterTest("TestDidStopLoading",
               TestDidStopLoading);
  RegisterTest("TestSetContentRestriction",
               TestSetContentRestriction);
  RegisterTest("TestHistogramPDFPageCount",
               TestHistogramPDFPageCount);
  RegisterTest("TestUserMetricsRecordAction",
               TestUserMetricsRecordAction);
  RegisterTest("TestHasUnsupportedFeature",
               TestHasUnsupportedFeature);
  RegisterTest("TestSaveAs",
                TestSaveAs);
}

void SetupPluginInterfaces() {
}

