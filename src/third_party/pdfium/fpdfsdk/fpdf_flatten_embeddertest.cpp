// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "public/fpdf_flatten.h"
#include "public/fpdfview.h"
#include "testing/embedder_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FPDFFlattenEmbedderTest : public EmbedderTest {};

}  // namespace

TEST_F(FPDFFlattenEmbedderTest, FlatNothing) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);
  EXPECT_EQ(FLATTEN_NOTHINGTODO, FPDFPage_Flatten(page, FLAT_NORMALDISPLAY));
  UnloadPage(page);
}

TEST_F(FPDFFlattenEmbedderTest, FlatNormal) {
  ASSERT_TRUE(OpenDocument("annotiter.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);
  EXPECT_EQ(FLATTEN_SUCCESS, FPDFPage_Flatten(page, FLAT_NORMALDISPLAY));
  UnloadPage(page);
}

TEST_F(FPDFFlattenEmbedderTest, FlatPrint) {
  ASSERT_TRUE(OpenDocument("annotiter.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);
  EXPECT_EQ(FLATTEN_SUCCESS, FPDFPage_Flatten(page, FLAT_PRINT));
  UnloadPage(page);
}

TEST_F(FPDFFlattenEmbedderTest, BUG_861842) {
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#if defined(OS_WIN)
  constexpr char kCheckboxChecksum[] = "ec7d1600d179aca614f2231c1f77ccb9";
#elif defined(OS_APPLE)
  constexpr char kCheckboxChecksum[] = "c7c687f93fb34a4174bdae33535e0627";
#else
  constexpr char kCheckboxChecksum[] = "b8aecddfece463096d51596537a20b61";
#endif
#else
#if defined(OS_WIN)
  constexpr char kCheckboxChecksum[] = "95fba3cb7bce7e0d3c94279f60984e17";
#elif defined(OS_APPLE)
  constexpr char kCheckboxChecksum[] = "6aafcb2d98da222964bcdbf5aa1f4f1f";
#else
  constexpr char kCheckboxChecksum[] = "594265790b81df2d93120d33b72a6ada";
#endif
#endif  // defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)

  ASSERT_TRUE(OpenDocument("bug_861842.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  ScopedFPDFBitmap bitmap = RenderLoadedPageWithFlags(page, FPDF_ANNOT);
  CompareBitmap(bitmap.get(), 100, 120, kCheckboxChecksum);

  EXPECT_EQ(FLATTEN_SUCCESS, FPDFPage_Flatten(page, FLAT_PRINT));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  UnloadPage(page);

  // TODO(crbug.com/861842): This should not render blank.
  constexpr char kBlankPageHash[] = "48400809c3862dae64b0cd00d51057a4";
  VerifySavedDocument(100, 120, kBlankPageHash);
}

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_BUG_889099 DISABLED_BUG_889099
#else
#define MAYBE_BUG_889099 BUG_889099
#endif
TEST_F(FPDFFlattenEmbedderTest, MAYBE_BUG_889099) {
#if defined(OS_WIN)
  constexpr char kPageHash[] = "8c6e1dab0a15072f2c9c0ca240fdc739";
  constexpr char kFlattenedPageHash[] = "9fb932ce7f370c0e68eec0a5d4d76271";
#elif defined(OS_APPLE)
  constexpr char kPageHash[] = "d43f54c60b325726392a558f861402a9";
  constexpr char kFlattenedPageHash[] = "627f143efb920a5e7ddd311e963b9c66";
#else
  constexpr char kPageHash[] = "51f35e80dbc8a69a024b5a02aa64d463";
  constexpr char kFlattenedPageHash[] = "ef01f57507662ec9aef7cc7cff92f96c";
#endif

  ASSERT_TRUE(OpenDocument("bug_889099.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // The original document has a malformed media box; the height is -400.
  ScopedFPDFBitmap bitmap = RenderLoadedPageWithFlags(page, FPDF_ANNOT);
  CompareBitmap(bitmap.get(), 300, 400, kPageHash);

  EXPECT_EQ(FLATTEN_SUCCESS, FPDFPage_Flatten(page, FLAT_PRINT));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  UnloadPage(page);

  VerifySavedDocument(300, 400, kFlattenedPageHash);
}

TEST_F(FPDFFlattenEmbedderTest, BUG_890322) {
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
  constexpr char kChecksum[] = "793689536cf64fe792c2f241888c0cf3";
#else
  constexpr char kChecksum[] = "6c674642154408e877d88c6c082d67e9";
#endif
  ASSERT_TRUE(OpenDocument("bug_890322.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  ScopedFPDFBitmap bitmap = RenderLoadedPageWithFlags(page, FPDF_ANNOT);
  CompareBitmap(bitmap.get(), 200, 200, kChecksum);

  EXPECT_EQ(FLATTEN_SUCCESS, FPDFPage_Flatten(page, FLAT_PRINT));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  UnloadPage(page);

  VerifySavedDocument(200, 200, kChecksum);
}

TEST_F(FPDFFlattenEmbedderTest, BUG_896366) {
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
  constexpr char kChecksum[] = "c3cccfadc4c5249e6aa0675e511fa4c3";
#else
  constexpr char kChecksum[] = "f71ab085c52c8445ae785eca3ec858b1";
#endif
  ASSERT_TRUE(OpenDocument("bug_896366.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  ScopedFPDFBitmap bitmap = RenderLoadedPageWithFlags(page, FPDF_ANNOT);
  CompareBitmap(bitmap.get(), 612, 792, kChecksum);

  EXPECT_EQ(FLATTEN_SUCCESS, FPDFPage_Flatten(page, FLAT_PRINT));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  UnloadPage(page);

  VerifySavedDocument(612, 792, kChecksum);
}
