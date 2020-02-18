// Copyright 2019 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/fpdf_sysfontinfo.h"

#include "testing/embedder_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

extern "C" {

void FakeRelease(FPDF_SYSFONTINFO* pThis) {}
void FakeEnumFonts(FPDF_SYSFONTINFO* pThis, void* pMapper) {}

void* FakeMapFont(FPDF_SYSFONTINFO* pThis,
                  int weight,
                  FPDF_BOOL bItalic,
                  int charset,
                  int pitch_family,
                  const char* face,
                  FPDF_BOOL* bExact) {
  // Any non-null return will do.
  return pThis;
}

void* FakeGetFont(FPDF_SYSFONTINFO* pThis, const char* face) {
  // Any non-null return will do.
  return pThis;
}

unsigned long FakeGetFontData(FPDF_SYSFONTINFO* pThis,
                              void* hFont,
                              unsigned int table,
                              unsigned char* buffer,
                              unsigned long buf_size) {
  return 0;
}

unsigned long FakeGetFaceName(FPDF_SYSFONTINFO* pThis,
                              void* hFont,
                              char* buffer,
                              unsigned long buf_size) {
  return 0;
}

int FakeGetFontCharset(FPDF_SYSFONTINFO* pThis, void* hFont) {
  return 1;
}

void FakeDeleteFont(FPDF_SYSFONTINFO* pThis, void* hFont) {}

}  // extern "C"

class FPDFUnavailableSysFontInfoEmbedderTest : public EmbedderTest {
 public:
  FPDFUnavailableSysFontInfoEmbedderTest() = default;
  ~FPDFUnavailableSysFontInfoEmbedderTest() override = default;

  void SetUp() override {
    EmbedderTest::SetUp();
    font_info_.version = 1;
    font_info_.Release = FakeRelease;
    font_info_.EnumFonts = FakeEnumFonts;
    font_info_.MapFont = FakeMapFont;
    font_info_.GetFont = FakeGetFont;
    font_info_.GetFontData = FakeGetFontData;
    font_info_.GetFaceName = FakeGetFaceName;
    font_info_.GetFontCharset = FakeGetFontCharset;
    font_info_.DeleteFont = FakeDeleteFont;
    FPDF_SetSystemFontInfo(&font_info_);
  }

  FPDF_SYSFONTINFO font_info_;
};

class FPDFSysFontInfoEmbedderTest : public EmbedderTest {
 public:
  FPDFSysFontInfoEmbedderTest() = default;
  ~FPDFSysFontInfoEmbedderTest() override = default;

  void SetUp() override {
    EmbedderTest::SetUp();
    font_info_ = FPDF_GetDefaultSystemFontInfo();
    ASSERT_TRUE(font_info_);
    FPDF_SetSystemFontInfo(font_info_);
  }

  void TearDown() override {
    EmbedderTest::TearDown();
    FPDF_FreeDefaultSystemFontInfo(font_info_);
  }

  FPDF_SYSFONTINFO* font_info_;
};

}  // namespace

TEST_F(FPDFUnavailableSysFontInfoEmbedderTest, Bug_972518) {
  ASSERT_TRUE(OpenDocument("bug_972518.pdf"));
  ASSERT_EQ(1, FPDF_GetPageCount(document()));

  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  UnloadPage(page);
}

TEST_F(FPDFSysFontInfoEmbedderTest, DefaultSystemFontInfo) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  ASSERT_EQ(1, FPDF_GetPageCount(document()));

  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  {
    // Not checking the rendering because it will depend on the fonts installed.
    ScopedFPDFBitmap bitmap = RenderPage(page);
    ASSERT_EQ(200, FPDFBitmap_GetWidth(bitmap.get()));
    ASSERT_EQ(200, FPDFBitmap_GetHeight(bitmap.get()));
  }

  UnloadPage(page);
}
