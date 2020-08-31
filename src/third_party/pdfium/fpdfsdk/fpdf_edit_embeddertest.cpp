// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/page/cpdf_formobject.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxge/fx_font.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "public/cpp/fpdf_scopers.h"
#include "public/fpdf_annot.h"
#include "public/fpdf_edit.h"
#include "public/fpdfview.h"
#include "testing/embedder_test.h"
#include "testing/embedder_test_constants.h"
#include "testing/fx_string_testhelpers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/utils/hash.h"

using pdfium::kHelloWorldChecksum;

namespace {

const char kRedRectangleChecksum[] = "66d02eaa6181e2c069ce2ea99beda497";

#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#if defined(OS_WIN)
const char kFirstRemovedChecksum[] = "d468228cc55071a9e838f66afc0f58ff";
#else
const char kFirstRemovedChecksum[] = "0c79dc1065b1d06991e3ac4aaa35d25c";
#endif  // defined(OS_WIN)
#else
#if defined(OS_WIN)
const char kFirstRemovedChecksum[] = "aae6c5334721f90ec30d3d59f4ef7deb";
#elif defined(OS_MACOSX)
const char kFirstRemovedChecksum[] = "17ca3778fd8bb395b46532f1fa17f702";
#else
const char kFirstRemovedChecksum[] = "b76df015fe88009c3c342395df96abf1";
#endif
#endif  // defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)

}  // namespace

class FPDFEditEmbedderTest : public EmbedderTest {
 protected:
  FPDF_DOCUMENT CreateNewDocument() {
    document_ = FPDF_CreateNewDocument();
    cpdf_doc_ = CPDFDocumentFromFPDFDocument(document_);
    return document_;
  }

  void CheckFontDescriptor(const CPDF_Dictionary* font_dict,
                           int font_type,
                           bool bold,
                           bool italic,
                           pdfium::span<const uint8_t> span) {
    const CPDF_Dictionary* font_desc = font_dict->GetDictFor("FontDescriptor");
    ASSERT_TRUE(font_desc);
    EXPECT_EQ("FontDescriptor", font_desc->GetStringFor("Type"));
    EXPECT_EQ(font_dict->GetStringFor("BaseFont"),
              font_desc->GetStringFor("FontName"));

    // Check that the font descriptor has the required keys according to spec
    // 1.7 Table 5.19
    ASSERT_TRUE(font_desc->KeyExist("Flags"));

    int font_flags = font_desc->GetIntegerFor("Flags");
    EXPECT_EQ(bold, FontStyleIsForceBold(font_flags));
    EXPECT_EQ(italic, FontStyleIsItalic(font_flags));
    EXPECT_TRUE(FontStyleIsNonSymbolic(font_flags));
    ASSERT_TRUE(font_desc->KeyExist("FontBBox"));

    const CPDF_Array* fontBBox = font_desc->GetArrayFor("FontBBox");
    ASSERT_TRUE(fontBBox);
    EXPECT_EQ(4u, fontBBox->size());
    // Check that the coordinates are in the preferred order according to spec
    // 1.7 Section 3.8.4
    EXPECT_TRUE(fontBBox->GetIntegerAt(0) < fontBBox->GetIntegerAt(2));
    EXPECT_TRUE(fontBBox->GetIntegerAt(1) < fontBBox->GetIntegerAt(3));

    EXPECT_TRUE(font_desc->KeyExist("ItalicAngle"));
    EXPECT_TRUE(font_desc->KeyExist("Ascent"));
    EXPECT_TRUE(font_desc->KeyExist("Descent"));
    EXPECT_TRUE(font_desc->KeyExist("CapHeight"));
    EXPECT_TRUE(font_desc->KeyExist("StemV"));
    ByteString present("FontFile");
    ByteString absent("FontFile2");
    if (font_type == FPDF_FONT_TRUETYPE)
      std::swap(present, absent);
    EXPECT_TRUE(font_desc->KeyExist(present));
    EXPECT_FALSE(font_desc->KeyExist(absent));

    auto streamAcc =
        pdfium::MakeRetain<CPDF_StreamAcc>(font_desc->GetStreamFor(present));
    streamAcc->LoadAllDataRaw();

    // Check that the font stream is the one that was provided
    ASSERT_EQ(span.size(), streamAcc->GetSize());
    if (font_type == FPDF_FONT_TRUETYPE) {
      ASSERT_EQ(static_cast<int>(span.size()),
                streamAcc->GetDict()->GetIntegerFor("Length1"));
    }

    const uint8_t* stream_data = streamAcc->GetData();
    for (size_t j = 0; j < span.size(); j++)
      EXPECT_EQ(span[j], stream_data[j]) << " at byte " << j;
  }

  void CheckCompositeFontWidths(const CPDF_Array* widths_array,
                                CPDF_Font* typed_font) {
    // Check that W array is in a format that conforms to PDF spec 1.7 section
    // "Glyph Metrics in CIDFonts" (these checks are not
    // implementation-specific).
    EXPECT_GT(widths_array->size(), 1u);
    int num_cids_checked = 0;
    int cur_cid = 0;
    for (size_t idx = 0; idx < widths_array->size(); idx++) {
      int cid = widths_array->GetNumberAt(idx);
      EXPECT_GE(cid, cur_cid);
      ASSERT_FALSE(++idx == widths_array->size());
      const CPDF_Object* next = widths_array->GetObjectAt(idx);
      if (next->IsArray()) {
        // We are in the c [w1 w2 ...] case
        const CPDF_Array* arr = next->AsArray();
        int cnt = static_cast<int>(arr->size());
        size_t inner_idx = 0;
        for (cur_cid = cid; cur_cid < cid + cnt; cur_cid++) {
          uint32_t width = arr->GetNumberAt(inner_idx++);
          EXPECT_EQ(width, typed_font->GetCharWidthF(cur_cid))
              << " at cid " << cur_cid;
        }
        num_cids_checked += cnt;
        continue;
      }
      // Otherwise, are in the c_first c_last w case.
      ASSERT_TRUE(next->IsNumber());
      int last_cid = next->AsNumber()->GetInteger();
      ASSERT_FALSE(++idx == widths_array->size());
      uint32_t width = widths_array->GetNumberAt(idx);
      for (cur_cid = cid; cur_cid <= last_cid; cur_cid++) {
        EXPECT_EQ(width, typed_font->GetCharWidthF(cur_cid))
            << " at cid " << cur_cid;
      }
      num_cids_checked += last_cid - cid + 1;
    }
    // Make sure we have a good amount of cids described
    EXPECT_GT(num_cids_checked, 900);
  }
  CPDF_Document* cpdf_doc() { return cpdf_doc_; }

 private:
  CPDF_Document* cpdf_doc_;
};

namespace {

const char kExpectedPDF[] =
    "%PDF-1.7\r\n"
    "%\xA1\xB3\xC5\xD7\r\n"
    "1 0 obj\r\n"
    "<</Pages 2 0 R /Type/Catalog>>\r\n"
    "endobj\r\n"
    "2 0 obj\r\n"
    "<</Count 1/Kids\\[ 4 0 R \\]/Type/Pages>>\r\n"
    "endobj\r\n"
    "3 0 obj\r\n"
    "<</CreationDate\\(D:.*\\)/Creator\\(PDFium\\)>>\r\n"
    "endobj\r\n"
    "4 0 obj\r\n"
    "<</MediaBox\\[ 0 0 640 480\\]/Parent 2 0 R "
    "/Resources<</ExtGState<</FXE1 5 0 R >>>>"
    "/Rotate 0/Type/Page"
    ">>\r\n"
    "endobj\r\n"
    "5 0 obj\r\n"
    "<</BM/Normal/CA 1/ca 1>>\r\n"
    "endobj\r\n"
    "xref\r\n"
    "0 6\r\n"
    "0000000000 65535 f\r\n"
    "0000000017 00000 n\r\n"
    "0000000066 00000 n\r\n"
    "0000000122 00000 n\r\n"
    "0000000192 00000 n\r\n"
    "0000000311 00000 n\r\n"
    "trailer\r\n"
    "<<\r\n"
    "/Root 1 0 R\r\n"
    "/Info 3 0 R\r\n"
    "/Size 6/ID\\[<.*><.*>\\]>>\r\n"
    "startxref\r\n"
    "354\r\n"
    "%%EOF\r\n";

}  // namespace

TEST_F(FPDFEditEmbedderTest, EmptyCreation) {
  EXPECT_TRUE(CreateEmptyDocument());
  FPDF_PAGE page = FPDFPage_New(document(), 0, 640.0, 480.0);
  EXPECT_NE(nullptr, page);
  // The FPDFPage_GenerateContent call should do nothing.
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  EXPECT_THAT(GetString(), testing::MatchesRegex(std::string(
                               kExpectedPDF, sizeof(kExpectedPDF))));
  FPDF_ClosePage(page);
}

// Regression test for https://crbug.com/667012
TEST_F(FPDFEditEmbedderTest, RasterizePDF) {
  const char kAllBlackMd5sum[] = "5708fc5c4a8bd0abde99c8e8f0390615";

  // Get the bitmap for the original document.
  ScopedFPDFBitmap orig_bitmap;
  {
    EXPECT_TRUE(OpenDocument("black.pdf"));
    FPDF_PAGE orig_page = LoadPage(0);
    ASSERT_TRUE(orig_page);
    orig_bitmap = RenderLoadedPage(orig_page);
    CompareBitmap(orig_bitmap.get(), 612, 792, kAllBlackMd5sum);
    UnloadPage(orig_page);
  }

  // Create a new document from |orig_bitmap| and save it.
  {
    FPDF_DOCUMENT temp_doc = FPDF_CreateNewDocument();
    FPDF_PAGE temp_page = FPDFPage_New(temp_doc, 0, 612, 792);

    // Add the bitmap to an image object and add the image object to the output
    // page.
    FPDF_PAGEOBJECT temp_img = FPDFPageObj_NewImageObj(temp_doc);
    EXPECT_TRUE(
        FPDFImageObj_SetBitmap(&temp_page, 1, temp_img, orig_bitmap.get()));
    EXPECT_TRUE(FPDFImageObj_SetMatrix(temp_img, 612, 0, 0, 792, 0, 0));
    FPDFPage_InsertObject(temp_page, temp_img);
    EXPECT_TRUE(FPDFPage_GenerateContent(temp_page));
    EXPECT_TRUE(FPDF_SaveAsCopy(temp_doc, this, 0));
    FPDF_ClosePage(temp_page);
    FPDF_CloseDocument(temp_doc);
  }

  // Get the generated content. Make sure it is at least as big as the original
  // PDF.
  EXPECT_GT(GetString().size(), 923u);
  VerifySavedDocument(612, 792, kAllBlackMd5sum);
}

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_AddPaths DISABLED_AddPaths
#else
#define MAYBE_AddPaths AddPaths
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_AddPaths) {
  // Start with a blank page
  FPDF_PAGE page = FPDFPage_New(CreateNewDocument(), 0, 612, 792);
  ASSERT_TRUE(page);

  // We will first add a red rectangle
  FPDF_PAGEOBJECT red_rect = FPDFPageObj_CreateNewRect(10, 10, 20, 20);
  ASSERT_TRUE(red_rect);
  // Expect false when trying to set colors out of range
  EXPECT_FALSE(FPDFPageObj_SetStrokeColor(red_rect, 100, 100, 100, 300));
  EXPECT_FALSE(FPDFPageObj_SetFillColor(red_rect, 200, 256, 200, 0));

  // Fill rectangle with red and insert to the page
  EXPECT_TRUE(FPDFPageObj_SetFillColor(red_rect, 255, 0, 0, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(red_rect, FPDF_FILLMODE_ALTERNATE, 0));

  int fillmode = FPDF_FILLMODE_NONE;
  FPDF_BOOL stroke = true;
  EXPECT_TRUE(FPDFPath_GetDrawMode(red_rect, &fillmode, &stroke));
  EXPECT_EQ(FPDF_FILLMODE_ALTERNATE, fillmode);
  EXPECT_FALSE(stroke);

  static const FS_MATRIX kMatrix = {1, 2, 3, 4, 5, 6};
  EXPECT_FALSE(FPDFPath_SetMatrix(nullptr, &kMatrix));
  EXPECT_TRUE(FPDFPath_SetMatrix(red_rect, &kMatrix));

  FS_MATRIX matrix;
  EXPECT_FALSE(FPDFPath_GetMatrix(nullptr, &matrix));
  EXPECT_TRUE(FPDFPath_GetMatrix(red_rect, &matrix));
  EXPECT_FLOAT_EQ(1.0f, matrix.a);
  EXPECT_FLOAT_EQ(2.0f, matrix.b);
  EXPECT_FLOAT_EQ(3.0f, matrix.c);
  EXPECT_FLOAT_EQ(4.0f, matrix.d);
  EXPECT_FLOAT_EQ(5.0f, matrix.e);
  EXPECT_FLOAT_EQ(6.0f, matrix.f);

  // Set back the identity matrix.
  matrix = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
  EXPECT_TRUE(FPDFPath_SetMatrix(red_rect, &matrix));

  FPDFPage_InsertObject(page, red_rect);
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792, kRedRectangleChecksum);
  }

  // Now add to that a green rectangle with some medium alpha
  FPDF_PAGEOBJECT green_rect = FPDFPageObj_CreateNewRect(100, 100, 40, 40);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(green_rect, 0, 255, 0, 128));

  // Make sure the type of the rectangle is a path.
  EXPECT_EQ(FPDF_PAGEOBJ_PATH, FPDFPageObj_GetType(green_rect));

  // Make sure we get back the same color we set previously.
  unsigned int R;
  unsigned int G;
  unsigned int B;
  unsigned int A;
  EXPECT_TRUE(FPDFPageObj_GetFillColor(green_rect, &R, &G, &B, &A));
  EXPECT_EQ(0u, R);
  EXPECT_EQ(255u, G);
  EXPECT_EQ(0u, B);
  EXPECT_EQ(128u, A);

  // Make sure the path has 5 points (1 FXPT_TYPE::MoveTo and 4
  // FXPT_TYPE::LineTo).
  ASSERT_EQ(5, FPDFPath_CountSegments(green_rect));
  // Verify actual coordinates.
  FPDF_PATHSEGMENT segment = FPDFPath_GetPathSegment(green_rect, 0);
  float x;
  float y;
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(100, x);
  EXPECT_EQ(100, y);
  EXPECT_EQ(FPDF_SEGMENT_MOVETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));
  segment = FPDFPath_GetPathSegment(green_rect, 1);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(100, x);
  EXPECT_EQ(140, y);
  EXPECT_EQ(FPDF_SEGMENT_LINETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));
  segment = FPDFPath_GetPathSegment(green_rect, 2);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(140, x);
  EXPECT_EQ(140, y);
  EXPECT_EQ(FPDF_SEGMENT_LINETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));
  segment = FPDFPath_GetPathSegment(green_rect, 3);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(140, x);
  EXPECT_EQ(100, y);
  EXPECT_EQ(FPDF_SEGMENT_LINETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));
  segment = FPDFPath_GetPathSegment(green_rect, 4);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(100, x);
  EXPECT_EQ(100, y);
  EXPECT_EQ(FPDF_SEGMENT_LINETO, FPDFPathSegment_GetType(segment));
  EXPECT_TRUE(FPDFPathSegment_GetClose(segment));

  EXPECT_TRUE(FPDFPath_SetDrawMode(green_rect, FPDF_FILLMODE_WINDING, 0));
  FPDFPage_InsertObject(page, green_rect);
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792,
                  "7b0b87604594e773add528fae567a558");
  }

  // Add a black triangle.
  FPDF_PAGEOBJECT black_path = FPDFPageObj_CreateNewPath(400, 100);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(black_path, 0, 0, 0, 200));
  EXPECT_TRUE(FPDFPath_SetDrawMode(black_path, FPDF_FILLMODE_ALTERNATE, 0));
  EXPECT_TRUE(FPDFPath_LineTo(black_path, 400, 200));
  EXPECT_TRUE(FPDFPath_LineTo(black_path, 300, 100));
  EXPECT_TRUE(FPDFPath_Close(black_path));

  // Make sure the path has 3 points (1 FXPT_TYPE::MoveTo and 2
  // FXPT_TYPE::LineTo).
  ASSERT_EQ(3, FPDFPath_CountSegments(black_path));
  // Verify actual coordinates.
  segment = FPDFPath_GetPathSegment(black_path, 0);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(400, x);
  EXPECT_EQ(100, y);
  EXPECT_EQ(FPDF_SEGMENT_MOVETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));
  segment = FPDFPath_GetPathSegment(black_path, 1);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(400, x);
  EXPECT_EQ(200, y);
  EXPECT_EQ(FPDF_SEGMENT_LINETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));
  segment = FPDFPath_GetPathSegment(black_path, 2);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(300, x);
  EXPECT_EQ(100, y);
  EXPECT_EQ(FPDF_SEGMENT_LINETO, FPDFPathSegment_GetType(segment));
  EXPECT_TRUE(FPDFPathSegment_GetClose(segment));
  // Make sure out of bounds index access fails properly.
  EXPECT_EQ(nullptr, FPDFPath_GetPathSegment(black_path, 3));

  FPDFPage_InsertObject(page, black_path);
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792,
                  "eadc8020a14dfcf091da2688733d8806");
  }

  // Now add a more complex blue path.
  FPDF_PAGEOBJECT blue_path = FPDFPageObj_CreateNewPath(200, 200);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(blue_path, 0, 0, 255, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(blue_path, FPDF_FILLMODE_WINDING, 0));
  EXPECT_TRUE(FPDFPath_LineTo(blue_path, 230, 230));
  EXPECT_TRUE(FPDFPath_BezierTo(blue_path, 250, 250, 280, 280, 300, 300));
  EXPECT_TRUE(FPDFPath_LineTo(blue_path, 325, 325));
  EXPECT_TRUE(FPDFPath_LineTo(blue_path, 350, 325));
  EXPECT_TRUE(FPDFPath_BezierTo(blue_path, 375, 330, 390, 360, 400, 400));
  EXPECT_TRUE(FPDFPath_Close(blue_path));
  FPDFPage_InsertObject(page, blue_path);
  const char kLastMD5[] = "9823e1a21bd9b72b6a442ba4f12af946";
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792, kLastMD5);
  }

  // Now save the result, closing the page and document
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  FPDF_ClosePage(page);

  // Render the saved result
  VerifySavedDocument(612, 792, kLastMD5);
}

TEST_F(FPDFEditEmbedderTest, ClipPath) {
  // Load document with a clipped rectangle.
  EXPECT_TRUE(OpenDocument("clip_path.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  ASSERT_EQ(1, FPDFPage_CountObjects(page));

  FPDF_PAGEOBJECT triangle = FPDFPage_GetObject(page, 0);
  ASSERT_TRUE(triangle);

  // Test that we got the expected triangle.
  ASSERT_EQ(4, FPDFPath_CountSegments(triangle));

  FPDF_PATHSEGMENT segment = FPDFPath_GetPathSegment(triangle, 0);
  float x;
  float y;
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(10, x);
  EXPECT_EQ(10, y);
  EXPECT_EQ(FPDF_SEGMENT_MOVETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));

  segment = FPDFPath_GetPathSegment(triangle, 1);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(25, x);
  EXPECT_EQ(40, y);
  EXPECT_EQ(FPDF_SEGMENT_LINETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));

  segment = FPDFPath_GetPathSegment(triangle, 2);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(40, x);
  EXPECT_EQ(10, y);
  EXPECT_EQ(FPDF_SEGMENT_LINETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));

  segment = FPDFPath_GetPathSegment(triangle, 3);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_TRUE(FPDFPathSegment_GetClose(segment));

  // Test FPDFPageObj_GetClipPath().
  ASSERT_EQ(nullptr, FPDFPageObj_GetClipPath(nullptr));

  FPDF_CLIPPATH clip_path = FPDFPageObj_GetClipPath(triangle);
  ASSERT_TRUE(clip_path);

  // Test FPDFClipPath_CountPaths().
  ASSERT_EQ(-1, FPDFClipPath_CountPaths(nullptr));
  ASSERT_EQ(1, FPDFClipPath_CountPaths(clip_path));

  // Test FPDFClipPath_CountPathSegments().
  ASSERT_EQ(-1, FPDFClipPath_CountPathSegments(nullptr, 0));
  ASSERT_EQ(-1, FPDFClipPath_CountPathSegments(clip_path, -1));
  ASSERT_EQ(-1, FPDFClipPath_CountPathSegments(clip_path, 1));
  ASSERT_EQ(4, FPDFClipPath_CountPathSegments(clip_path, 0));

  // FPDFClipPath_GetPathSegment() negative testing.
  ASSERT_EQ(nullptr, FPDFClipPath_GetPathSegment(nullptr, 0, 0));
  ASSERT_EQ(nullptr, FPDFClipPath_GetPathSegment(clip_path, -1, 0));
  ASSERT_EQ(nullptr, FPDFClipPath_GetPathSegment(clip_path, 1, 0));
  ASSERT_EQ(nullptr, FPDFClipPath_GetPathSegment(clip_path, 0, -1));
  ASSERT_EQ(nullptr, FPDFClipPath_GetPathSegment(clip_path, 0, 4));

  // FPDFClipPath_GetPathSegment() positive testing.
  segment = FPDFClipPath_GetPathSegment(clip_path, 0, 0);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(10, x);
  EXPECT_EQ(15, y);
  EXPECT_EQ(FPDF_SEGMENT_MOVETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));

  segment = FPDFClipPath_GetPathSegment(clip_path, 0, 1);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(40, x);
  EXPECT_EQ(15, y);
  EXPECT_EQ(FPDF_SEGMENT_LINETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));

  segment = FPDFClipPath_GetPathSegment(clip_path, 0, 2);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(40, x);
  EXPECT_EQ(35, y);
  EXPECT_EQ(FPDF_SEGMENT_LINETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));

  segment = FPDFClipPath_GetPathSegment(clip_path, 0, 3);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_EQ(10, x);
  EXPECT_EQ(35, y);
  EXPECT_EQ(FPDF_SEGMENT_LINETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));

  UnloadPage(page);
}

TEST_F(FPDFEditEmbedderTest, BUG_1399) {
  // Load document with a clipped rectangle.
  EXPECT_TRUE(OpenDocument("bug_1399.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  ASSERT_EQ(7, FPDFPage_CountObjects(page));

  FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, 0);
  ASSERT_TRUE(obj);

  ASSERT_EQ(2, FPDFPath_CountSegments(obj));

  FPDF_PATHSEGMENT segment = FPDFPath_GetPathSegment(obj, 0);
  float x;
  float y;
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_FLOAT_EQ(107.718f, x);
  EXPECT_FLOAT_EQ(719.922f, y);
  EXPECT_EQ(FPDF_SEGMENT_MOVETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));

  segment = FPDFPath_GetPathSegment(obj, 1);
  EXPECT_TRUE(FPDFPathSegment_GetPoint(segment, &x, &y));
  EXPECT_FLOAT_EQ(394.718f, x);
  EXPECT_FLOAT_EQ(719.922f, y);
  EXPECT_EQ(FPDF_SEGMENT_LINETO, FPDFPathSegment_GetType(segment));
  EXPECT_FALSE(FPDFPathSegment_GetClose(segment));

  FPDF_CLIPPATH clip_path = FPDFPageObj_GetClipPath(obj);
  ASSERT_TRUE(clip_path);

  EXPECT_EQ(-1, FPDFClipPath_CountPaths(clip_path));
  EXPECT_EQ(-1, FPDFClipPath_CountPathSegments(clip_path, 0));
  EXPECT_FALSE(FPDFClipPath_GetPathSegment(clip_path, 0, 0));

  UnloadPage(page);
}

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_SetText DISABLED_SetText
#else
#define MAYBE_SetText SetText
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_SetText) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Get the "Hello, world!" text object and change it.
  ASSERT_EQ(2, FPDFPage_CountObjects(page));
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 0);
  ASSERT_TRUE(page_object);
  ScopedFPDFWideString text1 = GetFPDFWideString(L"Changed for SetText test");
  EXPECT_TRUE(FPDFText_SetText(page_object, text1.get()));

  // Verify the "Hello, world!" text is gone and "Changed for SetText test" is
  // now displayed.
  ASSERT_EQ(2, FPDFPage_CountObjects(page));
#if defined(OS_MACOSX)
  const char kChangedMD5[] = "904132275a1144ea06b0694537c80b4c";
#elif defined(OS_WIN)
  const char kChangedMD5[] = "3137fdb27962671f5c3963a5e965eff5";
#else
  const char kChangedMD5[] = "a0c4ea6620772991f66bf7130379b08a";
#endif
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 200, kChangedMD5);
  }

  // Now save the result.
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  UnloadPage(page);

  // Re-open the file and check the changes were kept in the saved .pdf.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);
  EXPECT_EQ(2, FPDFPage_CountObjects(saved_page));
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(saved_page);
    CompareBitmap(page_bitmap.get(), 200, 200, kChangedMD5);
  }

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, RemovePageObject) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Show what the original file looks like.
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 200, kHelloWorldChecksum);
  }

  // Get the "Hello, world!" text object and remove it.
  ASSERT_EQ(2, FPDFPage_CountObjects(page));
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 0);
  ASSERT_TRUE(page_object);
  EXPECT_TRUE(FPDFPage_RemoveObject(page, page_object));

  // Verify the "Hello, world!" text is gone.
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 200, kFirstRemovedChecksum);
  }
  ASSERT_EQ(1, FPDFPage_CountObjects(page));

  UnloadPage(page);
  FPDFPageObj_Destroy(page_object);
}

void CheckMarkCounts(FPDF_PAGE page,
                     int start_from,
                     int expected_object_count,
                     size_t expected_prime_count,
                     size_t expected_square_count,
                     size_t expected_greater_than_ten_count,
                     size_t expected_bounds_count) {
  int object_count = FPDFPage_CountObjects(page);
  ASSERT_EQ(expected_object_count, object_count);

  size_t prime_count = 0;
  size_t square_count = 0;
  size_t greater_than_ten_count = 0;
  size_t bounds_count = 0;
  for (int i = 0; i < object_count; ++i) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);

    int mark_count = FPDFPageObj_CountMarks(page_object);
    for (int j = 0; j < mark_count; ++j) {
      FPDF_PAGEOBJECTMARK mark = FPDFPageObj_GetMark(page_object, j);

      char buffer[256];
      unsigned long name_len = 999u;
      ASSERT_TRUE(
          FPDFPageObjMark_GetName(mark, buffer, sizeof(buffer), &name_len));
      EXPECT_GT(name_len, 0u);
      EXPECT_NE(999u, name_len);
      std::wstring name =
          GetPlatformWString(reinterpret_cast<unsigned short*>(buffer));
      if (name == L"Prime") {
        prime_count++;
      } else if (name == L"Square") {
        square_count++;
        int expected_square = start_from + i;
        EXPECT_EQ(1, FPDFPageObjMark_CountParams(mark));

        unsigned long get_param_key_return = 999u;
        ASSERT_TRUE(FPDFPageObjMark_GetParamKey(mark, 0, buffer, sizeof(buffer),
                                                &get_param_key_return));
        EXPECT_EQ((6u + 1u) * 2u, get_param_key_return);
        std::wstring key =
            GetPlatformWString(reinterpret_cast<unsigned short*>(buffer));
        EXPECT_EQ(L"Factor", key);

        EXPECT_EQ(FPDF_OBJECT_NUMBER,
                  FPDFPageObjMark_GetParamValueType(mark, "Factor"));
        int square_root;
        EXPECT_TRUE(
            FPDFPageObjMark_GetParamIntValue(mark, "Factor", &square_root));
        EXPECT_EQ(expected_square, square_root * square_root);
      } else if (name == L"GreaterThanTen") {
        greater_than_ten_count++;
      } else if (name == L"Bounds") {
        bounds_count++;
        EXPECT_EQ(1, FPDFPageObjMark_CountParams(mark));

        unsigned long get_param_key_return = 999u;
        ASSERT_TRUE(FPDFPageObjMark_GetParamKey(mark, 0, buffer, sizeof(buffer),
                                                &get_param_key_return));
        EXPECT_EQ((8u + 1u) * 2u, get_param_key_return);
        std::wstring key =
            GetPlatformWString(reinterpret_cast<unsigned short*>(buffer));
        EXPECT_EQ(L"Position", key);

        EXPECT_EQ(FPDF_OBJECT_STRING,
                  FPDFPageObjMark_GetParamValueType(mark, "Position"));
        unsigned long length;
        EXPECT_TRUE(FPDFPageObjMark_GetParamStringValue(
            mark, "Position", buffer, sizeof(buffer), &length));
        ASSERT_GT(length, 0u);
        std::wstring value =
            GetPlatformWString(reinterpret_cast<unsigned short*>(buffer));

        // "Position" can be "First", "Last", or "End".
        if (i == 0) {
          EXPECT_EQ((5u + 1u) * 2u, length);
          EXPECT_EQ(L"First", value);
        } else if (i == object_count - 1) {
          if (length == (4u + 1u) * 2u) {
            EXPECT_EQ(L"Last", value);
          } else if (length == (3u + 1u) * 2u) {
            EXPECT_EQ(L"End", value);
          } else {
            FAIL();
          }
        } else {
          FAIL();
        }
      } else {
        FAIL();
      }
    }
  }

  // Expect certain number of tagged objects. The test file contains strings
  // from 1 to 19.
  EXPECT_EQ(expected_prime_count, prime_count);
  EXPECT_EQ(expected_square_count, square_count);
  EXPECT_EQ(expected_greater_than_ten_count, greater_than_ten_count);
  EXPECT_EQ(expected_bounds_count, bounds_count);
}

TEST_F(FPDFEditEmbedderTest, ReadMarkedObjectsIndirectDict) {
  // Load document with some text marked with an indirect property.
  EXPECT_TRUE(OpenDocument("text_in_page_marked_indirect.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  CheckMarkCounts(page, 1, 19, 8, 4, 9, 1);

  UnloadPage(page);
}

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_RemoveMarkedObjectsPrime DISABLED_RemoveMarkedObjectsPrime
#else
#define MAYBE_RemoveMarkedObjectsPrime RemoveMarkedObjectsPrime
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_RemoveMarkedObjectsPrime) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("text_in_page_marked.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Show what the original file looks like.
  {
#if defined(OS_MACOSX)
    const char kOriginalMD5[] = "adf815e53c788a5272b4df07c610a1da";
#elif defined(OS_WIN)
    const char kOriginalMD5[] = "00542ee435b37749c4453be63bf7bdb6";
#else
    const char kOriginalMD5[] = "41647268d5911d049801803b15c2dfb0";
#endif
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 200, kOriginalMD5);
  }

  constexpr int expected_object_count = 19;
  CheckMarkCounts(page, 1, expected_object_count, 8, 4, 9, 1);

  // Get all objects marked with "Prime"
  std::vector<FPDF_PAGEOBJECT> primes;
  for (int i = 0; i < expected_object_count; ++i) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);

    int mark_count = FPDFPageObj_CountMarks(page_object);
    for (int j = 0; j < mark_count; ++j) {
      FPDF_PAGEOBJECTMARK mark = FPDFPageObj_GetMark(page_object, j);

      char buffer[256];
      unsigned long name_len = 999u;
      ASSERT_TRUE(
          FPDFPageObjMark_GetName(mark, buffer, sizeof(buffer), &name_len));
      EXPECT_GT(name_len, 0u);
      EXPECT_NE(999u, name_len);
      std::wstring name =
          GetPlatformWString(reinterpret_cast<unsigned short*>(buffer));
      if (name == L"Prime") {
        primes.push_back(page_object);
      }
    }
  }

  // Remove all objects marked with "Prime".
  for (FPDF_PAGEOBJECT page_object : primes) {
    EXPECT_TRUE(FPDFPage_RemoveObject(page, page_object));
    FPDFPageObj_Destroy(page_object);
  }

  EXPECT_EQ(11, FPDFPage_CountObjects(page));

#if defined(OS_MACOSX)
  const char kNonPrimesMD5[] = "d29e2ddff56e0d12f340794d26796400";
  const char kNonPrimesAfterSaveMD5[] = "10eff2cd0037b661496981779601fa6f";
#elif defined(OS_WIN)
  const char kNonPrimesMD5[] = "86e371fdae30c2471f476631f3f93413";
  const char kNonPrimesAfterSaveMD5[] = "86e371fdae30c2471f476631f3f93413";
#else
  const char kNonPrimesMD5[] = "67ab13115d0cc34e99a1003c28047b40";
  const char kNonPrimesAfterSaveMD5[] = "67ab13115d0cc34e99a1003c28047b40";
#endif
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 200, kNonPrimesMD5);
  }

  // Save the file.
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  // Re-open the file and check the prime marks are not there anymore.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);
  EXPECT_EQ(11, FPDFPage_CountObjects(saved_page));

  {
    ScopedFPDFBitmap page_bitmap = RenderPage(saved_page);
    CompareBitmap(page_bitmap.get(), 200, 200, kNonPrimesAfterSaveMD5);
  }

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, RemoveMarks) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("text_in_page_marked.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  constexpr int kExpectedObjectCount = 19;
  CheckMarkCounts(page, 1, kExpectedObjectCount, 8, 4, 9, 1);

  // Remove all "Prime" content marks.
  for (int i = 0; i < kExpectedObjectCount; ++i) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);

    int mark_count = FPDFPageObj_CountMarks(page_object);
    for (int j = mark_count - 1; j >= 0; --j) {
      FPDF_PAGEOBJECTMARK mark = FPDFPageObj_GetMark(page_object, j);

      char buffer[256];
      unsigned long name_len = 999u;
      ASSERT_TRUE(
          FPDFPageObjMark_GetName(mark, buffer, sizeof(buffer), &name_len));
      EXPECT_GT(name_len, 0u);
      EXPECT_NE(999u, name_len);
      std::wstring name =
          GetPlatformWString(reinterpret_cast<unsigned short*>(buffer));
      if (name == L"Prime") {
        // Remove mark.
        EXPECT_TRUE(FPDFPageObj_RemoveMark(page_object, mark));

        // Verify there is now one fewer mark in the page object.
        EXPECT_EQ(mark_count - 1, FPDFPageObj_CountMarks(page_object));
      }
    }
  }

  // Verify there are 0 "Prime" content marks now.
  CheckMarkCounts(page, 1, kExpectedObjectCount, 0, 4, 9, 1);

  // Save the file.
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  // Re-open the file and check the prime marks are not there anymore.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);

  CheckMarkCounts(saved_page, 1, kExpectedObjectCount, 0, 4, 9, 1);

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, RemoveMarkParam) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("text_in_page_marked.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  constexpr int kExpectedObjectCount = 19;
  CheckMarkCounts(page, 1, kExpectedObjectCount, 8, 4, 9, 1);

  // Remove all "Square" content marks parameters.
  for (int i = 0; i < kExpectedObjectCount; ++i) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);

    int mark_count = FPDFPageObj_CountMarks(page_object);
    for (int j = 0; j < mark_count; ++j) {
      FPDF_PAGEOBJECTMARK mark = FPDFPageObj_GetMark(page_object, j);

      char buffer[256];
      unsigned long name_len = 999u;
      ASSERT_TRUE(
          FPDFPageObjMark_GetName(mark, buffer, sizeof(buffer), &name_len));
      EXPECT_GT(name_len, 0u);
      EXPECT_NE(999u, name_len);
      std::wstring name =
          GetPlatformWString(reinterpret_cast<unsigned short*>(buffer));
      if (name == L"Square") {
        // Show the mark has a "Factor" parameter.
        int out_value;
        EXPECT_TRUE(
            FPDFPageObjMark_GetParamIntValue(mark, "Factor", &out_value));

        // Remove parameter.
        EXPECT_TRUE(FPDFPageObjMark_RemoveParam(page_object, mark, "Factor"));

        // Verify the "Factor" parameter is gone.
        EXPECT_FALSE(
            FPDFPageObjMark_GetParamIntValue(mark, "Factor", &out_value));
      }
    }
  }

  // Save the file.
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  // Re-open the file and check the "Factor" parameters are still gone.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);

  size_t square_count = 0;
  for (int i = 0; i < kExpectedObjectCount; ++i) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(saved_page, i);

    int mark_count = FPDFPageObj_CountMarks(page_object);
    for (int j = 0; j < mark_count; ++j) {
      FPDF_PAGEOBJECTMARK mark = FPDFPageObj_GetMark(page_object, j);

      char buffer[256];
      unsigned long name_len = 999u;
      ASSERT_TRUE(
          FPDFPageObjMark_GetName(mark, buffer, sizeof(buffer), &name_len));
      EXPECT_GT(name_len, 0u);
      EXPECT_NE(999u, name_len);
      std::wstring name =
          GetPlatformWString(reinterpret_cast<unsigned short*>(buffer));
      if (name == L"Square") {
        // Verify the "Factor" parameter is still gone.
        int out_value;
        EXPECT_FALSE(
            FPDFPageObjMark_GetParamIntValue(mark, "Factor", &out_value));

        ++square_count;
      }
    }
  }

  // Verify the parameters are gone, but the marks are not.
  EXPECT_EQ(4u, square_count);

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, MaintainMarkedObjects) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("text_in_page_marked.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Iterate over all objects, counting the number of times each content mark
  // name appears.
  CheckMarkCounts(page, 1, 19, 8, 4, 9, 1);

  // Remove first page object.
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 0);
  EXPECT_TRUE(FPDFPage_RemoveObject(page, page_object));
  FPDFPageObj_Destroy(page_object);

  CheckMarkCounts(page, 2, 18, 8, 3, 9, 1);

  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  UnloadPage(page);

  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);

  CheckMarkCounts(saved_page, 2, 18, 8, 3, 9, 1);

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, MaintainIndirectMarkedObjects) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("text_in_page_marked_indirect.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Iterate over all objects, counting the number of times each content mark
  // name appears.
  CheckMarkCounts(page, 1, 19, 8, 4, 9, 1);

  // Remove first page object.
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 0);
  EXPECT_TRUE(FPDFPage_RemoveObject(page, page_object));
  FPDFPageObj_Destroy(page_object);

  CheckMarkCounts(page, 2, 18, 8, 3, 9, 1);

  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  UnloadPage(page);

  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);

  CheckMarkCounts(saved_page, 2, 18, 8, 3, 9, 1);

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, RemoveExistingPageObject) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Get the "Hello, world!" text object and remove it.
  ASSERT_EQ(2, FPDFPage_CountObjects(page));
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 0);
  ASSERT_TRUE(page_object);
  EXPECT_TRUE(FPDFPage_RemoveObject(page, page_object));

  // Verify the "Hello, world!" text is gone.
  ASSERT_EQ(1, FPDFPage_CountObjects(page));

  // Save the file
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);
  FPDFPageObj_Destroy(page_object);

  // Re-open the file and check the page object count is still 1.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);
  EXPECT_EQ(1, FPDFPage_CountObjects(saved_page));
  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_RemoveExistingPageObjectSplitStreamsNotLonely \
  DISABLED_RemoveExistingPageObjectSplitStreamsNotLonely
#else
#define MAYBE_RemoveExistingPageObjectSplitStreamsNotLonely \
  RemoveExistingPageObjectSplitStreamsNotLonely
#endif
TEST_F(FPDFEditEmbedderTest,
       MAYBE_RemoveExistingPageObjectSplitStreamsNotLonely) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("hello_world_split_streams.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Get the "Hello, world!" text object and remove it. There is another object
  // in the same stream that says "Goodbye, world!"
  ASSERT_EQ(3, FPDFPage_CountObjects(page));
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 0);
  ASSERT_TRUE(page_object);
  EXPECT_TRUE(FPDFPage_RemoveObject(page, page_object));

  // Verify the "Hello, world!" text is gone.
  ASSERT_EQ(2, FPDFPage_CountObjects(page));
#if defined(OS_MACOSX)
  const char kHelloRemovedMD5[] = "3b3b27602a86dfe5996a33c42c59885b";
#elif defined(OS_WIN)
  const char kHelloRemovedMD5[] = "a97d4c72c969ba373c2dce675d277e65";
#else
  const char kHelloRemovedMD5[] = "95b92950647a2190e1230911e7a1a0e9";
#endif
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 200, kHelloRemovedMD5);
  }

  // Save the file
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);
  FPDFPageObj_Destroy(page_object);

  // Re-open the file and check the page object count is still 2.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);

  EXPECT_EQ(2, FPDFPage_CountObjects(saved_page));
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(saved_page);
    CompareBitmap(page_bitmap.get(), 200, 200, kHelloRemovedMD5);
  }

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, RemoveExistingPageObjectSplitStreamsLonely) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("hello_world_split_streams.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Get the "Greetings, world!" text object and remove it. This is the only
  // object in the stream.
  ASSERT_EQ(3, FPDFPage_CountObjects(page));
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 2);
  ASSERT_TRUE(page_object);
  EXPECT_TRUE(FPDFPage_RemoveObject(page, page_object));

  // Verify the "Greetings, world!" text is gone.
  ASSERT_EQ(2, FPDFPage_CountObjects(page));
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 200, kHelloWorldChecksum);
  }

  // Save the file
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);
  FPDFPageObj_Destroy(page_object);

  // Re-open the file and check the page object count is still 2.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);

  EXPECT_EQ(2, FPDFPage_CountObjects(saved_page));
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(saved_page);
    CompareBitmap(page_bitmap.get(), 200, 200, kHelloWorldChecksum);
  }

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, GetContentStream) {
  // Load document with some text split across streams.
  EXPECT_TRUE(OpenDocument("split_streams.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Content stream 0: page objects 0-14.
  // Content stream 1: page objects 15-17.
  // Content stream 2: page object 18.
  ASSERT_EQ(19, FPDFPage_CountObjects(page));
  for (int i = 0; i < 19; i++) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);
    ASSERT_TRUE(page_object);
    CPDF_PageObject* cpdf_page_object =
        CPDFPageObjectFromFPDFPageObject(page_object);
    if (i < 15)
      EXPECT_EQ(0, cpdf_page_object->GetContentStream()) << i;
    else if (i < 18)
      EXPECT_EQ(1, cpdf_page_object->GetContentStream()) << i;
    else
      EXPECT_EQ(2, cpdf_page_object->GetContentStream()) << i;
  }

  UnloadPage(page);
}

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_RemoveAllFromStream DISABLED_RemoveAllFromStream
#else
#define MAYBE_RemoveAllFromStream RemoveAllFromStream
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_RemoveAllFromStream) {
  // Load document with some text split across streams.
  EXPECT_TRUE(OpenDocument("split_streams.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Content stream 0: page objects 0-14.
  // Content stream 1: page objects 15-17.
  // Content stream 2: page object 18.
  ASSERT_EQ(19, FPDFPage_CountObjects(page));

  // Loop backwards because objects will being removed, which shifts the indexes
  // after the removed position.
  for (int i = 18; i >= 0; i--) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);
    ASSERT_TRUE(page_object);
    CPDF_PageObject* cpdf_page_object =
        CPDFPageObjectFromFPDFPageObject(page_object);

    // Empty content stream 1.
    if (cpdf_page_object->GetContentStream() == 1) {
      EXPECT_TRUE(FPDFPage_RemoveObject(page, page_object));
      FPDFPageObj_Destroy(page_object);
    }
  }

  // Content stream 0: page objects 0-14.
  // Content stream 2: page object 15.
  ASSERT_EQ(16, FPDFPage_CountObjects(page));
  for (int i = 0; i < 16; i++) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);
    ASSERT_TRUE(page_object);
    CPDF_PageObject* cpdf_page_object =
        CPDFPageObjectFromFPDFPageObject(page_object);
    if (i < 15)
      EXPECT_EQ(0, cpdf_page_object->GetContentStream()) << i;
    else
      EXPECT_EQ(2, cpdf_page_object->GetContentStream()) << i;
  }

  // Generate contents should remove the empty stream and update the page
  // objects' contents stream indexes.
  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  // Content stream 0: page objects 0-14.
  // Content stream 1: page object 15.
  ASSERT_EQ(16, FPDFPage_CountObjects(page));
  for (int i = 0; i < 16; i++) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);
    ASSERT_TRUE(page_object);
    CPDF_PageObject* cpdf_page_object =
        CPDFPageObjectFromFPDFPageObject(page_object);
    if (i < 15)
      EXPECT_EQ(0, cpdf_page_object->GetContentStream()) << i;
    else
      EXPECT_EQ(1, cpdf_page_object->GetContentStream()) << i;
  }

#if defined(OS_MACOSX)
  const char kStream1RemovedMD5[] = "0e8856ca9abc7049412e64f9230c7c43";
#elif defined(OS_WIN)
  const char kStream1RemovedMD5[] = "b4140f203523e38793283a5943d8075b";
#else
  const char kStream1RemovedMD5[] = "e86a3efc160ede6cfcb1f59bcacf1105";
#endif
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 200, kStream1RemovedMD5);
  }

  // Save the file
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  // Re-open the file and check the page object count is still 16, and that
  // content stream 1 was removed.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);

  // Content stream 0: page objects 0-14.
  // Content stream 1: page object 15.
  EXPECT_EQ(16, FPDFPage_CountObjects(saved_page));
  for (int i = 0; i < 16; i++) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(saved_page, i);
    ASSERT_TRUE(page_object);
    CPDF_PageObject* cpdf_page_object =
        CPDFPageObjectFromFPDFPageObject(page_object);
    if (i < 15)
      EXPECT_EQ(0, cpdf_page_object->GetContentStream()) << i;
    else
      EXPECT_EQ(1, cpdf_page_object->GetContentStream()) << i;
  }

  {
    ScopedFPDFBitmap page_bitmap = RenderPage(saved_page);
    CompareBitmap(page_bitmap.get(), 200, 200, kStream1RemovedMD5);
  }

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, RemoveAllFromSingleStream) {
  // Load document with a single stream.
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Content stream 0: page objects 0-1.
  ASSERT_EQ(2, FPDFPage_CountObjects(page));

  // Loop backwards because objects will being removed, which shifts the indexes
  // after the removed position.
  for (int i = 1; i >= 0; i--) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);
    ASSERT_TRUE(page_object);
    CPDF_PageObject* cpdf_page_object =
        CPDFPageObjectFromFPDFPageObject(page_object);
    ASSERT_EQ(0, cpdf_page_object->GetContentStream());
    ASSERT_TRUE(FPDFPage_RemoveObject(page, page_object));
    FPDFPageObj_Destroy(page_object);
  }

  // No more objects in the stream
  ASSERT_EQ(0, FPDFPage_CountObjects(page));

  // Generate contents should remove the empty stream and update the page
  // objects' contents stream indexes.
  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  ASSERT_EQ(0, FPDFPage_CountObjects(page));

  const char kAllRemovedMD5[] = "eee4600ac08b458ac7ac2320e225674c";
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 200, kAllRemovedMD5);
  }

  // Save the file
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  // Re-open the file and check the page object count is still 0.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);

  EXPECT_EQ(0, FPDFPage_CountObjects(saved_page));
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(saved_page);
    CompareBitmap(page_bitmap.get(), 200, 200, kAllRemovedMD5);
  }

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, RemoveFirstFromSingleStream) {
  // Load document with a single stream.
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Content stream 0: page objects 0-1.
  ASSERT_EQ(2, FPDFPage_CountObjects(page));

  // Remove first object.
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 0);
  ASSERT_TRUE(page_object);
  CPDF_PageObject* cpdf_page_object =
      CPDFPageObjectFromFPDFPageObject(page_object);
  ASSERT_EQ(0, cpdf_page_object->GetContentStream());
  ASSERT_TRUE(FPDFPage_RemoveObject(page, page_object));
  FPDFPageObj_Destroy(page_object);

  // One object left in the stream.
  ASSERT_EQ(1, FPDFPage_CountObjects(page));
  page_object = FPDFPage_GetObject(page, 0);
  ASSERT_TRUE(page_object);
  cpdf_page_object = CPDFPageObjectFromFPDFPageObject(page_object);
  ASSERT_EQ(0, cpdf_page_object->GetContentStream());

  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  // Still one object left in the stream.
  ASSERT_EQ(1, FPDFPage_CountObjects(page));
  page_object = FPDFPage_GetObject(page, 0);
  ASSERT_TRUE(page_object);
  cpdf_page_object = CPDFPageObjectFromFPDFPageObject(page_object);
  ASSERT_EQ(0, cpdf_page_object->GetContentStream());

  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 200, kFirstRemovedChecksum);
  }

  // Save the file
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  // Re-open the file and check the page object count is still 0.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);

  ASSERT_EQ(1, FPDFPage_CountObjects(saved_page));
  page_object = FPDFPage_GetObject(saved_page, 0);
  ASSERT_TRUE(page_object);
  cpdf_page_object = CPDFPageObjectFromFPDFPageObject(page_object);
  ASSERT_EQ(0, cpdf_page_object->GetContentStream());
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(saved_page);
    CompareBitmap(page_bitmap.get(), 200, 200, kFirstRemovedChecksum);
  }

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, RemoveLastFromSingleStream) {
  // Load document with a single stream.
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Content stream 0: page objects 0-1.
  ASSERT_EQ(2, FPDFPage_CountObjects(page));

  // Remove last object
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 1);
  ASSERT_TRUE(page_object);
  CPDF_PageObject* cpdf_page_object =
      CPDFPageObjectFromFPDFPageObject(page_object);
  ASSERT_EQ(0, cpdf_page_object->GetContentStream());
  ASSERT_TRUE(FPDFPage_RemoveObject(page, page_object));
  FPDFPageObj_Destroy(page_object);

  // One object left in the stream.
  ASSERT_EQ(1, FPDFPage_CountObjects(page));
  page_object = FPDFPage_GetObject(page, 0);
  ASSERT_TRUE(page_object);
  cpdf_page_object = CPDFPageObjectFromFPDFPageObject(page_object);
  ASSERT_EQ(0, cpdf_page_object->GetContentStream());

  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  // Still one object left in the stream.
  ASSERT_EQ(1, FPDFPage_CountObjects(page));
  page_object = FPDFPage_GetObject(page, 0);
  ASSERT_TRUE(page_object);
  cpdf_page_object = CPDFPageObjectFromFPDFPageObject(page_object);
  ASSERT_EQ(0, cpdf_page_object->GetContentStream());

  using pdfium::kHelloWorldRemovedChecksum;
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 200, kHelloWorldRemovedChecksum);
  }

  // Save the file
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  // Re-open the file and check the page object count is still 0.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);

  ASSERT_EQ(1, FPDFPage_CountObjects(saved_page));
  page_object = FPDFPage_GetObject(saved_page, 0);
  ASSERT_TRUE(page_object);
  cpdf_page_object = CPDFPageObjectFromFPDFPageObject(page_object);
  ASSERT_EQ(0, cpdf_page_object->GetContentStream());
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(saved_page);
    CompareBitmap(page_bitmap.get(), 200, 200, kHelloWorldRemovedChecksum);
  }

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, RemoveAllFromMultipleStreams) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("hello_world_split_streams.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Content stream 0: page objects 0-1.
  // Content stream 1: page object 2.
  ASSERT_EQ(3, FPDFPage_CountObjects(page));

  // Loop backwards because objects will being removed, which shifts the indexes
  // after the removed position.
  for (int i = 2; i >= 0; i--) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);
    ASSERT_TRUE(page_object);
    ASSERT_TRUE(FPDFPage_RemoveObject(page, page_object));
    FPDFPageObj_Destroy(page_object);
  }

  // No more objects in the page.
  ASSERT_EQ(0, FPDFPage_CountObjects(page));

  // Generate contents should remove the empty streams and update the page
  // objects' contents stream indexes.
  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  ASSERT_EQ(0, FPDFPage_CountObjects(page));

  const char kAllRemovedMD5[] = "eee4600ac08b458ac7ac2320e225674c";
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 200, kAllRemovedMD5);
  }

  // Save the file
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  // Re-open the file and check the page object count is still 0.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);

  EXPECT_EQ(0, FPDFPage_CountObjects(saved_page));
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(saved_page);
    CompareBitmap(page_bitmap.get(), 200, 200, kAllRemovedMD5);
  }

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, InsertPageObjectAndSave) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Add a red rectangle.
  ASSERT_EQ(2, FPDFPage_CountObjects(page));
  FPDF_PAGEOBJECT red_rect = FPDFPageObj_CreateNewRect(20, 100, 50, 50);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(red_rect, 255, 0, 0, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(red_rect, FPDF_FILLMODE_ALTERNATE, 0));
  FPDFPage_InsertObject(page, red_rect);

  // Verify the red rectangle was added.
  ASSERT_EQ(3, FPDFPage_CountObjects(page));

  // Save the file
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  // Re-open the file and check the page object count is still 3.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);
  EXPECT_EQ(3, FPDFPage_CountObjects(saved_page));
  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, InsertPageObjectEditAndSave) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Add a red rectangle.
  ASSERT_EQ(2, FPDFPage_CountObjects(page));
  FPDF_PAGEOBJECT red_rect = FPDFPageObj_CreateNewRect(20, 100, 50, 50);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(red_rect, 255, 100, 100, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(red_rect, FPDF_FILLMODE_ALTERNATE, 0));
  FPDFPage_InsertObject(page, red_rect);

  // Verify the red rectangle was added.
  ASSERT_EQ(3, FPDFPage_CountObjects(page));

  // Generate content but change it again
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDFPageObj_SetFillColor(red_rect, 255, 0, 0, 255));

  // Save the file
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  // Re-open the file and check the page object count is still 3.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);
  EXPECT_EQ(3, FPDFPage_CountObjects(saved_page));
  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_InsertAndRemoveLargeFile DISABLED_InsertAndRemoveLargeFile
#else
#define MAYBE_InsertAndRemoveLargeFile InsertAndRemoveLargeFile
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_InsertAndRemoveLargeFile) {
  const int kOriginalObjectCount = 600;

  // Load document with many objects.
  EXPECT_TRUE(OpenDocument("many_rectangles.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  using pdfium::kManyRectanglesChecksum;
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 300, kManyRectanglesChecksum);
  }

  // Add a black rectangle.
  ASSERT_EQ(kOriginalObjectCount, FPDFPage_CountObjects(page));
  FPDF_PAGEOBJECT black_rect = FPDFPageObj_CreateNewRect(20, 100, 50, 50);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(black_rect, 0, 0, 0, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(black_rect, FPDF_FILLMODE_ALTERNATE, 0));
  FPDFPage_InsertObject(page, black_rect);

  // Verify the black rectangle was added.
  ASSERT_EQ(kOriginalObjectCount + 1, FPDFPage_CountObjects(page));
  const char kPlusRectangleMD5[] = "6b9396ab570754b32b04ca629e902f77";
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 300, kPlusRectangleMD5);
  }

  // Save the file.
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  // Re-open the file and check the rectangle added is still there.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);
  EXPECT_EQ(kOriginalObjectCount + 1, FPDFPage_CountObjects(saved_page));
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(saved_page);
    CompareBitmap(page_bitmap.get(), 200, 300, kPlusRectangleMD5);
  }

  // Remove the added rectangle.
  FPDF_PAGEOBJECT added_object =
      FPDFPage_GetObject(saved_page, kOriginalObjectCount);
  EXPECT_TRUE(FPDFPage_RemoveObject(saved_page, added_object));
  FPDFPageObj_Destroy(added_object);
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(saved_page);
    CompareBitmap(page_bitmap.get(), 200, 300, kManyRectanglesChecksum);
  }
  EXPECT_EQ(kOriginalObjectCount, FPDFPage_CountObjects(saved_page));

  // Save the file again.
  EXPECT_TRUE(FPDFPage_GenerateContent(saved_page));
  EXPECT_TRUE(FPDF_SaveAsCopy(saved_document_, this, 0));

  CloseSavedPage(saved_page);
  CloseSavedDocument();

  // Re-open the file (again) and check the black rectangle was removed and the
  // rest is intact.
  ASSERT_TRUE(OpenSavedDocument());
  saved_page = LoadSavedPage(0);
  EXPECT_EQ(kOriginalObjectCount, FPDFPage_CountObjects(saved_page));
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(saved_page);
    CompareBitmap(page_bitmap.get(), 200, 300, kManyRectanglesChecksum);
  }

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, AddAndRemovePaths) {
  // Start with a blank page.
  FPDF_PAGE page = FPDFPage_New(CreateNewDocument(), 0, 612, 792);
  ASSERT_TRUE(page);

  // Render the blank page and verify it's a blank bitmap.
  using pdfium::kBlankPage612By792Checksum;
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792, kBlankPage612By792Checksum);
  }
  ASSERT_EQ(0, FPDFPage_CountObjects(page));

  // Add a red rectangle.
  FPDF_PAGEOBJECT red_rect = FPDFPageObj_CreateNewRect(10, 10, 20, 20);
  ASSERT_TRUE(red_rect);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(red_rect, 255, 0, 0, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(red_rect, FPDF_FILLMODE_ALTERNATE, 0));
  FPDFPage_InsertObject(page, red_rect);
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792, kRedRectangleChecksum);
  }
  EXPECT_EQ(1, FPDFPage_CountObjects(page));

  // Remove rectangle and verify it does not render anymore and the bitmap is
  // back to a blank one.
  EXPECT_TRUE(FPDFPage_RemoveObject(page, red_rect));
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792, kBlankPage612By792Checksum);
  }
  EXPECT_EQ(0, FPDFPage_CountObjects(page));

  // Trying to remove an object not in the page should return false.
  EXPECT_FALSE(FPDFPage_RemoveObject(page, red_rect));

  FPDF_ClosePage(page);
  FPDFPageObj_Destroy(red_rect);
}

TEST_F(FPDFEditEmbedderTest, PathsPoints) {
  CreateNewDocument();
  FPDF_PAGEOBJECT img = FPDFPageObj_NewImageObj(document_);
  // This should fail gracefully, even if img is not a path.
  ASSERT_EQ(-1, FPDFPath_CountSegments(img));

  // This should fail gracefully, even if path is NULL.
  ASSERT_EQ(-1, FPDFPath_CountSegments(nullptr));

  // FPDFPath_GetPathSegment() with a non-path.
  ASSERT_EQ(nullptr, FPDFPath_GetPathSegment(img, 0));
  // FPDFPath_GetPathSegment() with a NULL path.
  ASSERT_EQ(nullptr, FPDFPath_GetPathSegment(nullptr, 0));
  float x;
  float y;
  // FPDFPathSegment_GetPoint() with a NULL segment.
  EXPECT_FALSE(FPDFPathSegment_GetPoint(nullptr, &x, &y));

  // FPDFPathSegment_GetType() with a NULL segment.
  ASSERT_EQ(FPDF_SEGMENT_UNKNOWN, FPDFPathSegment_GetType(nullptr));

  // FPDFPathSegment_GetClose() with a NULL segment.
  EXPECT_FALSE(FPDFPathSegment_GetClose(nullptr));

  FPDFPageObj_Destroy(img);
}

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_PathOnTopOfText DISABLED_PathOnTopOfText
#else
#define MAYBE_PathOnTopOfText PathOnTopOfText
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_PathOnTopOfText) {
  // Load document with some text
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Add an opaque rectangle on top of some of the text.
  FPDF_PAGEOBJECT red_rect = FPDFPageObj_CreateNewRect(20, 100, 50, 50);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(red_rect, 255, 0, 0, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(red_rect, FPDF_FILLMODE_ALTERNATE, 0));
  FPDFPage_InsertObject(page, red_rect);

  // Add a transparent triangle on top of other part of the text.
  FPDF_PAGEOBJECT black_path = FPDFPageObj_CreateNewPath(20, 50);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(black_path, 0, 0, 0, 100));
  EXPECT_TRUE(FPDFPath_SetDrawMode(black_path, FPDF_FILLMODE_ALTERNATE, 0));
  EXPECT_TRUE(FPDFPath_LineTo(black_path, 30, 80));
  EXPECT_TRUE(FPDFPath_LineTo(black_path, 40, 10));
  EXPECT_TRUE(FPDFPath_Close(black_path));
  FPDFPage_InsertObject(page, black_path);

  // Render and check the result. Text is slightly different on Mac.
  ScopedFPDFBitmap bitmap = RenderLoadedPage(page);
#if defined(OS_MACOSX)
  const char md5[] = "e55bcd1facb7243dc6e16dd5f912265b";
#elif defined(OS_WIN)
  const char md5[] = "74dd9c393b8b2578d2b7feb032b7daad";
#else
  const char md5[] = "aa71b09b93b55f467f1290e5111babee";
#endif
  CompareBitmap(bitmap.get(), 200, 200, md5);
  UnloadPage(page);
}

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_EditOverExistingContent DISABLED_EditOverExistingContent
#else
#define MAYBE_EditOverExistingContent EditOverExistingContent
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_EditOverExistingContent) {
  // Load document with existing content
  EXPECT_TRUE(OpenDocument("bug_717.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Add a transparent rectangle on top of the existing content
  FPDF_PAGEOBJECT red_rect2 = FPDFPageObj_CreateNewRect(90, 700, 25, 50);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(red_rect2, 255, 0, 0, 100));
  EXPECT_TRUE(FPDFPath_SetDrawMode(red_rect2, FPDF_FILLMODE_ALTERNATE, 0));
  FPDFPage_InsertObject(page, red_rect2);

  // Add an opaque rectangle on top of the existing content
  FPDF_PAGEOBJECT red_rect = FPDFPageObj_CreateNewRect(115, 700, 25, 50);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(red_rect, 255, 0, 0, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(red_rect, FPDF_FILLMODE_ALTERNATE, 0));
  FPDFPage_InsertObject(page, red_rect);

  ScopedFPDFBitmap bitmap = RenderLoadedPage(page);
  CompareBitmap(bitmap.get(), 612, 792, "ad04e5bd0f471a9a564fb034bd0fb073");
  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  // Now save the result, closing the page and document
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);
  VerifySavedRendering(saved_page, 612, 792,
                       "ad04e5bd0f471a9a564fb034bd0fb073");

  ClearString();
  // Add another opaque rectangle on top of the existing content
  FPDF_PAGEOBJECT green_rect = FPDFPageObj_CreateNewRect(150, 700, 25, 50);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(green_rect, 0, 255, 0, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(green_rect, FPDF_FILLMODE_ALTERNATE, 0));
  FPDFPage_InsertObject(saved_page, green_rect);

  // Add another transparent rectangle on top of existing content
  FPDF_PAGEOBJECT green_rect2 = FPDFPageObj_CreateNewRect(175, 700, 25, 50);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(green_rect2, 0, 255, 0, 100));
  EXPECT_TRUE(FPDFPath_SetDrawMode(green_rect2, FPDF_FILLMODE_ALTERNATE, 0));
  FPDFPage_InsertObject(saved_page, green_rect2);
  const char kLastMD5[] = "4b5b00f824620f8c9b8801ebb98e1cdd";
  {
    ScopedFPDFBitmap new_bitmap = RenderSavedPage(saved_page);
    CompareBitmap(new_bitmap.get(), 612, 792, kLastMD5);
  }
  EXPECT_TRUE(FPDFPage_GenerateContent(saved_page));

  // Now save the result, closing the page and document
  EXPECT_TRUE(FPDF_SaveAsCopy(saved_document_, this, 0));

  CloseSavedPage(saved_page);
  CloseSavedDocument();

  // Render the saved result
  VerifySavedDocument(612, 792, kLastMD5);
}

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_AddStrokedPaths DISABLED_AddStrokedPaths
#else
#define MAYBE_AddStrokedPaths AddStrokedPaths
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_AddStrokedPaths) {
  // Start with a blank page
  FPDF_PAGE page = FPDFPage_New(CreateNewDocument(), 0, 612, 792);

  // Add a large stroked rectangle (fill color should not affect it).
  FPDF_PAGEOBJECT rect = FPDFPageObj_CreateNewRect(20, 20, 200, 400);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(rect, 255, 0, 0, 255));
  EXPECT_TRUE(FPDFPageObj_SetStrokeColor(rect, 0, 255, 0, 255));
  EXPECT_TRUE(FPDFPageObj_SetStrokeWidth(rect, 15.0f));

  float width = 0;
  EXPECT_TRUE(FPDFPageObj_GetStrokeWidth(rect, &width));
  EXPECT_EQ(15.0f, width);

  EXPECT_TRUE(FPDFPath_SetDrawMode(rect, 0, 1));
  FPDFPage_InsertObject(page, rect);
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792,
                  "64bd31f862a89e0a9e505a5af6efd506");
  }

  // Add crossed-checkmark
  FPDF_PAGEOBJECT check = FPDFPageObj_CreateNewPath(300, 500);
  EXPECT_TRUE(FPDFPath_LineTo(check, 400, 400));
  EXPECT_TRUE(FPDFPath_LineTo(check, 600, 600));
  EXPECT_TRUE(FPDFPath_MoveTo(check, 400, 600));
  EXPECT_TRUE(FPDFPath_LineTo(check, 600, 400));
  EXPECT_TRUE(FPDFPageObj_SetStrokeColor(check, 128, 128, 128, 180));
  EXPECT_TRUE(FPDFPageObj_SetStrokeWidth(check, 8.35f));
  EXPECT_TRUE(FPDFPath_SetDrawMode(check, 0, 1));
  FPDFPage_InsertObject(page, check);
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792,
                  "4b6f3b9d25c4e194821217d5016c3724");
  }

  // Add stroked and filled oval-ish path.
  FPDF_PAGEOBJECT path = FPDFPageObj_CreateNewPath(250, 100);
  EXPECT_TRUE(FPDFPath_BezierTo(path, 180, 166, 180, 233, 250, 300));
  EXPECT_TRUE(FPDFPath_LineTo(path, 255, 305));
  EXPECT_TRUE(FPDFPath_BezierTo(path, 325, 233, 325, 166, 255, 105));
  EXPECT_TRUE(FPDFPath_Close(path));
  EXPECT_TRUE(FPDFPageObj_SetFillColor(path, 200, 128, 128, 100));
  EXPECT_TRUE(FPDFPageObj_SetStrokeColor(path, 128, 200, 128, 150));
  EXPECT_TRUE(FPDFPageObj_SetStrokeWidth(path, 10.5f));
  EXPECT_TRUE(FPDFPath_SetDrawMode(path, FPDF_FILLMODE_ALTERNATE, 1));
  FPDFPage_InsertObject(page, path);
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792,
                  "ff3e6a22326754944cc6e56609acd73b");
  }
  FPDF_ClosePage(page);
}

// Tests adding text from standard font using FPDFPageObj_NewTextObj.
// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_AddStandardFontText DISABLED_AddStandardFontText
#else
#define MAYBE_AddStandardFontText AddStandardFontText
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_AddStandardFontText) {
  // Start with a blank page
  FPDF_PAGE page = FPDFPage_New(CreateNewDocument(), 0, 612, 792);

  // Add some text to the page
  FPDF_PAGEOBJECT text_object1 =
      FPDFPageObj_NewTextObj(document(), "Arial", 12.0f);
  EXPECT_TRUE(text_object1);
  ScopedFPDFWideString text1 =
      GetFPDFWideString(L"I'm at the bottom of the page");
  EXPECT_TRUE(FPDFText_SetText(text_object1, text1.get()));
  FPDFPageObj_Transform(text_object1, 1, 0, 0, 1, 20, 20);
  FPDFPage_InsertObject(page, text_object1);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
#if defined(OS_MACOSX)
    const char md5[] = "324e1db8164a040cf6104538baa95ba6";
#elif defined(OS_WIN)
    const char md5[] = "08d1ff3e5a42801bee6077fd366bef00";
#else
    const char md5[] = "eacaa24573b8ce997b3882595f096f00";
#endif
    CompareBitmap(page_bitmap.get(), 612, 792, md5);

    EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
    VerifySavedDocument(612, 792, md5);
  }

  // Try another font
  FPDF_PAGEOBJECT text_object2 =
      FPDFPageObj_NewTextObj(document(), "TimesNewRomanBold", 15.0f);
  EXPECT_TRUE(text_object2);
  ScopedFPDFWideString text2 =
      GetFPDFWideString(L"Hi, I'm Bold. Times New Roman Bold.");
  EXPECT_TRUE(FPDFText_SetText(text_object2, text2.get()));
  FPDFPageObj_Transform(text_object2, 1, 0, 0, 1, 100, 600);
  FPDFPage_InsertObject(page, text_object2);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
#if defined(OS_MACOSX)
    const char md5[] = "26a516d923b0a18fbea0a24e3aca5562";
#elif defined(OS_WIN)
    const char md5[] = "3755dd35abd4c605755369401ee85b2d";
#else
    const char md5[] = "76fcc7d08aa15445efd2e2ceb7c6cc3b";
#endif
    CompareBitmap(page_bitmap.get(), 612, 792, md5);

    EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
    VerifySavedDocument(612, 792, md5);
  }

  // And some randomly transformed text
  FPDF_PAGEOBJECT text_object3 =
      FPDFPageObj_NewTextObj(document(), "Courier-Bold", 20.0f);
  EXPECT_TRUE(text_object3);
  ScopedFPDFWideString text3 = GetFPDFWideString(L"Can you read me? <:)>");
  EXPECT_TRUE(FPDFText_SetText(text_object3, text3.get()));
  FPDFPageObj_Transform(text_object3, 1, 1.5, 2, 0.5, 200, 200);
  FPDFPage_InsertObject(page, text_object3);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
#if defined(OS_MACOSX)
    const char md5[] = "532024c9ded47843313bb64a060118f3";
#elif defined(OS_WIN)
    const char md5[] = "5ded49fe157f89627903553771431e3d";
#else
    const char md5[] = "344534539aa7c5cc78404cfff4bde7fb";
#endif
    CompareBitmap(page_bitmap.get(), 612, 792, md5);

    EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
    VerifySavedDocument(612, 792, md5);
  }

  FS_MATRIX matrix;
  EXPECT_FALSE(FPDFTextObj_GetMatrix(nullptr, &matrix));
  EXPECT_TRUE(FPDFTextObj_GetMatrix(text_object3, &matrix));
  EXPECT_FLOAT_EQ(1.0f, matrix.a);
  EXPECT_FLOAT_EQ(1.5f, matrix.b);
  EXPECT_FLOAT_EQ(2.0f, matrix.c);
  EXPECT_FLOAT_EQ(0.5f, matrix.d);
  EXPECT_FLOAT_EQ(200.0f, matrix.e);
  EXPECT_FLOAT_EQ(200.0f, matrix.f);

  EXPECT_EQ(0, FPDFTextObj_GetFontSize(nullptr));
  EXPECT_EQ(20, FPDFTextObj_GetFontSize(text_object3));

  // TODO(npm): Why are there issues with text rotated by 90 degrees?
  // TODO(npm): FPDF_SaveAsCopy not giving the desired result after this.
  FPDF_ClosePage(page);
}

TEST_F(FPDFEditEmbedderTest, GetTextRenderMode) {
  EXPECT_TRUE(OpenDocument("text_render_mode.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  ASSERT_EQ(2, FPDFPage_CountObjects(page));

  EXPECT_EQ(FPDF_TEXTRENDERMODE_UNKNOWN,
            FPDFTextObj_GetTextRenderMode(nullptr));

  FPDF_PAGEOBJECT fill = FPDFPage_GetObject(page, 0);
  EXPECT_EQ(FPDF_TEXTRENDERMODE_FILL, FPDFTextObj_GetTextRenderMode(fill));

  FPDF_PAGEOBJECT stroke = FPDFPage_GetObject(page, 1);
  EXPECT_EQ(FPDF_TEXTRENDERMODE_STROKE, FPDFTextObj_GetTextRenderMode(stroke));

  UnloadPage(page);
}

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_SetTextRenderMode DISABLED_SetTextRenderMode
#else
#define MAYBE_SetTextRenderMode SetTextRenderMode
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_SetTextRenderMode) {
// Platform specific original bitmaps
#if defined(OS_MACOSX)
  const char md5[] = "2453a9a524ee3f5f525dd21345ec4d81";
#elif defined(OS_WIN)
  const char md5[] = "de6e86bad3e9fda753a8471a45cfbb58";
#else
  const char md5[] = "5a012d2920ac075c39ffa9437ea42faa";
#endif
  const char md5_stroke[] = "412e52e621b46bd77baf2162e1fb1a1d";

  {
    EXPECT_TRUE(OpenDocument("text_render_mode.pdf"));
    FPDF_PAGE page = LoadPage(0);
    ASSERT_TRUE(page);
    ASSERT_EQ(2, FPDFPage_CountObjects(page));

    // Check the bitmap
    {
      ScopedFPDFBitmap page_bitmap = RenderPage(page);
      CompareBitmap(page_bitmap.get(), 612, 446, md5);
    }

    // Cannot set on a null object.
    EXPECT_FALSE(
        FPDFTextObj_SetTextRenderMode(nullptr, FPDF_TEXTRENDERMODE_UNKNOWN));
    EXPECT_FALSE(
        FPDFTextObj_SetTextRenderMode(nullptr, FPDF_TEXTRENDERMODE_INVISIBLE));

    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 0);
    ASSERT_TRUE(page_object);
    EXPECT_EQ(FPDF_TEXTRENDERMODE_FILL,
              FPDFTextObj_GetTextRenderMode(page_object));

    // Cannot set UNKNOWN as a render mode.
    EXPECT_FALSE(FPDFTextObj_SetTextRenderMode(page_object,
                                               FPDF_TEXTRENDERMODE_UNKNOWN));

    EXPECT_TRUE(
        FPDFTextObj_SetTextRenderMode(page_object, FPDF_TEXTRENDERMODE_STROKE));
    EXPECT_EQ(FPDF_TEXTRENDERMODE_STROKE,
              FPDFTextObj_GetTextRenderMode(page_object));

    // Check that bitmap displays changed content
    {
      ScopedFPDFBitmap page_bitmap = RenderPage(page);
      CompareBitmap(page_bitmap.get(), 612, 446, md5_stroke);
    }

    UnloadPage(page);
  }

  {
    // Save a copy, open the copy, and render it.
    // Note that the text render mode should be set to
    // |FPDF_TEXTRENDERMODE_STROKE|, but isn't.
    // TODO(crbug.com/pdfium/1470): Fix FPDFTextObj_SetTextRenderMode() to work
    // on saved documents.
    EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
    ASSERT_TRUE(OpenSavedDocument());
    FPDF_PAGE saved_page = LoadSavedPage(0);
    ASSERT_TRUE(saved_page);

    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(saved_page, 0);
    EXPECT_TRUE(page_object);
    EXPECT_EQ(FPDF_TEXTRENDERMODE_FILL,
              FPDFTextObj_GetTextRenderMode(page_object));

    ScopedFPDFBitmap bitmap = RenderSavedPage(saved_page);
    CompareBitmap(bitmap.get(), 612, 446, md5);

    CloseSavedPage(saved_page);
    CloseSavedDocument();
  }
}

TEST_F(FPDFEditEmbedderTest, TestGetTextFontName) {
  EXPECT_TRUE(OpenDocument("text_font.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  ASSERT_EQ(1, FPDFPage_CountObjects(page));

  // FPDFTextObj_GetFontName() positive testing.
  FPDF_PAGEOBJECT text = FPDFPage_GetObject(page, 0);
  unsigned long size = FPDFTextObj_GetFontName(text, nullptr, 0);
  const char kExpectedFontName[] = "Liberation Serif";
  ASSERT_EQ(sizeof(kExpectedFontName), size);
  std::vector<char> font_name(size);
  ASSERT_EQ(size, FPDFTextObj_GetFontName(text, font_name.data(), size));
  ASSERT_STREQ(kExpectedFontName, font_name.data());

  // FPDFTextObj_GetFontName() negative testing.
  ASSERT_EQ(0U, FPDFTextObj_GetFontName(nullptr, nullptr, 0));

  font_name.resize(2);
  font_name[0] = 'x';
  font_name[1] = '\0';
  size = FPDFTextObj_GetFontName(text, font_name.data(), font_name.size());
  ASSERT_EQ(sizeof(kExpectedFontName), size);
  ASSERT_EQ(std::string("x"), std::string(font_name.data()));

  UnloadPage(page);
}

TEST_F(FPDFEditEmbedderTest, TestFormGetObjects) {
  EXPECT_TRUE(OpenDocument("form_object.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  ASSERT_EQ(1, FPDFPage_CountObjects(page));

  FPDF_PAGEOBJECT form = FPDFPage_GetObject(page, 0);
  EXPECT_EQ(FPDF_PAGEOBJ_FORM, FPDFPageObj_GetType(form));
  ASSERT_EQ(-1, FPDFFormObj_CountObjects(nullptr));
  ASSERT_EQ(2, FPDFFormObj_CountObjects(form));

  // FPDFFormObj_GetObject() positive testing.
  FPDF_PAGEOBJECT text1 = FPDFFormObj_GetObject(form, 0);
  ASSERT_TRUE(text1);
  float left = 0;
  float bottom = 0;
  float right = 0;
  float top = 0;
  ASSERT_TRUE(FPDFPageObj_GetBounds(text1, &left, &bottom, &right, &top));
  ASSERT_EQ(271, static_cast<int>(top));

  FPDF_PAGEOBJECT text2 = FPDFFormObj_GetObject(form, 1);
  ASSERT_TRUE(text2);
  ASSERT_TRUE(FPDFPageObj_GetBounds(text2, &left, &bottom, &right, &top));
  ASSERT_EQ(221, static_cast<int>(top));

  // FPDFFormObj_GetObject() negative testing.
  ASSERT_EQ(nullptr, FPDFFormObj_GetObject(nullptr, 0));
  ASSERT_EQ(nullptr, FPDFFormObj_GetObject(form, -1));
  ASSERT_EQ(nullptr, FPDFFormObj_GetObject(form, 2));

  // Reset the form object matrix to identity.
  auto* pPageObj = CPDFPageObjectFromFPDFPageObject(form);
  CPDF_FormObject* pFormObj = pPageObj->AsForm();
  pFormObj->Transform(pFormObj->form_matrix().GetInverse());

  // FPDFFormObj_GetMatrix() positive testing.
  static constexpr FS_MATRIX kMatrix = {1.0f, 1.5f, 2.0f, 2.5f, 100.0f, 200.0f};
  pFormObj->Transform(CFXMatrixFromFSMatrix(kMatrix));

  FS_MATRIX matrix;
  EXPECT_TRUE(FPDFFormObj_GetMatrix(form, &matrix));
  EXPECT_FLOAT_EQ(kMatrix.a, matrix.a);
  EXPECT_FLOAT_EQ(kMatrix.b, matrix.b);
  EXPECT_FLOAT_EQ(kMatrix.c, matrix.c);
  EXPECT_FLOAT_EQ(kMatrix.d, matrix.d);
  EXPECT_FLOAT_EQ(kMatrix.e, matrix.e);
  EXPECT_FLOAT_EQ(kMatrix.f, matrix.f);

  // FPDFFormObj_GetMatrix() negative testing.
  EXPECT_FALSE(FPDFFormObj_GetMatrix(nullptr, &matrix));
  EXPECT_FALSE(FPDFFormObj_GetMatrix(form, nullptr));
  EXPECT_FALSE(FPDFFormObj_GetMatrix(nullptr, nullptr));

  UnloadPage(page);
}

// Tests adding text from standard font using FPDFText_LoadStandardFont.
// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_AddStandardFontText2 DISABLED_AddStandardFontText2
#else
#define MAYBE_AddStandardFontText2 AddStandardFontText2
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_AddStandardFontText2) {
  // Start with a blank page
  ScopedFPDFPage page(FPDFPage_New(CreateNewDocument(), 0, 612, 792));

  // Load a standard font.
  ScopedFPDFFont font(FPDFText_LoadStandardFont(document(), "Helvetica"));
  ASSERT_TRUE(font);

  // Add some text to the page.
  FPDF_PAGEOBJECT text_object =
      FPDFPageObj_CreateTextObj(document(), font.get(), 12.0f);
  EXPECT_TRUE(text_object);
  ScopedFPDFWideString text =
      GetFPDFWideString(L"I'm at the bottom of the page");
  EXPECT_TRUE(FPDFText_SetText(text_object, text.get()));
  FPDFPageObj_Transform(text_object, 1, 0, 0, 1, 20, 20);
  FPDFPage_InsertObject(page.get(), text_object);
  ScopedFPDFBitmap page_bitmap = RenderPage(page.get());
#if defined(OS_MACOSX)
  const char md5[] = "324e1db8164a040cf6104538baa95ba6";
#elif defined(OS_WIN)
  const char md5[] = "08d1ff3e5a42801bee6077fd366bef00";
#else
  const char md5[] = "eacaa24573b8ce997b3882595f096f00";
#endif
  CompareBitmap(page_bitmap.get(), 612, 792, md5);
}

TEST_F(FPDFEditEmbedderTest, LoadStandardFonts) {
  CreateNewDocument();
  static constexpr const char* kStandardFontNames[] = {
      "Arial",
      "Arial-Bold",
      "Arial-BoldItalic",
      "Arial-Italic",
      "Courier",
      "Courier-BoldOblique",
      "Courier-Oblique",
      "Courier-Bold",
      "CourierNew",
      "CourierNew-Bold",
      "CourierNew-BoldItalic",
      "CourierNew-Italic",
      "Helvetica",
      "Helvetica-Bold",
      "Helvetica-BoldOblique",
      "Helvetica-Oblique",
      "Symbol",
      "TimesNewRoman",
      "TimesNewRoman-Bold",
      "TimesNewRoman-BoldItalic",
      "TimesNewRoman-Italic",
      "ZapfDingbats"};
  for (const char* font_name : kStandardFontNames) {
    ScopedFPDFFont font(FPDFText_LoadStandardFont(document(), font_name));
    EXPECT_TRUE(font) << font_name << " should be considered a standard font.";
  }
  static constexpr const char* kNotStandardFontNames[] = {
      "Abcdefg",      "ArialB",    "Arial-Style",
      "Font Name",    "FontArial", "NotAStandardFontName",
      "TestFontName", "Quack",     "Symbol-Italic",
      "Zapf"};
  for (const char* font_name : kNotStandardFontNames) {
    ScopedFPDFFont font(FPDFText_LoadStandardFont(document(), font_name));
    EXPECT_FALSE(font) << font_name
                       << " should not be considered a standard font.";
  }
}

TEST_F(FPDFEditEmbedderTest, GraphicsData) {
  // New page
  ScopedFPDFPage page(FPDFPage_New(CreateNewDocument(), 0, 612, 792));

  // Create a rect with nontrivial graphics
  FPDF_PAGEOBJECT rect1 = FPDFPageObj_CreateNewRect(10, 10, 100, 100);
  FPDFPageObj_SetBlendMode(rect1, "Color");
  FPDFPage_InsertObject(page.get(), rect1);
  EXPECT_TRUE(FPDFPage_GenerateContent(page.get()));

  // Check that the ExtGState was created
  CPDF_Page* cpage = CPDFPageFromFPDFPage(page.get());
  CPDF_Dictionary* graphics_dict = cpage->m_pResources->GetDictFor("ExtGState");
  ASSERT_TRUE(graphics_dict);
  EXPECT_EQ(2u, graphics_dict->size());

  // Add a text object causing no change to the graphics dictionary
  FPDF_PAGEOBJECT text1 = FPDFPageObj_NewTextObj(document(), "Arial", 12.0f);
  // Only alpha, the last component, matters for the graphics dictionary. And
  // the default value is 255.
  EXPECT_TRUE(FPDFPageObj_SetFillColor(text1, 100, 100, 100, 255));
  FPDFPage_InsertObject(page.get(), text1);
  EXPECT_TRUE(FPDFPage_GenerateContent(page.get()));
  EXPECT_EQ(2u, graphics_dict->size());

  // Add a text object increasing the size of the graphics dictionary
  FPDF_PAGEOBJECT text2 =
      FPDFPageObj_NewTextObj(document(), "Times-Roman", 12.0f);
  FPDFPage_InsertObject(page.get(), text2);
  FPDFPageObj_SetBlendMode(text2, "Darken");
  EXPECT_TRUE(FPDFPageObj_SetFillColor(text2, 0, 0, 255, 150));
  EXPECT_TRUE(FPDFPage_GenerateContent(page.get()));
  EXPECT_EQ(3u, graphics_dict->size());

  // Add a path that should reuse graphics
  FPDF_PAGEOBJECT path = FPDFPageObj_CreateNewPath(400, 100);
  FPDFPageObj_SetBlendMode(path, "Darken");
  EXPECT_TRUE(FPDFPageObj_SetFillColor(path, 200, 200, 100, 150));
  FPDFPage_InsertObject(page.get(), path);
  EXPECT_TRUE(FPDFPage_GenerateContent(page.get()));
  EXPECT_EQ(3u, graphics_dict->size());

  // Add a rect increasing the size of the graphics dictionary
  FPDF_PAGEOBJECT rect2 = FPDFPageObj_CreateNewRect(10, 10, 100, 100);
  FPDFPageObj_SetBlendMode(rect2, "Darken");
  EXPECT_TRUE(FPDFPageObj_SetFillColor(rect2, 0, 0, 255, 150));
  EXPECT_TRUE(FPDFPageObj_SetStrokeColor(rect2, 0, 0, 0, 200));
  FPDFPage_InsertObject(page.get(), rect2);
  EXPECT_TRUE(FPDFPage_GenerateContent(page.get()));
  EXPECT_EQ(4u, graphics_dict->size());
}

TEST_F(FPDFEditEmbedderTest, DoubleGenerating) {
  // Start with a blank page
  FPDF_PAGE page = FPDFPage_New(CreateNewDocument(), 0, 612, 792);

  // Add a red rectangle with some non-default alpha
  FPDF_PAGEOBJECT rect = FPDFPageObj_CreateNewRect(10, 10, 100, 100);
  EXPECT_TRUE(FPDFPageObj_SetFillColor(rect, 255, 0, 0, 128));
  EXPECT_TRUE(FPDFPath_SetDrawMode(rect, FPDF_FILLMODE_WINDING, 0));
  FPDFPage_InsertObject(page, rect);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  // Check the ExtGState
  CPDF_Page* cpage = CPDFPageFromFPDFPage(page);
  CPDF_Dictionary* graphics_dict = cpage->m_pResources->GetDictFor("ExtGState");
  ASSERT_TRUE(graphics_dict);
  EXPECT_EQ(2u, graphics_dict->size());

  // Check the bitmap
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792,
                  "5384da3406d62360ffb5cac4476fff1c");
  }

  // Never mind, my new favorite color is blue, increase alpha
  EXPECT_TRUE(FPDFPageObj_SetFillColor(rect, 0, 0, 255, 180));
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_EQ(3u, graphics_dict->size());

  // Check that bitmap displays changed content
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792,
                  "2e51656f5073b0bee611d9cd086aa09c");
  }

  // And now generate, without changes
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_EQ(3u, graphics_dict->size());
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792,
                  "2e51656f5073b0bee611d9cd086aa09c");
  }

  // Add some text to the page
  FPDF_PAGEOBJECT text_object =
      FPDFPageObj_NewTextObj(document(), "Arial", 12.0f);
  ScopedFPDFWideString text =
      GetFPDFWideString(L"Something something #text# something");
  EXPECT_TRUE(FPDFText_SetText(text_object, text.get()));
  FPDFPageObj_Transform(text_object, 1, 0, 0, 1, 300, 300);
  FPDFPage_InsertObject(page, text_object);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  CPDF_Dictionary* font_dict = cpage->m_pResources->GetDictFor("Font");
  ASSERT_TRUE(font_dict);
  EXPECT_EQ(1u, font_dict->size());

  // Generate yet again, check dicts are reasonably sized
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_EQ(3u, graphics_dict->size());
  EXPECT_EQ(1u, font_dict->size());
  FPDF_ClosePage(page);
}

TEST_F(FPDFEditEmbedderTest, LoadSimpleType1Font) {
  CreateNewDocument();
  // TODO(npm): use other fonts after disallowing loading any font as any type
  RetainPtr<CPDF_Font> stock_font =
      CPDF_Font::GetStockFont(cpdf_doc(), "Times-Bold");
  pdfium::span<const uint8_t> span = stock_font->GetFont()->GetFontSpan();
  ScopedFPDFFont font(FPDFText_LoadFont(document(), span.data(), span.size(),
                                        FPDF_FONT_TYPE1, false));
  ASSERT_TRUE(font.get());
  CPDF_Font* typed_font = CPDFFontFromFPDFFont(font.get());
  EXPECT_TRUE(typed_font->IsType1Font());

  const CPDF_Dictionary* font_dict = typed_font->GetFontDict();
  EXPECT_EQ("Font", font_dict->GetStringFor("Type"));
  EXPECT_EQ("Type1", font_dict->GetStringFor("Subtype"));
  EXPECT_EQ("TimesNewRomanPS-BoldMT", font_dict->GetStringFor("BaseFont"));
  ASSERT_TRUE(font_dict->KeyExist("FirstChar"));
  ASSERT_TRUE(font_dict->KeyExist("LastChar"));
  EXPECT_EQ(32, font_dict->GetIntegerFor("FirstChar"));
  EXPECT_EQ(255, font_dict->GetIntegerFor("LastChar"));

  const CPDF_Array* widths_array = font_dict->GetArrayFor("Widths");
  ASSERT_TRUE(widths_array);
  ASSERT_EQ(224u, widths_array->size());
  EXPECT_EQ(250, widths_array->GetNumberAt(0));
  EXPECT_EQ(569, widths_array->GetNumberAt(11));
  EXPECT_EQ(500, widths_array->GetNumberAt(223));
  CheckFontDescriptor(font_dict, FPDF_FONT_TYPE1, true, false, span);
}

TEST_F(FPDFEditEmbedderTest, LoadSimpleTrueTypeFont) {
  CreateNewDocument();
  RetainPtr<CPDF_Font> stock_font =
      CPDF_Font::GetStockFont(cpdf_doc(), "Courier");
  pdfium::span<const uint8_t> span = stock_font->GetFont()->GetFontSpan();
  ScopedFPDFFont font(FPDFText_LoadFont(document(), span.data(), span.size(),
                                        FPDF_FONT_TRUETYPE, false));
  ASSERT_TRUE(font.get());
  CPDF_Font* typed_font = CPDFFontFromFPDFFont(font.get());
  EXPECT_TRUE(typed_font->IsTrueTypeFont());

  const CPDF_Dictionary* font_dict = typed_font->GetFontDict();
  EXPECT_EQ("Font", font_dict->GetStringFor("Type"));
  EXPECT_EQ("TrueType", font_dict->GetStringFor("Subtype"));
  EXPECT_EQ("CourierNewPSMT", font_dict->GetStringFor("BaseFont"));
  ASSERT_TRUE(font_dict->KeyExist("FirstChar"));
  ASSERT_TRUE(font_dict->KeyExist("LastChar"));
  EXPECT_EQ(32, font_dict->GetIntegerFor("FirstChar"));
  EXPECT_EQ(255, font_dict->GetIntegerFor("LastChar"));

  const CPDF_Array* widths_array = font_dict->GetArrayFor("Widths");
  ASSERT_TRUE(widths_array);
  ASSERT_EQ(224u, widths_array->size());
  EXPECT_EQ(600, widths_array->GetNumberAt(33));
  EXPECT_EQ(600, widths_array->GetNumberAt(74));
  EXPECT_EQ(600, widths_array->GetNumberAt(223));
  CheckFontDescriptor(font_dict, FPDF_FONT_TRUETYPE, false, false, span);
}

TEST_F(FPDFEditEmbedderTest, LoadCIDType0Font) {
  CreateNewDocument();
  RetainPtr<CPDF_Font> stock_font =
      CPDF_Font::GetStockFont(cpdf_doc(), "Times-Roman");
  pdfium::span<const uint8_t> span = stock_font->GetFont()->GetFontSpan();
  ScopedFPDFFont font(FPDFText_LoadFont(document(), span.data(), span.size(),
                                        FPDF_FONT_TYPE1, 1));
  ASSERT_TRUE(font.get());
  CPDF_Font* typed_font = CPDFFontFromFPDFFont(font.get());
  EXPECT_TRUE(typed_font->IsCIDFont());

  // Check font dictionary entries
  const CPDF_Dictionary* font_dict = typed_font->GetFontDict();
  EXPECT_EQ("Font", font_dict->GetStringFor("Type"));
  EXPECT_EQ("Type0", font_dict->GetStringFor("Subtype"));
  EXPECT_EQ("TimesNewRomanPSMT-Identity-H",
            font_dict->GetStringFor("BaseFont"));
  EXPECT_EQ("Identity-H", font_dict->GetStringFor("Encoding"));
  const CPDF_Array* descendant_array =
      font_dict->GetArrayFor("DescendantFonts");
  ASSERT_TRUE(descendant_array);
  EXPECT_EQ(1u, descendant_array->size());

  // Check the CIDFontDict
  const CPDF_Dictionary* cidfont_dict = descendant_array->GetDictAt(0);
  EXPECT_EQ("Font", cidfont_dict->GetStringFor("Type"));
  EXPECT_EQ("CIDFontType0", cidfont_dict->GetStringFor("Subtype"));
  EXPECT_EQ("TimesNewRomanPSMT", cidfont_dict->GetStringFor("BaseFont"));
  const CPDF_Dictionary* cidinfo_dict =
      cidfont_dict->GetDictFor("CIDSystemInfo");
  ASSERT_TRUE(cidinfo_dict);
  const CPDF_Object* registry = cidinfo_dict->GetObjectFor("Registry");
  ASSERT_TRUE(registry);
  EXPECT_EQ(CPDF_Object::kString, registry->GetType());
  EXPECT_EQ("Adobe", registry->GetString());
  const CPDF_Object* ordering = cidinfo_dict->GetObjectFor("Ordering");
  ASSERT_TRUE(ordering);
  EXPECT_EQ(CPDF_Object::kString, ordering->GetType());
  EXPECT_EQ("Identity", ordering->GetString());
  EXPECT_EQ(0, cidinfo_dict->GetNumberFor("Supplement"));
  CheckFontDescriptor(cidfont_dict, FPDF_FONT_TYPE1, false, false, span);

  // Check widths
  const CPDF_Array* widths_array = cidfont_dict->GetArrayFor("W");
  ASSERT_TRUE(widths_array);
  EXPECT_GT(widths_array->size(), 1u);
  CheckCompositeFontWidths(widths_array, typed_font);
}

TEST_F(FPDFEditEmbedderTest, LoadCIDType2Font) {
  CreateNewDocument();
  RetainPtr<CPDF_Font> stock_font =
      CPDF_Font::GetStockFont(cpdf_doc(), "Helvetica-Oblique");
  pdfium::span<const uint8_t> span = stock_font->GetFont()->GetFontSpan();
  ScopedFPDFFont font(FPDFText_LoadFont(document(), span.data(), span.size(),
                                        FPDF_FONT_TRUETYPE, 1));
  ASSERT_TRUE(font.get());
  CPDF_Font* typed_font = CPDFFontFromFPDFFont(font.get());
  EXPECT_TRUE(typed_font->IsCIDFont());

  // Check font dictionary entries
  const CPDF_Dictionary* font_dict = typed_font->GetFontDict();
  EXPECT_EQ("Font", font_dict->GetStringFor("Type"));
  EXPECT_EQ("Type0", font_dict->GetStringFor("Subtype"));
  EXPECT_EQ("Arial-ItalicMT", font_dict->GetStringFor("BaseFont"));
  EXPECT_EQ("Identity-H", font_dict->GetStringFor("Encoding"));
  const CPDF_Array* descendant_array =
      font_dict->GetArrayFor("DescendantFonts");
  ASSERT_TRUE(descendant_array);
  EXPECT_EQ(1u, descendant_array->size());

  // Check the CIDFontDict
  const CPDF_Dictionary* cidfont_dict = descendant_array->GetDictAt(0);
  EXPECT_EQ("Font", cidfont_dict->GetStringFor("Type"));
  EXPECT_EQ("CIDFontType2", cidfont_dict->GetStringFor("Subtype"));
  EXPECT_EQ("Arial-ItalicMT", cidfont_dict->GetStringFor("BaseFont"));
  const CPDF_Dictionary* cidinfo_dict =
      cidfont_dict->GetDictFor("CIDSystemInfo");
  ASSERT_TRUE(cidinfo_dict);
  EXPECT_EQ("Adobe", cidinfo_dict->GetStringFor("Registry"));
  EXPECT_EQ("Identity", cidinfo_dict->GetStringFor("Ordering"));
  EXPECT_EQ(0, cidinfo_dict->GetNumberFor("Supplement"));
  CheckFontDescriptor(cidfont_dict, FPDF_FONT_TRUETYPE, false, true, span);

  // Check widths
  const CPDF_Array* widths_array = cidfont_dict->GetArrayFor("W");
  ASSERT_TRUE(widths_array);
  CheckCompositeFontWidths(widths_array, typed_font);
}

TEST_F(FPDFEditEmbedderTest, NormalizeNegativeRotation) {
  // Load document with a -90 degree rotation
  EXPECT_TRUE(OpenDocument("bug_713197.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_NE(nullptr, page);

  EXPECT_EQ(3, FPDFPage_GetRotation(page));
  UnloadPage(page);
}

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_AddTrueTypeFontText DISABLED_AddTrueTypeFontText
#else
#define MAYBE_AddTrueTypeFontText AddTrueTypeFontText
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_AddTrueTypeFontText) {
  // Start with a blank page
  FPDF_PAGE page = FPDFPage_New(CreateNewDocument(), 0, 612, 792);
  {
    RetainPtr<CPDF_Font> stock_font =
        CPDF_Font::GetStockFont(cpdf_doc(), "Arial");
    pdfium::span<const uint8_t> span = stock_font->GetFont()->GetFontSpan();
    ScopedFPDFFont font(FPDFText_LoadFont(document(), span.data(), span.size(),
                                          FPDF_FONT_TRUETYPE, 0));
    ASSERT_TRUE(font.get());

    // Add some text to the page
    FPDF_PAGEOBJECT text_object =
        FPDFPageObj_CreateTextObj(document(), font.get(), 12.0f);
    EXPECT_TRUE(text_object);
    ScopedFPDFWideString text =
        GetFPDFWideString(L"I am testing my loaded font, WEE.");
    EXPECT_TRUE(FPDFText_SetText(text_object, text.get()));
    FPDFPageObj_Transform(text_object, 1, 0, 0, 1, 400, 400);
    FPDFPage_InsertObject(page, text_object);
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
#if defined(OS_MACOSX)
    const char md5[] = "fc921c0bbdde73986ac13c15a85db4c3";
#elif defined(OS_WIN)
    const char md5[] = "d60ba39f9698e32360d99e727dd93165";
#else
    const char md5[] = "70592859010ffbf532a2237b8118bcc4";
#endif
    CompareBitmap(page_bitmap.get(), 612, 792, md5);

    // Add some more text, same font
    FPDF_PAGEOBJECT text_object2 =
        FPDFPageObj_CreateTextObj(document(), font.get(), 15.0f);
    ScopedFPDFWideString text2 = GetFPDFWideString(L"Bigger font size");
    EXPECT_TRUE(FPDFText_SetText(text_object2, text2.get()));
    FPDFPageObj_Transform(text_object2, 1, 0, 0, 1, 200, 200);
    FPDFPage_InsertObject(page, text_object2);
  }
  ScopedFPDFBitmap page_bitmap2 = RenderPage(page);
#if defined(OS_MACOSX)
  const char md5_2[] = "9a1a7dfebe659513691aadd0d95b8d50";
#elif defined(OS_WIN)
  const char md5_2[] = "2199b579c49ab5f80c246a586a80ee90";
#else
  const char md5_2[] = "c1d10cce1761c4a998a16b2562030568";
#endif
  CompareBitmap(page_bitmap2.get(), 612, 792, md5_2);

  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  FPDF_ClosePage(page);

  VerifySavedDocument(612, 792, md5_2);
}

TEST_F(FPDFEditEmbedderTest, TransformAnnot) {
  // Open a file with one annotation and load its first page.
  ASSERT_TRUE(OpenDocument("annotation_highlight_long_content.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  {
    // Add an underline annotation to the page without specifying its rectangle.
    ScopedFPDFAnnotation annot(
        FPDFPage_CreateAnnot(page, FPDF_ANNOT_UNDERLINE));
    ASSERT_TRUE(annot);

    // FPDFPage_TransformAnnots() should run without errors when modifying
    // annotation rectangles.
    FPDFPage_TransformAnnots(page, 1, 2, 3, 4, 5, 6);
  }
  UnloadPage(page);
}

// TODO(npm): Add tests using Japanese fonts in other OS.
#if _FX_PLATFORM_ == _FX_PLATFORM_LINUX_
// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_AddCIDFontText DISABLED_AddCIDFontText
#else
#define MAYBE_AddCIDFontText AddCIDFontText
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_AddCIDFontText) {
  // Start with a blank page
  FPDF_PAGE page = FPDFPage_New(CreateNewDocument(), 0, 612, 792);
  CFX_Font CIDfont;
  {
    // First, get the data from the font
    CIDfont.LoadSubst("IPAGothic", 1, 0, 400, 0, 932, 0);
    EXPECT_EQ("IPAGothic", CIDfont.GetFaceName());
    pdfium::span<const uint8_t> span = CIDfont.GetFontSpan();

    // Load the data into a FPDF_Font.
    ScopedFPDFFont font(FPDFText_LoadFont(document(), span.data(), span.size(),
                                          FPDF_FONT_TRUETYPE, 1));
    ASSERT_TRUE(font.get());

    // Add some text to the page
    FPDF_PAGEOBJECT text_object =
        FPDFPageObj_CreateTextObj(document(), font.get(), 12.0f);
    ASSERT_TRUE(text_object);
    std::wstring wstr = L"ABCDEFGhijklmnop.";
    ScopedFPDFWideString text = GetFPDFWideString(wstr);
    EXPECT_TRUE(FPDFText_SetText(text_object, text.get()));
    FPDFPageObj_Transform(text_object, 1, 0, 0, 1, 200, 200);
    FPDFPage_InsertObject(page, text_object);

    // And add some Japanese characters
    FPDF_PAGEOBJECT text_object2 =
        FPDFPageObj_CreateTextObj(document(), font.get(), 18.0f);
    ASSERT_TRUE(text_object2);
    std::wstring wstr2 =
        L"\u3053\u3093\u306B\u3061\u306f\u4e16\u754C\u3002\u3053\u3053\u306B1"
        L"\u756A";
    ScopedFPDFWideString text2 = GetFPDFWideString(wstr2);
    EXPECT_TRUE(FPDFText_SetText(text_object2, text2.get()));
    FPDFPageObj_Transform(text_object2, 1, 0, 0, 1, 100, 500);
    FPDFPage_InsertObject(page, text_object2);
  }

  // Check that the text renders properly.
  const char md5[] = "5159a72903fe57bf0cf645c894de8a74";
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792, md5);
  }

  // Save the document, close the page.
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  FPDF_ClosePage(page);

  VerifySavedDocument(612, 792, md5);
}
#endif  // _FX_PLATFORM_ == _FX_PLATFORM_LINUX_

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_SaveAndRender DISABLED_SaveAndRender
#else
#define MAYBE_SaveAndRender SaveAndRender
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_SaveAndRender) {
  const char md5[] = "3c20472b0552c0c22b88ab1ed8c6202b";
  {
    EXPECT_TRUE(OpenDocument("bug_779.pdf"));
    FPDF_PAGE page = LoadPage(0);
    ASSERT_NE(nullptr, page);

    // Now add a more complex blue path.
    FPDF_PAGEOBJECT green_path = FPDFPageObj_CreateNewPath(20, 20);
    EXPECT_TRUE(FPDFPageObj_SetFillColor(green_path, 0, 255, 0, 200));
    // TODO(npm): stroking will cause the MD5s to differ.
    EXPECT_TRUE(FPDFPath_SetDrawMode(green_path, FPDF_FILLMODE_WINDING, 0));
    EXPECT_TRUE(FPDFPath_LineTo(green_path, 20, 63));
    EXPECT_TRUE(FPDFPath_BezierTo(green_path, 55, 55, 78, 78, 90, 90));
    EXPECT_TRUE(FPDFPath_LineTo(green_path, 133, 133));
    EXPECT_TRUE(FPDFPath_LineTo(green_path, 133, 33));
    EXPECT_TRUE(FPDFPath_BezierTo(green_path, 38, 33, 39, 36, 40, 40));
    EXPECT_TRUE(FPDFPath_Close(green_path));
    FPDFPage_InsertObject(page, green_path);
    ScopedFPDFBitmap page_bitmap = RenderLoadedPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792, md5);

    // Now save the result, closing the page and document
    EXPECT_TRUE(FPDFPage_GenerateContent(page));
    EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
    UnloadPage(page);
  }

  VerifySavedDocument(612, 792, md5);
}

TEST_F(FPDFEditEmbedderTest, AddMark) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("text_in_page_marked.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  CheckMarkCounts(page, 1, 19, 8, 4, 9, 1);

  // Add to the first page object a "Bounds" mark with "Position": "First".
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 0);
  FPDF_PAGEOBJECTMARK mark = FPDFPageObj_AddMark(page_object, "Bounds");
  EXPECT_TRUE(mark);
  EXPECT_TRUE(FPDFPageObjMark_SetStringParam(document(), page_object, mark,
                                             "Position", "First"));

  CheckMarkCounts(page, 1, 19, 8, 4, 9, 2);

  // Save the file
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  // Re-open the file and check the new mark is present.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);

  CheckMarkCounts(saved_page, 1, 19, 8, 4, 9, 2);

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_AddMarkCompressedStream DISABLED_AddMarkCompressedStream
#else
#define MAYBE_AddMarkCompressedStream AddMarkCompressedStream
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_AddMarkCompressedStream) {
  // Load document with some text in a compressed stream.
  EXPECT_TRUE(OpenDocument("hello_world_compressed_stream.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Render and check there are no marks.
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 200, kHelloWorldChecksum);
  }
  CheckMarkCounts(page, 0, 2, 0, 0, 0, 0);

  // Add to the first page object a "Bounds" mark with "Position": "First".
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 0);
  FPDF_PAGEOBJECTMARK mark = FPDFPageObj_AddMark(page_object, "Bounds");
  EXPECT_TRUE(mark);
  EXPECT_TRUE(FPDFPageObjMark_SetStringParam(document(), page_object, mark,
                                             "Position", "First"));

  // Render and check there is 1 mark.
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 200, 200, kHelloWorldChecksum);
  }
  CheckMarkCounts(page, 0, 2, 0, 0, 0, 1);

  // Save the file.
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  // Re-open the file and check the new mark is present.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);

  {
    ScopedFPDFBitmap page_bitmap = RenderPage(saved_page);
    CompareBitmap(page_bitmap.get(), 200, 200, kHelloWorldChecksum);
  }
  CheckMarkCounts(saved_page, 0, 2, 0, 0, 0, 1);

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, SetMarkParam) {
  // Load document with some text.
  EXPECT_TRUE(OpenDocument("text_in_page_marked.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  constexpr int kExpectedObjectCount = 19;
  CheckMarkCounts(page, 1, kExpectedObjectCount, 8, 4, 9, 1);

  // Check the "Bounds" mark's "Position" param is "Last".
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 18);
  FPDF_PAGEOBJECTMARK mark = FPDFPageObj_GetMark(page_object, 1);
  ASSERT_TRUE(mark);
  char buffer[256];
  unsigned long name_len = 999u;
  ASSERT_TRUE(FPDFPageObjMark_GetName(mark, buffer, sizeof(buffer), &name_len));
  EXPECT_EQ((6u + 1u) * 2u, name_len);
  ASSERT_EQ(L"Bounds",
            GetPlatformWString(reinterpret_cast<unsigned short*>(buffer)));
  unsigned long out_buffer_len;
  ASSERT_TRUE(FPDFPageObjMark_GetParamStringValue(
      mark, "Position", buffer, sizeof(buffer), &out_buffer_len));
  ASSERT_EQ(L"Last",
            GetPlatformWString(reinterpret_cast<unsigned short*>(buffer)));

  // Set is to "End".
  EXPECT_TRUE(FPDFPageObjMark_SetStringParam(document(), page_object, mark,
                                             "Position", "End"));

  // Verify the object passed must correspond to the mark passed.
  FPDF_PAGEOBJECT another_page_object = FPDFPage_GetObject(page, 17);
  EXPECT_FALSE(FPDFPageObjMark_SetStringParam(document(), another_page_object,
                                              mark, "Position", "End"));

  // Verify nothing else changed.
  CheckMarkCounts(page, 1, kExpectedObjectCount, 8, 4, 9, 1);

  // Verify "Position" now maps to "End".
  EXPECT_TRUE(FPDFPageObjMark_GetParamStringValue(
      mark, "Position", buffer, sizeof(buffer), &out_buffer_len));
  EXPECT_EQ(L"End",
            GetPlatformWString(reinterpret_cast<unsigned short*>(buffer)));

  // Save the file
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));
  UnloadPage(page);

  // Re-open the file and cerify "Position" still maps to "End".
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);

  CheckMarkCounts(saved_page, 1, kExpectedObjectCount, 8, 4, 9, 1);
  page_object = FPDFPage_GetObject(saved_page, 18);
  mark = FPDFPageObj_GetMark(page_object, 1);
  EXPECT_TRUE(FPDFPageObjMark_GetParamStringValue(
      mark, "Position", buffer, sizeof(buffer), &out_buffer_len));
  EXPECT_EQ(L"End",
            GetPlatformWString(reinterpret_cast<unsigned short*>(buffer)));

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

// TODO(crbug.com/pdfium/11): Fix this test and enable.
#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#define MAYBE_AddMarkedText DISABLED_AddMarkedText
#else
#define MAYBE_AddMarkedText AddMarkedText
#endif
TEST_F(FPDFEditEmbedderTest, MAYBE_AddMarkedText) {
  // Start with a blank page.
  FPDF_PAGE page = FPDFPage_New(CreateNewDocument(), 0, 612, 792);

  RetainPtr<CPDF_Font> stock_font =
      CPDF_Font::GetStockFont(cpdf_doc(), "Arial");
  pdfium::span<const uint8_t> span = stock_font->GetFont()->GetFontSpan();
  ScopedFPDFFont font(FPDFText_LoadFont(document(), span.data(), span.size(),
                                        FPDF_FONT_TRUETYPE, 0));
  ASSERT_TRUE(font.get());

  // Add some text to the page.
  FPDF_PAGEOBJECT text_object =
      FPDFPageObj_CreateTextObj(document(), font.get(), 12.0f);

  EXPECT_TRUE(text_object);
  ScopedFPDFWideString text1 =
      GetFPDFWideString(L"I am testing my loaded font, WEE.");
  EXPECT_TRUE(FPDFText_SetText(text_object, text1.get()));
  FPDFPageObj_Transform(text_object, 1, 0, 0, 1, 400, 400);
  FPDFPage_InsertObject(page, text_object);

  // Add a mark with the tag "TestMarkName" to that text.
  EXPECT_EQ(0, FPDFPageObj_CountMarks(text_object));
  FPDF_PAGEOBJECTMARK mark = FPDFPageObj_AddMark(text_object, "Test Mark Name");
  EXPECT_TRUE(mark);
  EXPECT_EQ(1, FPDFPageObj_CountMarks(text_object));
  EXPECT_EQ(mark, FPDFPageObj_GetMark(text_object, 0));
  char buffer[256];
  unsigned long name_len = 999u;
  ASSERT_TRUE(FPDFPageObjMark_GetName(mark, buffer, sizeof(buffer), &name_len));
  EXPECT_EQ((14u + 1u) * 2, name_len);
  std::wstring name =
      GetPlatformWString(reinterpret_cast<unsigned short*>(buffer));
  EXPECT_EQ(L"Test Mark Name", name);

  // Add parameters:
  // - int "IntKey" : 42
  // - string "StringKey": "StringValue"
  // - blob "BlobKey": "\x01\x02\x03\0BlobValue1\0\0\0BlobValue2\0"
  constexpr size_t kBlobLen = 28;
  char block_value[kBlobLen];
  memcpy(block_value, "\x01\x02\x03\0BlobValue1\0\0\0BlobValue2\0", kBlobLen);
  EXPECT_EQ(0, FPDFPageObjMark_CountParams(mark));
  EXPECT_TRUE(
      FPDFPageObjMark_SetIntParam(document(), text_object, mark, "IntKey", 42));
  EXPECT_TRUE(FPDFPageObjMark_SetStringParam(document(), text_object, mark,
                                             "StringKey", "StringValue"));
  EXPECT_TRUE(FPDFPageObjMark_SetBlobParam(document(), text_object, mark,
                                           "BlobKey", block_value, kBlobLen));
  EXPECT_EQ(3, FPDFPageObjMark_CountParams(mark));

  // Check the two parameters can be retrieved.
  EXPECT_EQ(FPDF_OBJECT_NUMBER,
            FPDFPageObjMark_GetParamValueType(mark, "IntKey"));
  int int_value;
  EXPECT_TRUE(FPDFPageObjMark_GetParamIntValue(mark, "IntKey", &int_value));
  EXPECT_EQ(42, int_value);

  EXPECT_EQ(FPDF_OBJECT_STRING,
            FPDFPageObjMark_GetParamValueType(mark, "StringKey"));
  unsigned long out_buffer_len = 999u;
  EXPECT_TRUE(FPDFPageObjMark_GetParamStringValue(
      mark, "StringKey", buffer, sizeof(buffer), &out_buffer_len));
  EXPECT_GT(out_buffer_len, 0u);
  EXPECT_NE(999u, out_buffer_len);
  name = GetPlatformWString(reinterpret_cast<unsigned short*>(buffer));
  EXPECT_EQ(L"StringValue", name);

  EXPECT_EQ(FPDF_OBJECT_STRING,
            FPDFPageObjMark_GetParamValueType(mark, "BlobKey"));
  out_buffer_len = 0;
  EXPECT_TRUE(FPDFPageObjMark_GetParamBlobValue(
      mark, "BlobKey", buffer, sizeof(buffer), &out_buffer_len));
  EXPECT_EQ(kBlobLen, out_buffer_len);
  EXPECT_EQ(0, memcmp(block_value, buffer, kBlobLen));

// Render and check the bitmap is the expected one.
#if defined(OS_MACOSX)
  const char md5[] = "fc921c0bbdde73986ac13c15a85db4c3";
#elif defined(OS_WIN)
  const char md5[] = "d60ba39f9698e32360d99e727dd93165";
#else
  const char md5[] = "70592859010ffbf532a2237b8118bcc4";
#endif
  {
    ScopedFPDFBitmap page_bitmap = RenderPage(page);
    CompareBitmap(page_bitmap.get(), 612, 792, md5);
  }

  // Now save the result.
  EXPECT_EQ(1, FPDFPage_CountObjects(page));
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  FPDF_ClosePage(page);

  // Re-open the file and check the changes were kept in the saved .pdf.
  ASSERT_TRUE(OpenSavedDocument());
  FPDF_PAGE saved_page = LoadSavedPage(0);
  EXPECT_EQ(1, FPDFPage_CountObjects(saved_page));

  text_object = FPDFPage_GetObject(saved_page, 0);
  EXPECT_TRUE(text_object);
  EXPECT_EQ(1, FPDFPageObj_CountMarks(text_object));
  mark = FPDFPageObj_GetMark(text_object, 0);
  EXPECT_TRUE(mark);

  name_len = 999u;
  ASSERT_TRUE(FPDFPageObjMark_GetName(mark, buffer, sizeof(buffer), &name_len));
  EXPECT_EQ((14u + 1u) * 2, name_len);
  name = GetPlatformWString(reinterpret_cast<unsigned short*>(buffer));
  EXPECT_EQ(L"Test Mark Name", name);

  CloseSavedPage(saved_page);
  CloseSavedDocument();
}

TEST_F(FPDFEditEmbedderTest, MarkGetName) {
  EXPECT_TRUE(OpenDocument("text_in_page_marked.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 18);
  FPDF_PAGEOBJECTMARK mark = FPDFPageObj_GetMark(page_object, 1);
  ASSERT_TRUE(mark);

  char buffer[256];
  unsigned long out_len;

  // Show the positive cases of FPDFPageObjMark_GetName.
  out_len = 999u;
  EXPECT_TRUE(FPDFPageObjMark_GetName(mark, nullptr, 0, &out_len));
  EXPECT_EQ((6u + 1u) * 2u, out_len);

  out_len = 999u;
  EXPECT_TRUE(FPDFPageObjMark_GetName(mark, buffer, sizeof(buffer), &out_len));
  EXPECT_EQ(L"Bounds",
            GetPlatformWString(reinterpret_cast<unsigned short*>(buffer)));
  EXPECT_EQ((6u + 1u) * 2u, out_len);

  // Show the negative cases of FPDFPageObjMark_GetName.
  out_len = 999u;
  EXPECT_FALSE(
      FPDFPageObjMark_GetName(nullptr, buffer, sizeof(buffer), &out_len));
  EXPECT_EQ(999u, out_len);

  EXPECT_FALSE(FPDFPageObjMark_GetName(mark, buffer, sizeof(buffer), nullptr));

  UnloadPage(page);
}

TEST_F(FPDFEditEmbedderTest, MarkGetParamKey) {
  EXPECT_TRUE(OpenDocument("text_in_page_marked.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 18);
  FPDF_PAGEOBJECTMARK mark = FPDFPageObj_GetMark(page_object, 1);
  ASSERT_TRUE(mark);

  char buffer[256];
  unsigned long out_len;

  // Show the positive cases of FPDFPageObjMark_GetParamKey.
  out_len = 999u;
  EXPECT_TRUE(FPDFPageObjMark_GetParamKey(mark, 0, nullptr, 0, &out_len));
  EXPECT_EQ((8u + 1u) * 2u, out_len);

  out_len = 999u;
  EXPECT_TRUE(
      FPDFPageObjMark_GetParamKey(mark, 0, buffer, sizeof(buffer), &out_len));
  EXPECT_EQ(L"Position",
            GetPlatformWString(reinterpret_cast<unsigned short*>(buffer)));
  EXPECT_EQ((8u + 1u) * 2u, out_len);

  // Show the negative cases of FPDFPageObjMark_GetParamKey.
  out_len = 999u;
  EXPECT_FALSE(FPDFPageObjMark_GetParamKey(nullptr, 0, buffer, sizeof(buffer),
                                           &out_len));
  EXPECT_EQ(999u, out_len);

  out_len = 999u;
  EXPECT_FALSE(
      FPDFPageObjMark_GetParamKey(mark, 1, buffer, sizeof(buffer), &out_len));
  EXPECT_EQ(999u, out_len);

  EXPECT_FALSE(
      FPDFPageObjMark_GetParamKey(mark, 0, buffer, sizeof(buffer), nullptr));

  UnloadPage(page);
}

TEST_F(FPDFEditEmbedderTest, MarkGetIntParam) {
  EXPECT_TRUE(OpenDocument("text_in_page_marked.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 8);
  FPDF_PAGEOBJECTMARK mark = FPDFPageObj_GetMark(page_object, 0);
  ASSERT_TRUE(mark);

  int out_value;

  // Show the positive cases of FPDFPageObjMark_GetParamIntValue.
  out_value = 999;
  EXPECT_TRUE(FPDFPageObjMark_GetParamIntValue(mark, "Factor", &out_value));
  EXPECT_EQ(3, out_value);

  // Show the negative cases of FPDFPageObjMark_GetParamIntValue.
  out_value = 999;
  EXPECT_FALSE(FPDFPageObjMark_GetParamIntValue(nullptr, "Factor", &out_value));
  EXPECT_EQ(999, out_value);

  out_value = 999;
  EXPECT_FALSE(FPDFPageObjMark_GetParamIntValue(mark, "ParamThatDoesNotExist",
                                                &out_value));
  EXPECT_EQ(999, out_value);

  EXPECT_FALSE(FPDFPageObjMark_GetParamIntValue(mark, "Factor", nullptr));

  page_object = FPDFPage_GetObject(page, 18);
  mark = FPDFPageObj_GetMark(page_object, 1);
  out_value = 999;
  EXPECT_FALSE(FPDFPageObjMark_GetParamIntValue(mark, "Position", &out_value));
  EXPECT_EQ(999, out_value);

  UnloadPage(page);
}

TEST_F(FPDFEditEmbedderTest, MarkGetStringParam) {
  EXPECT_TRUE(OpenDocument("text_in_page_marked.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, 18);
  FPDF_PAGEOBJECTMARK mark = FPDFPageObj_GetMark(page_object, 1);
  ASSERT_TRUE(mark);

  char buffer[256];
  unsigned long out_len;

  // Show the positive cases of FPDFPageObjMark_GetParamStringValue.
  out_len = 999u;
  EXPECT_TRUE(FPDFPageObjMark_GetParamStringValue(mark, "Position", nullptr, 0,
                                                  &out_len));
  EXPECT_EQ((4u + 1u) * 2u, out_len);

  out_len = 999u;
  EXPECT_TRUE(FPDFPageObjMark_GetParamStringValue(mark, "Position", buffer,
                                                  sizeof(buffer), &out_len));
  EXPECT_EQ(L"Last",
            GetPlatformWString(reinterpret_cast<unsigned short*>(buffer)));
  EXPECT_EQ((4u + 1u) * 2u, out_len);

  // Show the negative cases of FPDFPageObjMark_GetParamStringValue.
  out_len = 999u;
  EXPECT_FALSE(FPDFPageObjMark_GetParamStringValue(nullptr, "Position", buffer,
                                                   sizeof(buffer), &out_len));
  EXPECT_EQ(999u, out_len);

  out_len = 999u;
  EXPECT_FALSE(FPDFPageObjMark_GetParamStringValue(
      mark, "ParamThatDoesNotExist", buffer, sizeof(buffer), &out_len));
  EXPECT_EQ(999u, out_len);

  EXPECT_FALSE(FPDFPageObjMark_GetParamStringValue(mark, "Position", buffer,
                                                   sizeof(buffer), nullptr));

  page_object = FPDFPage_GetObject(page, 8);
  mark = FPDFPageObj_GetMark(page_object, 0);
  out_len = 999u;
  EXPECT_FALSE(FPDFPageObjMark_GetParamStringValue(mark, "Factor", buffer,
                                                   sizeof(buffer), &out_len));
  EXPECT_EQ(999u, out_len);

  UnloadPage(page);
}

TEST_F(FPDFEditEmbedderTest, ExtractImageBitmap) {
  ASSERT_TRUE(OpenDocument("embedded_images.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  ASSERT_EQ(39, FPDFPage_CountObjects(page));

  FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, 32);
  EXPECT_NE(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  EXPECT_FALSE(FPDFImageObj_GetBitmap(obj));

  obj = FPDFPage_GetObject(page, 33);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  FPDF_BITMAP bitmap = FPDFImageObj_GetBitmap(obj);
  EXPECT_EQ(FPDFBitmap_BGR, FPDFBitmap_GetFormat(bitmap));
  CompareBitmap(bitmap, 109, 88, "cb3637934bb3b95a6e4ae1ea9eb9e56e");
  FPDFBitmap_Destroy(bitmap);

  obj = FPDFPage_GetObject(page, 34);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  bitmap = FPDFImageObj_GetBitmap(obj);
  EXPECT_EQ(FPDFBitmap_BGR, FPDFBitmap_GetFormat(bitmap));
  CompareBitmap(bitmap, 103, 75, "c8d51fa6821ceb2a67f08446ff236c40");
  FPDFBitmap_Destroy(bitmap);

  obj = FPDFPage_GetObject(page, 35);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  bitmap = FPDFImageObj_GetBitmap(obj);
  EXPECT_EQ(FPDFBitmap_Gray, FPDFBitmap_GetFormat(bitmap));
  CompareBitmap(bitmap, 92, 68, "9c6d76cb1e37ef8514f9455d759391f3");
  FPDFBitmap_Destroy(bitmap);

  obj = FPDFPage_GetObject(page, 36);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  bitmap = FPDFImageObj_GetBitmap(obj);
  EXPECT_EQ(FPDFBitmap_BGR, FPDFBitmap_GetFormat(bitmap));
  CompareBitmap(bitmap, 79, 60, "f4e72fb783a01c7b4614cdc25eaa98ac");
  FPDFBitmap_Destroy(bitmap);

  obj = FPDFPage_GetObject(page, 37);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  bitmap = FPDFImageObj_GetBitmap(obj);
  EXPECT_EQ(FPDFBitmap_BGR, FPDFBitmap_GetFormat(bitmap));
  CompareBitmap(bitmap, 126, 106, "2cf9e66414c72461f4ccbf9cdebdfa1b");
  FPDFBitmap_Destroy(bitmap);

  obj = FPDFPage_GetObject(page, 38);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  bitmap = FPDFImageObj_GetBitmap(obj);
  EXPECT_EQ(FPDFBitmap_BGR, FPDFBitmap_GetFormat(bitmap));
  CompareBitmap(bitmap, 194, 119, "a8f3a126cec274dab8242fd2ccdc1b8b");
  FPDFBitmap_Destroy(bitmap);
  UnloadPage(page);
}

TEST_F(FPDFEditEmbedderTest, ExtractJBigImageBitmap) {
  ASSERT_TRUE(OpenDocument("bug_631912.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  ASSERT_EQ(1, FPDFPage_CountObjects(page));

  FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, 0);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  {
    ScopedFPDFBitmap bitmap(FPDFImageObj_GetBitmap(obj));
    ASSERT_TRUE(bitmap);
    EXPECT_EQ(FPDFBitmap_Gray, FPDFBitmap_GetFormat(bitmap.get()));
    CompareBitmap(bitmap.get(), 1152, 720, "3f6a48e2b3e91b799bf34567f55cb4de");
  }

  UnloadPage(page);
}

TEST_F(FPDFEditEmbedderTest, GetImageData) {
  EXPECT_TRUE(OpenDocument("embedded_images.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  ASSERT_EQ(39, FPDFPage_CountObjects(page));

  // Retrieve an image object with flate-encoded data stream.
  FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, 33);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));

  // Check that the raw image data has the correct length and hash value.
  unsigned long len = FPDFImageObj_GetImageDataRaw(obj, nullptr, 0);
  std::vector<char> buf(len);
  EXPECT_EQ(4091u, FPDFImageObj_GetImageDataRaw(obj, buf.data(), len));
  EXPECT_EQ("f73802327d2e88e890f653961bcda81a",
            GenerateMD5Base16(reinterpret_cast<uint8_t*>(buf.data()), len));

  // Check that the decoded image data has the correct length and hash value.
  len = FPDFImageObj_GetImageDataDecoded(obj, nullptr, 0);
  buf.clear();
  buf.resize(len);
  EXPECT_EQ(28776u, FPDFImageObj_GetImageDataDecoded(obj, buf.data(), len));
  EXPECT_EQ("cb3637934bb3b95a6e4ae1ea9eb9e56e",
            GenerateMD5Base16(reinterpret_cast<uint8_t*>(buf.data()), len));

  // Retrieve an image object with DCTDecode-encoded data stream.
  obj = FPDFPage_GetObject(page, 37);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));

  // Check that the raw image data has the correct length and hash value.
  len = FPDFImageObj_GetImageDataRaw(obj, nullptr, 0);
  buf.clear();
  buf.resize(len);
  EXPECT_EQ(4370u, FPDFImageObj_GetImageDataRaw(obj, buf.data(), len));
  EXPECT_EQ("6aae1f3710335023a9e12191be66b64b",
            GenerateMD5Base16(reinterpret_cast<uint8_t*>(buf.data()), len));

  // Check that the decoded image data has the correct length and hash value,
  // which should be the same as those of the raw data, since this image is
  // encoded by a single DCTDecode filter and decoding is a noop.
  len = FPDFImageObj_GetImageDataDecoded(obj, nullptr, 0);
  buf.clear();
  buf.resize(len);
  EXPECT_EQ(4370u, FPDFImageObj_GetImageDataDecoded(obj, buf.data(), len));
  EXPECT_EQ("6aae1f3710335023a9e12191be66b64b",
            GenerateMD5Base16(reinterpret_cast<uint8_t*>(buf.data()), len));

  UnloadPage(page);
}

TEST_F(FPDFEditEmbedderTest, GetImageMatrix) {
  ASSERT_TRUE(OpenDocument("embedded_images.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  ASSERT_EQ(39, FPDFPage_CountObjects(page));

  FPDF_PAGEOBJECT obj;
  double a;
  double b;
  double c;
  double d;
  double e;
  double f;

  obj = FPDFPage_GetObject(page, 33);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  EXPECT_TRUE(FPDFImageObj_GetMatrix(obj, &a, &b, &c, &d, &e, &f));
  EXPECT_DOUBLE_EQ(53.0, a);
  EXPECT_DOUBLE_EQ(0.0, b);
  EXPECT_DOUBLE_EQ(0.0, c);
  EXPECT_DOUBLE_EQ(43.0, d);
  EXPECT_DOUBLE_EQ(72.0, e);
  EXPECT_DOUBLE_EQ(646.510009765625, f);

  obj = FPDFPage_GetObject(page, 34);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  EXPECT_TRUE(FPDFImageObj_GetMatrix(obj, &a, &b, &c, &d, &e, &f));
  EXPECT_DOUBLE_EQ(70.0, a);
  EXPECT_DOUBLE_EQ(0.0, b);
  EXPECT_DOUBLE_EQ(0.0, c);
  EXPECT_DOUBLE_EQ(51.0, d);
  EXPECT_DOUBLE_EQ(216.0, e);
  EXPECT_DOUBLE_EQ(646.510009765625, f);

  obj = FPDFPage_GetObject(page, 35);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  EXPECT_TRUE(FPDFImageObj_GetMatrix(obj, &a, &b, &c, &d, &e, &f));
  EXPECT_DOUBLE_EQ(69.0, a);
  EXPECT_DOUBLE_EQ(0.0, b);
  EXPECT_DOUBLE_EQ(0.0, c);
  EXPECT_DOUBLE_EQ(51.0, d);
  EXPECT_DOUBLE_EQ(360.0, e);
  EXPECT_DOUBLE_EQ(646.510009765625, f);

  obj = FPDFPage_GetObject(page, 36);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  EXPECT_TRUE(FPDFImageObj_GetMatrix(obj, &a, &b, &c, &d, &e, &f));
  EXPECT_DOUBLE_EQ(59.0, a);
  EXPECT_DOUBLE_EQ(0.0, b);
  EXPECT_DOUBLE_EQ(0.0, c);
  EXPECT_DOUBLE_EQ(45.0, d);
  EXPECT_DOUBLE_EQ(72.0, e);
  EXPECT_DOUBLE_EQ(553.510009765625, f);

  obj = FPDFPage_GetObject(page, 37);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  EXPECT_TRUE(FPDFImageObj_GetMatrix(obj, &a, &b, &c, &d, &e, &f));
  EXPECT_DOUBLE_EQ(55.94000244140625, a);
  EXPECT_DOUBLE_EQ(0.0, b);
  EXPECT_DOUBLE_EQ(0.0, c);
  EXPECT_DOUBLE_EQ(46.950000762939453, d);
  EXPECT_DOUBLE_EQ(216.0, e);
  EXPECT_DOUBLE_EQ(552.510009765625, f);

  obj = FPDFPage_GetObject(page, 38);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  EXPECT_TRUE(FPDFImageObj_GetMatrix(obj, &a, &b, &c, &d, &e, &f));
  EXPECT_DOUBLE_EQ(70.528999328613281, a);
  EXPECT_DOUBLE_EQ(0.0, b);
  EXPECT_DOUBLE_EQ(0.0, c);
  EXPECT_DOUBLE_EQ(43.149997711181641, d);
  EXPECT_DOUBLE_EQ(360.0, e);
  EXPECT_DOUBLE_EQ(553.3599853515625, f);

  UnloadPage(page);
}

TEST_F(FPDFEditEmbedderTest, DestroyPageObject) {
  FPDF_PAGEOBJECT rect = FPDFPageObj_CreateNewRect(10, 10, 20, 20);
  ASSERT_TRUE(rect);

  // There should be no memory leaks with a call to FPDFPageObj_Destroy().
  FPDFPageObj_Destroy(rect);
}

TEST_F(FPDFEditEmbedderTest, GetImageFilters) {
  EXPECT_TRUE(OpenDocument("embedded_images.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Verify that retrieving the filter of a non-image object would fail.
  FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, 32);
  ASSERT_NE(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  ASSERT_EQ(0, FPDFImageObj_GetImageFilterCount(obj));
  EXPECT_EQ(0u, FPDFImageObj_GetImageFilter(obj, 0, nullptr, 0));

  // Verify the returned filter string for an image object with a single filter.
  obj = FPDFPage_GetObject(page, 33);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  ASSERT_EQ(1, FPDFImageObj_GetImageFilterCount(obj));
  unsigned long len = FPDFImageObj_GetImageFilter(obj, 0, nullptr, 0);
  std::vector<char> buf(len);
  static constexpr char kFlateDecode[] = "FlateDecode";
  EXPECT_EQ(sizeof(kFlateDecode),
            FPDFImageObj_GetImageFilter(obj, 0, buf.data(), len));
  EXPECT_STREQ(kFlateDecode, buf.data());
  EXPECT_EQ(0u, FPDFImageObj_GetImageFilter(obj, 1, nullptr, 0));

  // Verify all the filters for an image object with a list of filters.
  obj = FPDFPage_GetObject(page, 38);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  ASSERT_EQ(2, FPDFImageObj_GetImageFilterCount(obj));
  len = FPDFImageObj_GetImageFilter(obj, 0, nullptr, 0);
  buf.clear();
  buf.resize(len);
  static constexpr char kASCIIHexDecode[] = "ASCIIHexDecode";
  EXPECT_EQ(sizeof(kASCIIHexDecode),
            FPDFImageObj_GetImageFilter(obj, 0, buf.data(), len));
  EXPECT_STREQ(kASCIIHexDecode, buf.data());

  len = FPDFImageObj_GetImageFilter(obj, 1, nullptr, 0);
  buf.clear();
  buf.resize(len);
  static constexpr char kDCTDecode[] = "DCTDecode";
  EXPECT_EQ(sizeof(kDCTDecode),
            FPDFImageObj_GetImageFilter(obj, 1, buf.data(), len));
  EXPECT_STREQ(kDCTDecode, buf.data());

  UnloadPage(page);
}

TEST_F(FPDFEditEmbedderTest, GetImageMetadata) {
  ASSERT_TRUE(OpenDocument("embedded_images.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  // Check that getting the metadata of a null object would fail.
  FPDF_IMAGEOBJ_METADATA metadata;
  EXPECT_FALSE(FPDFImageObj_GetImageMetadata(nullptr, page, &metadata));

  // Check that receiving the metadata with a null metadata object would fail.
  FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, 35);
  EXPECT_FALSE(FPDFImageObj_GetImageMetadata(obj, page, nullptr));

  // Check that when retrieving an image object's metadata without passing in
  // |page|, all values are correct, with the last two being default values.
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  ASSERT_TRUE(FPDFImageObj_GetImageMetadata(obj, nullptr, &metadata));
  EXPECT_EQ(7, metadata.marked_content_id);
  EXPECT_EQ(92u, metadata.width);
  EXPECT_EQ(68u, metadata.height);
  EXPECT_FLOAT_EQ(96.0f, metadata.horizontal_dpi);
  EXPECT_FLOAT_EQ(96.0f, metadata.vertical_dpi);
  EXPECT_EQ(0u, metadata.bits_per_pixel);
  EXPECT_EQ(FPDF_COLORSPACE_UNKNOWN, metadata.colorspace);

  // Verify the metadata of a bitmap image with indexed colorspace.
  ASSERT_TRUE(FPDFImageObj_GetImageMetadata(obj, page, &metadata));
  EXPECT_EQ(7, metadata.marked_content_id);
  EXPECT_EQ(92u, metadata.width);
  EXPECT_EQ(68u, metadata.height);
  EXPECT_FLOAT_EQ(96.0f, metadata.horizontal_dpi);
  EXPECT_FLOAT_EQ(96.0f, metadata.vertical_dpi);
  EXPECT_EQ(1u, metadata.bits_per_pixel);
  EXPECT_EQ(FPDF_COLORSPACE_INDEXED, metadata.colorspace);

  // Verify the metadata of an image with RGB colorspace.
  obj = FPDFPage_GetObject(page, 37);
  ASSERT_EQ(FPDF_PAGEOBJ_IMAGE, FPDFPageObj_GetType(obj));
  ASSERT_TRUE(FPDFImageObj_GetImageMetadata(obj, page, &metadata));
  EXPECT_EQ(9, metadata.marked_content_id);
  EXPECT_EQ(126u, metadata.width);
  EXPECT_EQ(106u, metadata.height);
  EXPECT_FLOAT_EQ(162.173752f, metadata.horizontal_dpi);
  EXPECT_FLOAT_EQ(162.555878f, metadata.vertical_dpi);
  EXPECT_EQ(24u, metadata.bits_per_pixel);
  EXPECT_EQ(FPDF_COLORSPACE_DEVICERGB, metadata.colorspace);

  UnloadPage(page);
}
