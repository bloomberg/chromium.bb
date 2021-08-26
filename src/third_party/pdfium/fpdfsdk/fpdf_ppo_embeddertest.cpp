// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "core/fpdfapi/page/cpdf_form.h"
#include "core/fpdfapi/page/cpdf_formobject.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "public/cpp/fpdf_scopers.h"
#include "public/fpdf_edit.h"
#include "public/fpdf_ppo.h"
#include "public/fpdf_save.h"
#include "public/fpdfview.h"
#include "testing/embedder_test.h"
#include "testing/embedder_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/base/cxx17_backports.h"

namespace {

class FPDFPPOEmbedderTest : public EmbedderTest {};

int FakeBlockWriter(FPDF_FILEWRITE* pThis,
                    const void* pData,
                    unsigned long size) {
  return size;
}

}  // namespace

TEST_F(FPDFPPOEmbedderTest, NoViewerPreferences) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  EXPECT_TRUE(output_doc);
  EXPECT_FALSE(FPDF_CopyViewerPreferences(output_doc, document()));
  FPDF_CloseDocument(output_doc);
}

TEST_F(FPDFPPOEmbedderTest, ViewerPreferences) {
  ASSERT_TRUE(OpenDocument("viewer_ref.pdf"));

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  EXPECT_TRUE(output_doc);
  EXPECT_TRUE(FPDF_CopyViewerPreferences(output_doc, document()));
  FPDF_CloseDocument(output_doc);
}

TEST_F(FPDFPPOEmbedderTest, ImportPagesByIndex) {
  ASSERT_TRUE(OpenDocument("viewer_ref.pdf"));

  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  ScopedFPDFDocument output_doc(FPDF_CreateNewDocument());
  ASSERT_TRUE(output_doc);
  EXPECT_TRUE(FPDF_CopyViewerPreferences(output_doc.get(), document()));

  static constexpr int kPageIndices[] = {1};
  EXPECT_TRUE(FPDF_ImportPagesByIndex(output_doc.get(), document(),
                                      kPageIndices, pdfium::size(kPageIndices),
                                      0));
  EXPECT_EQ(1, FPDF_GetPageCount(output_doc.get()));

  UnloadPage(page);
}

TEST_F(FPDFPPOEmbedderTest, ImportPages) {
  ASSERT_TRUE(OpenDocument("viewer_ref.pdf"));

  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  ASSERT_TRUE(output_doc);
  EXPECT_TRUE(FPDF_CopyViewerPreferences(output_doc, document()));
  EXPECT_TRUE(FPDF_ImportPages(output_doc, document(), "1", 0));
  EXPECT_EQ(1, FPDF_GetPageCount(output_doc));
  FPDF_CloseDocument(output_doc);

  UnloadPage(page);
}

TEST_F(FPDFPPOEmbedderTest, ImportNPages) {
  ASSERT_TRUE(OpenDocument("rectangles_multi_pages.pdf"));

  ScopedFPDFDocument output_doc_2up(
      FPDF_ImportNPagesToOne(document(), 612, 792, 2, 1));
  ASSERT_TRUE(output_doc_2up);
  EXPECT_EQ(3, FPDF_GetPageCount(output_doc_2up.get()));
  ScopedFPDFDocument output_doc_5up(
      FPDF_ImportNPagesToOne(document(), 612, 792, 5, 1));
  ASSERT_TRUE(output_doc_5up);
  EXPECT_EQ(1, FPDF_GetPageCount(output_doc_5up.get()));
  ScopedFPDFDocument output_doc_8up(
      FPDF_ImportNPagesToOne(document(), 792, 612, 8, 1));
  ASSERT_TRUE(output_doc_8up);
  EXPECT_EQ(1, FPDF_GetPageCount(output_doc_8up.get()));
  ScopedFPDFDocument output_doc_128up(
      FPDF_ImportNPagesToOne(document(), 792, 612, 128, 1));
  ASSERT_TRUE(output_doc_128up);
  EXPECT_EQ(1, FPDF_GetPageCount(output_doc_128up.get()));
}

TEST_F(FPDFPPOEmbedderTest, BadNupParams) {
  ASSERT_TRUE(OpenDocument("rectangles_multi_pages.pdf"));

  FPDF_DOCUMENT output_doc_zero_row =
      FPDF_ImportNPagesToOne(document(), 612, 792, 0, 3);
  ASSERT_FALSE(output_doc_zero_row);
  FPDF_DOCUMENT output_doc_zero_col =
      FPDF_ImportNPagesToOne(document(), 612, 792, 2, 0);
  ASSERT_FALSE(output_doc_zero_col);
  FPDF_DOCUMENT output_doc_zero_width =
      FPDF_ImportNPagesToOne(document(), 0, 792, 2, 1);
  ASSERT_FALSE(output_doc_zero_width);
  FPDF_DOCUMENT output_doc_zero_height =
      FPDF_ImportNPagesToOne(document(), 612, 0, 7, 1);
  ASSERT_FALSE(output_doc_zero_height);
}

// TODO(Xlou): Add more tests to check output doc content of
// FPDF_ImportNPagesToOne()
TEST_F(FPDFPPOEmbedderTest, NupRenderImage) {
  ASSERT_TRUE(OpenDocument("rectangles_multi_pages.pdf"));
  const int kPageCount = 2;
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
  static constexpr const char* kExpectedMD5s[kPageCount] = {
      "7a4cddd5a17a60ce50acb53e318d94f8", "4fa6a7507e9f3ef4f28719a7d656c3a5"};
#else
  static constexpr const char* kExpectedMD5s[kPageCount] = {
      "72d0d7a19a2f40e010ca6a1133b33e1e", "fb18142190d770cfbc329d2b071aee4d"};
#endif
  ScopedFPDFDocument output_doc_3up(
      FPDF_ImportNPagesToOne(document(), 792, 612, 3, 1));
  ASSERT_TRUE(output_doc_3up);
  ASSERT_EQ(kPageCount, FPDF_GetPageCount(output_doc_3up.get()));
  for (int i = 0; i < kPageCount; ++i) {
    ScopedFPDFPage page(FPDF_LoadPage(output_doc_3up.get(), i));
    ASSERT_TRUE(page);
    ScopedFPDFBitmap bitmap = RenderPage(page.get());
    EXPECT_EQ(792, FPDFBitmap_GetWidth(bitmap.get()));
    EXPECT_EQ(612, FPDFBitmap_GetHeight(bitmap.get()));
    EXPECT_EQ(kExpectedMD5s[i], HashBitmap(bitmap.get()));
  }
}

TEST_F(FPDFPPOEmbedderTest, ImportPageToXObject) {
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
  static const char kChecksum[] = "d6ebc0a8afc22fe0137f54ce54e1a19c";
#else
  static const char kChecksum[] = "2d88d180af7109eb346439f7c855bb29";
#endif

  ASSERT_TRUE(OpenDocument("rectangles.pdf"));

  {
    ScopedFPDFDocument output_doc(FPDF_CreateNewDocument());
    ASSERT_TRUE(output_doc);

    FPDF_XOBJECT xobject =
        FPDF_NewXObjectFromPage(output_doc.get(), document(), 0);
    ASSERT_TRUE(xobject);

    for (int i = 0; i < 2; ++i) {
      ScopedFPDFPage page(FPDFPage_New(output_doc.get(), 0, 612, 792));
      ASSERT_TRUE(page);

      FPDF_PAGEOBJECT page_object = FPDF_NewFormObjectFromXObject(xobject);
      ASSERT_TRUE(page_object);
      EXPECT_EQ(FPDF_PAGEOBJ_FORM, FPDFPageObj_GetType(page_object));
      FPDFPage_InsertObject(page.get(), page_object);
      EXPECT_TRUE(FPDFPage_GenerateContent(page.get()));

      // TODO(thestig): This should have `kChecksum`.
      ScopedFPDFBitmap page_bitmap = RenderPage(page.get());
      CompareBitmap(page_bitmap.get(), 612, 792,
                    pdfium::kBlankPage612By792Checksum);
    }

    EXPECT_TRUE(FPDF_SaveAsCopy(output_doc.get(), this, 0));

    FPDF_CloseXObject(xobject);
  }

  constexpr int kExpectedPageCount = 2;
  ASSERT_TRUE(OpenSavedDocument());

  FPDF_PAGE saved_pages[kExpectedPageCount];
  FPDF_PAGEOBJECT xobjects[kExpectedPageCount];
  for (int i = 0; i < kExpectedPageCount; ++i) {
    saved_pages[i] = LoadSavedPage(i);
    ASSERT_TRUE(saved_pages[i]);

    EXPECT_EQ(1, FPDFPage_CountObjects(saved_pages[i]));
    xobjects[i] = FPDFPage_GetObject(saved_pages[i], 0);
    ASSERT_TRUE(xobjects[i]);
    ASSERT_EQ(FPDF_PAGEOBJ_FORM, FPDFPageObj_GetType(xobjects[i]));
    EXPECT_EQ(8, FPDFFormObj_CountObjects(xobjects[i]));

    {
      ScopedFPDFBitmap page_bitmap = RenderPage(saved_pages[i]);
      CompareBitmap(page_bitmap.get(), 612, 792, kChecksum);
    }
  }

  // Peek at object internals to make sure the two XObjects use the same stream.
  EXPECT_NE(xobjects[0], xobjects[1]);
  CPDF_PageObject* obj1 = CPDFPageObjectFromFPDFPageObject(xobjects[0]);
  ASSERT_TRUE(obj1->AsForm());
  ASSERT_TRUE(obj1->AsForm()->form());
  ASSERT_TRUE(obj1->AsForm()->form()->GetStream());
  CPDF_PageObject* obj2 = CPDFPageObjectFromFPDFPageObject(xobjects[1]);
  ASSERT_TRUE(obj2->AsForm());
  ASSERT_TRUE(obj2->AsForm()->form());
  ASSERT_TRUE(obj2->AsForm()->form()->GetStream());
  EXPECT_EQ(obj1->AsForm()->form()->GetStream(),
            obj2->AsForm()->form()->GetStream());

  for (FPDF_PAGE saved_page : saved_pages)
    CloseSavedPage(saved_page);

  CloseSavedDocument();
}

TEST_F(FPDFPPOEmbedderTest, ImportPageToXObjectWithSameDoc) {
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
  static const char kChecksum[] = "8e7d672f49f9ca98fb9157824cefc204";
#else
  static const char kChecksum[] = "4d5ca14827b7707f8283e639b33c121a";
#endif

  ASSERT_TRUE(OpenDocument("rectangles.pdf"));

  FPDF_XOBJECT xobject = FPDF_NewXObjectFromPage(document(), document(), 0);
  ASSERT_TRUE(xobject);

  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  {
    ScopedFPDFBitmap bitmap = RenderLoadedPage(page);
    CompareBitmap(bitmap.get(), 200, 300, pdfium::kRectanglesChecksum);
  }

  FPDF_PAGEOBJECT page_object = FPDF_NewFormObjectFromXObject(xobject);
  ASSERT_TRUE(page_object);
  ASSERT_EQ(FPDF_PAGEOBJ_FORM, FPDFPageObj_GetType(page_object));

  static constexpr FS_MATRIX kMatrix = {0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f};
  EXPECT_TRUE(FPDFPageObj_SetMatrix(page_object, &kMatrix));

  FPDFPage_InsertObject(page, page_object);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  {
    // TODO(thestig): This should have `kChecksum`.
    ScopedFPDFBitmap bitmap = RenderLoadedPage(page);
    CompareBitmap(bitmap.get(), 200, 300, pdfium::kRectanglesChecksum);
  }

  FPDF_CloseXObject(xobject);

  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  VerifySavedDocument(200, 300, kChecksum);

  UnloadPage(page);
}

TEST_F(FPDFPPOEmbedderTest, XObjectNullParams) {
  ASSERT_TRUE(OpenDocument("rectangles.pdf"));
  ASSERT_EQ(1, FPDF_GetPageCount(document()));

  EXPECT_FALSE(FPDF_NewXObjectFromPage(nullptr, nullptr, -1));
  EXPECT_FALSE(FPDF_NewXObjectFromPage(nullptr, nullptr, 0));
  EXPECT_FALSE(FPDF_NewXObjectFromPage(nullptr, nullptr, 1));
  EXPECT_FALSE(FPDF_NewXObjectFromPage(document(), nullptr, -1));
  EXPECT_FALSE(FPDF_NewXObjectFromPage(document(), nullptr, 0));
  EXPECT_FALSE(FPDF_NewXObjectFromPage(document(), nullptr, 1));
  EXPECT_FALSE(FPDF_NewXObjectFromPage(nullptr, document(), -1));
  EXPECT_FALSE(FPDF_NewXObjectFromPage(nullptr, document(), 0));
  EXPECT_FALSE(FPDF_NewXObjectFromPage(nullptr, document(), 1));

  {
    ScopedFPDFDocument output_doc(FPDF_CreateNewDocument());
    ASSERT_TRUE(output_doc);
    EXPECT_FALSE(FPDF_NewXObjectFromPage(output_doc.get(), document(), -1));
    EXPECT_FALSE(FPDF_NewXObjectFromPage(output_doc.get(), document(), 1));
  }

  // Should be a no-op.
  FPDF_CloseXObject(nullptr);

  EXPECT_FALSE(FPDF_NewFormObjectFromXObject(nullptr));
}

TEST_F(FPDFPPOEmbedderTest, BUG_925981) {
  ASSERT_TRUE(OpenDocument("bug_925981.pdf"));
  ScopedFPDFDocument output_doc_2up(
      FPDF_ImportNPagesToOne(document(), 612, 792, 2, 1));
  EXPECT_EQ(1, FPDF_GetPageCount(output_doc_2up.get()));
}

TEST_F(FPDFPPOEmbedderTest, BUG_1229106) {
  static constexpr int kPageCount = 4;
  static constexpr int kTwoUpPageCount = 2;
  static const char kRectsChecksum[] = "140d629b3c96a07ced2e3e408ea85a1d";
  static const char kTwoUpChecksum[] = "fa4501562301b2e75da354bd067495ec";

  ASSERT_TRUE(OpenDocument("bug_1229106.pdf"));

  // Show all pages render the same.
  ASSERT_EQ(kPageCount, FPDF_GetPageCount(document()));
  for (int i = 0; i < kPageCount; ++i) {
    FPDF_PAGE page = LoadPage(0);
    ScopedFPDFBitmap bitmap = RenderLoadedPage(page);
    CompareBitmap(bitmap.get(), 792, 612, kRectsChecksum);
    UnloadPage(page);
  }

  // Create a 2-up PDF.
  ScopedFPDFDocument output_doc_2up(
      FPDF_ImportNPagesToOne(document(), 612, 792, 1, 2));
  ASSERT_EQ(kTwoUpPageCount, FPDF_GetPageCount(output_doc_2up.get()));
  for (int i = 0; i < kTwoUpPageCount; ++i) {
    ScopedFPDFPage page(FPDF_LoadPage(output_doc_2up.get(), i));
    ScopedFPDFBitmap bitmap = RenderPage(page.get());
    CompareBitmap(bitmap.get(), 612, 792, kTwoUpChecksum);
  }
}

TEST_F(FPDFPPOEmbedderTest, BadRepeatViewerPref) {
  ASSERT_TRUE(OpenDocument("repeat_viewer_ref.pdf"));

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  EXPECT_TRUE(output_doc);
  EXPECT_TRUE(FPDF_CopyViewerPreferences(output_doc, document()));

  FPDF_FILEWRITE writer;
  writer.version = 1;
  writer.WriteBlock = FakeBlockWriter;

  EXPECT_TRUE(FPDF_SaveAsCopy(output_doc, &writer, 0));
  FPDF_CloseDocument(output_doc);
}

TEST_F(FPDFPPOEmbedderTest, BadCircularViewerPref) {
  ASSERT_TRUE(OpenDocument("circular_viewer_ref.pdf"));

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  EXPECT_TRUE(output_doc);
  EXPECT_TRUE(FPDF_CopyViewerPreferences(output_doc, document()));

  FPDF_FILEWRITE writer;
  writer.version = 1;
  writer.WriteBlock = FakeBlockWriter;

  EXPECT_TRUE(FPDF_SaveAsCopy(output_doc, &writer, 0));
  FPDF_CloseDocument(output_doc);
}

TEST_F(FPDFPPOEmbedderTest, BadIndices) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));

  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  ScopedFPDFDocument output_doc(FPDF_CreateNewDocument());
  EXPECT_TRUE(output_doc);

  static constexpr int kBadIndices1[] = {-1};
  EXPECT_FALSE(FPDF_ImportPagesByIndex(output_doc.get(), document(),
                                       kBadIndices1, pdfium::size(kBadIndices1),
                                       0));

  static constexpr int kBadIndices2[] = {1};
  EXPECT_FALSE(FPDF_ImportPagesByIndex(output_doc.get(), document(),
                                       kBadIndices2, pdfium::size(kBadIndices2),
                                       0));

  static constexpr int kBadIndices3[] = {-1, 0, 1};
  EXPECT_FALSE(FPDF_ImportPagesByIndex(output_doc.get(), document(),
                                       kBadIndices3, pdfium::size(kBadIndices3),
                                       0));

  static constexpr int kBadIndices4[] = {42};
  EXPECT_FALSE(FPDF_ImportPagesByIndex(output_doc.get(), document(),
                                       kBadIndices4, pdfium::size(kBadIndices4),
                                       0));

  UnloadPage(page);
}

TEST_F(FPDFPPOEmbedderTest, GoodIndices) {
  ASSERT_TRUE(OpenDocument("viewer_ref.pdf"));

  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  ScopedFPDFDocument output_doc(FPDF_CreateNewDocument());
  EXPECT_TRUE(output_doc);

  static constexpr int kGoodIndices1[] = {0, 0, 0, 0};
  EXPECT_TRUE(FPDF_ImportPagesByIndex(output_doc.get(), document(),
                                      kGoodIndices1,
                                      pdfium::size(kGoodIndices1), 0));
  EXPECT_EQ(4, FPDF_GetPageCount(output_doc.get()));

  static constexpr int kGoodIndices2[] = {0};
  EXPECT_TRUE(FPDF_ImportPagesByIndex(output_doc.get(), document(),
                                      kGoodIndices2,
                                      pdfium::size(kGoodIndices2), 0));
  EXPECT_EQ(5, FPDF_GetPageCount(output_doc.get()));

  static constexpr int kGoodIndices3[] = {4};
  EXPECT_TRUE(FPDF_ImportPagesByIndex(output_doc.get(), document(),
                                      kGoodIndices3,
                                      pdfium::size(kGoodIndices3), 0));
  EXPECT_EQ(6, FPDF_GetPageCount(output_doc.get()));

  static constexpr int kGoodIndices4[] = {1, 2, 3};
  EXPECT_TRUE(FPDF_ImportPagesByIndex(output_doc.get(), document(),
                                      kGoodIndices4,
                                      pdfium::size(kGoodIndices4), 0));
  EXPECT_EQ(9, FPDF_GetPageCount(output_doc.get()));

  // Passing in a nullptr should import all the pages.
  EXPECT_TRUE(
      FPDF_ImportPagesByIndex(output_doc.get(), document(), nullptr, 0, 0));
  EXPECT_EQ(14, FPDF_GetPageCount(output_doc.get()));

  UnloadPage(page);
}

TEST_F(FPDFPPOEmbedderTest, BadRanges) {
  ASSERT_TRUE(OpenDocument("hello_world.pdf"));

  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  EXPECT_TRUE(output_doc);
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "clams", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "0", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "42", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "1,2", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "1-2", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), ",1", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "1,", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "1-", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "-1", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "-,0,,,1-", 0));
  FPDF_CloseDocument(output_doc);

  UnloadPage(page);
}

TEST_F(FPDFPPOEmbedderTest, GoodRanges) {
  ASSERT_TRUE(OpenDocument("viewer_ref.pdf"));

  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  EXPECT_TRUE(output_doc);
  EXPECT_TRUE(FPDF_CopyViewerPreferences(output_doc, document()));
  EXPECT_TRUE(FPDF_ImportPages(output_doc, document(), "1,1,1,1", 0));
  EXPECT_EQ(4, FPDF_GetPageCount(output_doc));
  EXPECT_TRUE(FPDF_ImportPages(output_doc, document(), "1-1", 0));
  EXPECT_EQ(5, FPDF_GetPageCount(output_doc));
  EXPECT_TRUE(FPDF_ImportPages(output_doc, document(), "5-5", 0));
  EXPECT_EQ(6, FPDF_GetPageCount(output_doc));
  EXPECT_TRUE(FPDF_ImportPages(output_doc, document(), "2-4", 0));
  EXPECT_EQ(9, FPDF_GetPageCount(output_doc));
  FPDF_CloseDocument(output_doc);

  UnloadPage(page);
}

TEST_F(FPDFPPOEmbedderTest, BUG_664284) {
  ASSERT_TRUE(OpenDocument("bug_664284.pdf"));

  FPDF_PAGE page = LoadPage(0);
  ASSERT_NE(nullptr, page);

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  EXPECT_TRUE(output_doc);

  static constexpr int kIndices[] = {0};
  EXPECT_TRUE(FPDF_ImportPagesByIndex(output_doc, document(), kIndices,
                                      pdfium::size(kIndices), 0));
  FPDF_CloseDocument(output_doc);

  UnloadPage(page);
}

TEST_F(FPDFPPOEmbedderTest, BUG_750568) {
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
  const char* const kHashes[] = {
      "eaa139e944eafb43d31e8742a0e158de", "226485e9d4fa6a67dfe0a88723f12060",
      "c5601a3492ae5dcc5dd25140fc463bfe", "1f60055b54de4fac8a59c65e90da156e"};
#else
  const char* const kHashes[] = {
      "64ad08132a1c5a166768298c8a578f57", "83b83e2f6bc80707d0a917c7634140b9",
      "913cd3723a451e4e46fbc2c05702d1ee", "81fb7cfd4860f855eb468f73dfeb6d60"};
#endif

  ASSERT_TRUE(OpenDocument("bug_750568.pdf"));
  ASSERT_EQ(4, FPDF_GetPageCount(document()));

  for (size_t i = 0; i < 4; ++i) {
    FPDF_PAGE page = LoadPage(i);
    ASSERT_TRUE(page);

    ScopedFPDFBitmap bitmap = RenderLoadedPage(page);
    CompareBitmap(bitmap.get(), 200, 200, kHashes[i]);
    UnloadPage(page);
  }

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  ASSERT_TRUE(output_doc);

  static constexpr int kIndices[] = {0, 1, 2, 3};
  EXPECT_TRUE(FPDF_ImportPagesByIndex(output_doc, document(), kIndices,
                                      pdfium::size(kIndices), 0));
  ASSERT_EQ(4, FPDF_GetPageCount(output_doc));
  for (size_t i = 0; i < 4; ++i) {
    FPDF_PAGE page = FPDF_LoadPage(output_doc, i);
    ASSERT_TRUE(page);

    ScopedFPDFBitmap bitmap = RenderPage(page);
    CompareBitmap(bitmap.get(), 200, 200, kHashes[i]);
    FPDF_ClosePage(page);
  }
  FPDF_CloseDocument(output_doc);
}

TEST_F(FPDFPPOEmbedderTest, ImportWithZeroLengthStream) {
  ASSERT_TRUE(OpenDocument("zero_length_stream.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  ScopedFPDFBitmap bitmap = RenderLoadedPage(page);
  CompareBitmap(bitmap.get(), 200, 200, pdfium::kHelloWorldChecksum);
  UnloadPage(page);

  ScopedFPDFDocument new_doc(FPDF_CreateNewDocument());
  ASSERT_TRUE(new_doc);

  static constexpr int kIndices[] = {0};
  EXPECT_TRUE(FPDF_ImportPagesByIndex(new_doc.get(), document(), kIndices,
                                      pdfium::size(kIndices), 0));

  EXPECT_EQ(1, FPDF_GetPageCount(new_doc.get()));
  ScopedFPDFPage new_page(FPDF_LoadPage(new_doc.get(), 0));
  ASSERT_TRUE(new_page);
  ScopedFPDFBitmap new_bitmap = RenderPage(new_page.get());
  CompareBitmap(new_bitmap.get(), 200, 200, pdfium::kHelloWorldChecksum);
}
