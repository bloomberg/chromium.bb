// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/fpdf_doc.h"

#include <memory>
#include <vector>

#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/page/cpdf_pagemodule.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_null.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_parser.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/render/cpdf_docrenderdata.h"
#include "core/fpdfdoc/cpdf_dest.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "public/cpp/fpdf_scopers.h"
#include "testing/fx_string_testhelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

class CPDF_TestDocument final : public CPDF_Document {
 public:
  CPDF_TestDocument()
      : CPDF_Document(std::make_unique<CPDF_DocRenderData>(),
                      std::make_unique<CPDF_DocPageData>()) {}

  void SetRoot(CPDF_Dictionary* root) { SetRootForTesting(root); }
  CPDF_IndirectObjectHolder* GetHolder() { return this; }
};

class PDFDocTest : public testing::Test {
 public:
  struct DictObjInfo {
    uint32_t num;
    CPDF_Dictionary* obj;
  };

  void SetUp() override {
    CPDF_PageModule::Create();
    auto pTestDoc = std::make_unique<CPDF_TestDocument>();
    m_pIndirectObjs = pTestDoc->GetHolder();
    m_pRootObj.Reset(m_pIndirectObjs->NewIndirect<CPDF_Dictionary>());
    pTestDoc->SetRoot(m_pRootObj.Get());
    m_pDoc.reset(FPDFDocumentFromCPDFDocument(pTestDoc.release()));
  }

  void TearDown() override {
    m_pRootObj = nullptr;
    m_pIndirectObjs = nullptr;
    m_pDoc.reset();
    CPDF_PageModule::Destroy();
  }

  std::vector<DictObjInfo> CreateDictObjs(int num) {
    std::vector<DictObjInfo> info;
    for (int i = 0; i < num; ++i) {
      // Objects created will be released by the document.
      CPDF_Dictionary* obj = m_pIndirectObjs->NewIndirect<CPDF_Dictionary>();
      info.push_back({obj->GetObjNum(), obj});
    }
    return info;
  }

 protected:
  ScopedFPDFDocument m_pDoc;
  UnownedPtr<CPDF_IndirectObjectHolder> m_pIndirectObjs;
  RetainPtr<CPDF_Dictionary> m_pRootObj;
};

TEST_F(PDFDocTest, FindBookmark) {
  {
    // No bookmark information.
    ScopedFPDFWideString title = GetFPDFWideString(L"");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));

    title = GetFPDFWideString(L"Preface");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));
  }
  {
    // Empty bookmark tree.
    m_pRootObj->SetNewFor<CPDF_Dictionary>("Outlines");
    ScopedFPDFWideString title = GetFPDFWideString(L"");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));

    title = GetFPDFWideString(L"Preface");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));
  }
  {
    // Check on a regular bookmark tree.
    auto bookmarks = CreateDictObjs(3);

    bookmarks[1].obj->SetNewFor<CPDF_String>("Title", L"Chapter 1");
    bookmarks[1].obj->SetNewFor<CPDF_Reference>("Parent", m_pIndirectObjs.Get(),
                                                bookmarks[0].num);
    bookmarks[1].obj->SetNewFor<CPDF_Reference>("Next", m_pIndirectObjs.Get(),
                                                bookmarks[2].num);

    bookmarks[2].obj->SetNewFor<CPDF_String>("Title", L"Chapter 2");
    bookmarks[2].obj->SetNewFor<CPDF_Reference>("Parent", m_pIndirectObjs.Get(),
                                                bookmarks[0].num);
    bookmarks[2].obj->SetNewFor<CPDF_Reference>("Prev", m_pIndirectObjs.Get(),
                                                bookmarks[1].num);

    bookmarks[0].obj->SetNewFor<CPDF_Name>("Type", "Outlines");
    bookmarks[0].obj->SetNewFor<CPDF_Number>("Count", 2);
    bookmarks[0].obj->SetNewFor<CPDF_Reference>("First", m_pIndirectObjs.Get(),
                                                bookmarks[1].num);
    bookmarks[0].obj->SetNewFor<CPDF_Reference>("Last", m_pIndirectObjs.Get(),
                                                bookmarks[2].num);

    m_pRootObj->SetNewFor<CPDF_Reference>("Outlines", m_pIndirectObjs.Get(),
                                          bookmarks[0].num);

    // Title with no match.
    ScopedFPDFWideString title = GetFPDFWideString(L"Chapter 3");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));

    // Title with partial match only.
    title = GetFPDFWideString(L"Chapter");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));

    // Title with a match.
    title = GetFPDFWideString(L"Chapter 2");
    EXPECT_EQ(FPDFBookmarkFromCPDFDictionary(bookmarks[2].obj),
              FPDFBookmark_Find(m_pDoc.get(), title.get()));

    // Title match is case insensitive.
    title = GetFPDFWideString(L"cHaPter 2");
    EXPECT_EQ(FPDFBookmarkFromCPDFDictionary(bookmarks[2].obj),
              FPDFBookmark_Find(m_pDoc.get(), title.get()));
  }
  {
    // Circular bookmarks in depth.
    auto bookmarks = CreateDictObjs(3);

    bookmarks[1].obj->SetNewFor<CPDF_String>("Title", L"Chapter 1");
    bookmarks[1].obj->SetNewFor<CPDF_Reference>("Parent", m_pIndirectObjs.Get(),
                                                bookmarks[0].num);
    bookmarks[1].obj->SetNewFor<CPDF_Reference>("First", m_pIndirectObjs.Get(),
                                                bookmarks[2].num);

    bookmarks[2].obj->SetNewFor<CPDF_String>("Title", L"Chapter 2");
    bookmarks[2].obj->SetNewFor<CPDF_Reference>("Parent", m_pIndirectObjs.Get(),
                                                bookmarks[1].num);
    bookmarks[2].obj->SetNewFor<CPDF_Reference>("First", m_pIndirectObjs.Get(),
                                                bookmarks[1].num);

    bookmarks[0].obj->SetNewFor<CPDF_Name>("Type", "Outlines");
    bookmarks[0].obj->SetNewFor<CPDF_Number>("Count", 2);
    bookmarks[0].obj->SetNewFor<CPDF_Reference>("First", m_pIndirectObjs.Get(),
                                                bookmarks[1].num);
    bookmarks[0].obj->SetNewFor<CPDF_Reference>("Last", m_pIndirectObjs.Get(),
                                                bookmarks[2].num);

    m_pRootObj->SetNewFor<CPDF_Reference>("Outlines", m_pIndirectObjs.Get(),
                                          bookmarks[0].num);

    // Title with no match.
    ScopedFPDFWideString title = GetFPDFWideString(L"Chapter 3");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));

    // Title with a match.
    title = GetFPDFWideString(L"Chapter 2");
    EXPECT_EQ(FPDFBookmarkFromCPDFDictionary(bookmarks[2].obj),
              FPDFBookmark_Find(m_pDoc.get(), title.get()));
  }
  {
    // Circular bookmarks in breadth.
    auto bookmarks = CreateDictObjs(4);

    bookmarks[1].obj->SetNewFor<CPDF_String>("Title", L"Chapter 1");
    bookmarks[1].obj->SetNewFor<CPDF_Reference>("Parent", m_pIndirectObjs.Get(),
                                                bookmarks[0].num);
    bookmarks[1].obj->SetNewFor<CPDF_Reference>("Next", m_pIndirectObjs.Get(),
                                                bookmarks[2].num);

    bookmarks[2].obj->SetNewFor<CPDF_String>("Title", L"Chapter 2");
    bookmarks[2].obj->SetNewFor<CPDF_Reference>("Parent", m_pIndirectObjs.Get(),
                                                bookmarks[0].num);
    bookmarks[2].obj->SetNewFor<CPDF_Reference>("Next", m_pIndirectObjs.Get(),
                                                bookmarks[3].num);

    bookmarks[3].obj->SetNewFor<CPDF_String>("Title", L"Chapter 3");
    bookmarks[3].obj->SetNewFor<CPDF_Reference>("Parent", m_pIndirectObjs.Get(),
                                                bookmarks[0].num);
    bookmarks[3].obj->SetNewFor<CPDF_Reference>("Next", m_pIndirectObjs.Get(),
                                                bookmarks[1].num);

    bookmarks[0].obj->SetNewFor<CPDF_Name>("Type", "Outlines");
    bookmarks[0].obj->SetNewFor<CPDF_Number>("Count", 2);
    bookmarks[0].obj->SetNewFor<CPDF_Reference>("First", m_pIndirectObjs.Get(),
                                                bookmarks[1].num);
    bookmarks[0].obj->SetNewFor<CPDF_Reference>("Last", m_pIndirectObjs.Get(),
                                                bookmarks[2].num);

    m_pRootObj->SetNewFor<CPDF_Reference>("Outlines", m_pIndirectObjs.Get(),
                                          bookmarks[0].num);

    // Title with no match.
    ScopedFPDFWideString title = GetFPDFWideString(L"Chapter 8");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));

    // Title with a match.
    title = GetFPDFWideString(L"Chapter 3");
    EXPECT_EQ(FPDFBookmarkFromCPDFDictionary(bookmarks[3].obj),
              FPDFBookmark_Find(m_pDoc.get(), title.get()));
  }
}

TEST_F(PDFDocTest, GetLocationInPage) {
  auto array = pdfium::MakeRetain<CPDF_Array>();
  array->AppendNew<CPDF_Number>(0);  // Page Index.
  array->AppendNew<CPDF_Name>("XYZ");
  array->AppendNew<CPDF_Number>(4);  // X
  array->AppendNew<CPDF_Number>(5);  // Y
  array->AppendNew<CPDF_Number>(6);  // Zoom.

  FPDF_BOOL hasX;
  FPDF_BOOL hasY;
  FPDF_BOOL hasZoom;
  FS_FLOAT x;
  FS_FLOAT y;
  FS_FLOAT zoom;

  EXPECT_TRUE(FPDFDest_GetLocationInPage(FPDFDestFromCPDFArray(array.Get()),
                                         &hasX, &hasY, &hasZoom, &x, &y,
                                         &zoom));
  EXPECT_TRUE(hasX);
  EXPECT_TRUE(hasY);
  EXPECT_TRUE(hasZoom);
  EXPECT_EQ(4, x);
  EXPECT_EQ(5, y);
  EXPECT_EQ(6, zoom);

  array->SetNewAt<CPDF_Null>(2);
  array->SetNewAt<CPDF_Null>(3);
  array->SetNewAt<CPDF_Null>(4);
  EXPECT_TRUE(FPDFDest_GetLocationInPage(FPDFDestFromCPDFArray(array.Get()),
                                         &hasX, &hasY, &hasZoom, &x, &y,
                                         &zoom));
  EXPECT_FALSE(hasX);
  EXPECT_FALSE(hasY);
  EXPECT_FALSE(hasZoom);

  array = pdfium::MakeRetain<CPDF_Array>();
  EXPECT_FALSE(FPDFDest_GetLocationInPage(FPDFDestFromCPDFArray(array.Get()),
                                          &hasX, &hasY, &hasZoom, &x, &y,
                                          &zoom));
}
