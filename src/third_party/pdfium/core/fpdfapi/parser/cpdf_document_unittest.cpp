// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/parser/cpdf_document.h"

#include <memory>
#include <utility>

#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/page/cpdf_pagemodule.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_boolean.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_linearized_header.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_parser.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/render/cpdf_docrenderdata.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/base/ptr_util.h"

namespace {

const int kNumTestPages = 7;

CPDF_Dictionary* CreatePageTreeNode(RetainPtr<CPDF_Array> kids,
                                    CPDF_Document* pDoc,
                                    int count) {
  CPDF_Array* pUnowned = pDoc->AddIndirectObject(std::move(kids))->AsArray();
  CPDF_Dictionary* pageNode = pDoc->NewIndirect<CPDF_Dictionary>();
  pageNode->SetNewFor<CPDF_String>("Type", "Pages", false);
  pageNode->SetNewFor<CPDF_Reference>("Kids", pDoc, pUnowned->GetObjNum());
  pageNode->SetNewFor<CPDF_Number>("Count", count);
  for (size_t i = 0; i < pUnowned->size(); i++) {
    pUnowned->GetDictAt(i)->SetNewFor<CPDF_Reference>("Parent", pDoc,
                                                      pageNode->GetObjNum());
  }
  return pageNode;
}

RetainPtr<CPDF_Dictionary> CreateNumberedPage(size_t number) {
  auto page = pdfium::MakeRetain<CPDF_Dictionary>();
  page->SetNewFor<CPDF_String>("Type", "Page", false);
  page->SetNewFor<CPDF_Number>("PageNumbering", static_cast<int>(number));
  return page;
}

class CPDF_TestDocumentForPages final : public CPDF_Document {
 public:
  CPDF_TestDocumentForPages()
      : CPDF_Document(pdfium::MakeUnique<CPDF_DocRenderData>(),
                      pdfium::MakeUnique<CPDF_DocPageData>()) {
    // Set up test
    auto zeroToTwo = pdfium::MakeRetain<CPDF_Array>();
    zeroToTwo->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(0))->GetObjNum());
    zeroToTwo->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(1))->GetObjNum());
    zeroToTwo->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(2))->GetObjNum());
    CPDF_Dictionary* branch1 =
        CreatePageTreeNode(std::move(zeroToTwo), this, 3);

    auto zeroToThree = pdfium::MakeRetain<CPDF_Array>();
    zeroToThree->AddNew<CPDF_Reference>(this, branch1->GetObjNum());
    zeroToThree->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(3))->GetObjNum());
    CPDF_Dictionary* branch2 =
        CreatePageTreeNode(std::move(zeroToThree), this, 4);

    auto fourFive = pdfium::MakeRetain<CPDF_Array>();
    fourFive->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(4))->GetObjNum());
    fourFive->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(5))->GetObjNum());
    CPDF_Dictionary* branch3 = CreatePageTreeNode(std::move(fourFive), this, 2);

    auto justSix = pdfium::MakeRetain<CPDF_Array>();
    justSix->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(6))->GetObjNum());
    CPDF_Dictionary* branch4 = CreatePageTreeNode(std::move(justSix), this, 1);

    auto allPages = pdfium::MakeRetain<CPDF_Array>();
    allPages->AddNew<CPDF_Reference>(this, branch2->GetObjNum());
    allPages->AddNew<CPDF_Reference>(this, branch3->GetObjNum());
    allPages->AddNew<CPDF_Reference>(this, branch4->GetObjNum());
    CPDF_Dictionary* pagesDict =
        CreatePageTreeNode(std::move(allPages), this, kNumTestPages);

    m_pRootDict.Reset(NewIndirect<CPDF_Dictionary>());
    m_pRootDict->SetNewFor<CPDF_Reference>("Pages", this,
                                           pagesDict->GetObjNum());
    m_PageList.resize(kNumTestPages);
  }

  void SetTreeSize(int size) {
    m_pRootDict->SetNewFor<CPDF_Number>("Count", size);
    m_PageList.resize(size);
  }
};

class CPDF_TestDocumentWithPageWithoutPageNum final : public CPDF_Document {
 public:
  CPDF_TestDocumentWithPageWithoutPageNum()
      : CPDF_Document(pdfium::MakeUnique<CPDF_DocRenderData>(),
                      pdfium::MakeUnique<CPDF_DocPageData>()) {
    // Set up test
    auto allPages = pdfium::MakeRetain<CPDF_Array>();
    allPages->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(0))->GetObjNum());
    allPages->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(1))->GetObjNum());
    // Page without pageNum.
    inlined_page_ = allPages->Add(CreateNumberedPage(2));
    CPDF_Dictionary* pagesDict =
        CreatePageTreeNode(std::move(allPages), this, 3);
    m_pRootDict.Reset(NewIndirect<CPDF_Dictionary>());
    m_pRootDict->SetNewFor<CPDF_Reference>("Pages", this,
                                           pagesDict->GetObjNum());
    m_PageList.resize(3);
  }

  const CPDF_Object* inlined_page() const { return inlined_page_; }

 private:
  const CPDF_Object* inlined_page_;
};

class TestLinearized final : public CPDF_LinearizedHeader {
 public:
  explicit TestLinearized(CPDF_Dictionary* dict)
      : CPDF_LinearizedHeader(dict, 0) {}
};

class CPDF_TestDocPagesWithoutKids final : public CPDF_Document {
 public:
  CPDF_TestDocPagesWithoutKids()
      : CPDF_Document(pdfium::MakeUnique<CPDF_DocRenderData>(),
                      pdfium::MakeUnique<CPDF_DocPageData>()) {
    CPDF_Dictionary* pagesDict = NewIndirect<CPDF_Dictionary>();
    pagesDict->SetNewFor<CPDF_Name>("Type", "Pages");
    pagesDict->SetNewFor<CPDF_Number>("Count", 3);
    m_PageList.resize(10);
    m_pRootDict.Reset(NewIndirect<CPDF_Dictionary>());
    m_pRootDict->SetNewFor<CPDF_Reference>("Pages", this,
                                           pagesDict->GetObjNum());
  }
};

class CPDF_TestDocumentAllowSetParser final : public CPDF_Document {
 public:
  CPDF_TestDocumentAllowSetParser()
      : CPDF_Document(pdfium::MakeUnique<CPDF_DocRenderData>(),
                      pdfium::MakeUnique<CPDF_DocPageData>()) {}

  using CPDF_Document::SetParser;
};

}  // namespace

class cpdf_document_test : public testing::Test {
 public:
  void SetUp() override { CPDF_PageModule::Create(); }
  void TearDown() override { CPDF_PageModule::Destroy(); }
};

TEST_F(cpdf_document_test, GetPages) {
  std::unique_ptr<CPDF_TestDocumentForPages> document =
      pdfium::MakeUnique<CPDF_TestDocumentForPages>();
  for (int i = 0; i < kNumTestPages; i++) {
    CPDF_Dictionary* page = document->GetPageDictionary(i);
    ASSERT_TRUE(page);
    ASSERT_TRUE(page->KeyExist("PageNumbering"));
    EXPECT_EQ(i, page->GetIntegerFor("PageNumbering"));
  }
  CPDF_Dictionary* page = document->GetPageDictionary(kNumTestPages);
  EXPECT_FALSE(page);
}

TEST_F(cpdf_document_test, GetPageWithoutObjNumTwice) {
  auto document = pdfium::MakeUnique<CPDF_TestDocumentWithPageWithoutPageNum>();
  CPDF_Dictionary* page = document->GetPageDictionary(2);
  ASSERT_TRUE(page);
  ASSERT_EQ(document->inlined_page(), page);

  CPDF_Dictionary* second_call_page = document->GetPageDictionary(2);
  EXPECT_TRUE(second_call_page);
  EXPECT_EQ(page, second_call_page);
}

TEST_F(cpdf_document_test, GetPagesReverseOrder) {
  std::unique_ptr<CPDF_TestDocumentForPages> document =
      pdfium::MakeUnique<CPDF_TestDocumentForPages>();
  for (int i = 6; i >= 0; i--) {
    CPDF_Dictionary* page = document->GetPageDictionary(i);
    ASSERT_TRUE(page);
    ASSERT_TRUE(page->KeyExist("PageNumbering"));
    EXPECT_EQ(i, page->GetIntegerFor("PageNumbering"));
  }
  CPDF_Dictionary* page = document->GetPageDictionary(kNumTestPages);
  EXPECT_FALSE(page);
}

TEST_F(cpdf_document_test, GetPagesInDisorder) {
  std::unique_ptr<CPDF_TestDocumentForPages> document =
      pdfium::MakeUnique<CPDF_TestDocumentForPages>();

  CPDF_Dictionary* page = document->GetPageDictionary(1);
  ASSERT_TRUE(page);
  ASSERT_TRUE(page->KeyExist("PageNumbering"));
  EXPECT_EQ(1, page->GetIntegerFor("PageNumbering"));

  page = document->GetPageDictionary(3);
  ASSERT_TRUE(page);
  ASSERT_TRUE(page->KeyExist("PageNumbering"));
  EXPECT_EQ(3, page->GetIntegerFor("PageNumbering"));

  page = document->GetPageDictionary(kNumTestPages);
  EXPECT_FALSE(page);

  page = document->GetPageDictionary(6);
  ASSERT_TRUE(page);
  ASSERT_TRUE(page->KeyExist("PageNumbering"));
  EXPECT_EQ(6, page->GetIntegerFor("PageNumbering"));
}

TEST_F(cpdf_document_test, UseCachedPageObjNumIfHaveNotPagesDict) {
  // ObjNum can be added in CPDF_DataAvail::IsPageAvail, and PagesDict
  // can be not exists in this case.
  // (case, when hint table is used to page check in CPDF_DataAvail).
  auto dict = pdfium::MakeRetain<CPDF_Dictionary>();
  dict->SetNewFor<CPDF_Boolean>("Linearized", true);
  const int page_count = 100;
  dict->SetNewFor<CPDF_Number>("N", page_count);
  auto linearized = pdfium::MakeUnique<TestLinearized>(dict.Get());
  auto parser = pdfium::MakeUnique<CPDF_Parser>();
  parser->SetLinearizedHeader(std::move(linearized));
  CPDF_TestDocumentAllowSetParser document;
  document.SetParser(std::move(parser));
  document.LoadPages();
  ASSERT_EQ(page_count, document.GetPageCount());
  CPDF_Object* page_stub = document.NewIndirect<CPDF_Dictionary>();
  const uint32_t obj_num = page_stub->GetObjNum();
  const int test_page_num = 33;

  EXPECT_FALSE(document.IsPageLoaded(test_page_num));
  EXPECT_EQ(nullptr, document.GetPageDictionary(test_page_num));

  document.SetPageObjNum(test_page_num, obj_num);
  EXPECT_TRUE(document.IsPageLoaded(test_page_num));
  EXPECT_EQ(page_stub, document.GetPageDictionary(test_page_num));
}

TEST_F(cpdf_document_test, CountGreaterThanPageTree) {
  std::unique_ptr<CPDF_TestDocumentForPages> document =
      pdfium::MakeUnique<CPDF_TestDocumentForPages>();
  document->SetTreeSize(kNumTestPages + 3);
  for (int i = 0; i < kNumTestPages; i++)
    EXPECT_TRUE(document->GetPageDictionary(i));
  for (int i = kNumTestPages; i < kNumTestPages + 4; i++)
    EXPECT_FALSE(document->GetPageDictionary(i));
  EXPECT_TRUE(document->GetPageDictionary(kNumTestPages - 1));
}

TEST_F(cpdf_document_test, PagesWithoutKids) {
  // Set up a document with Pages dict without kids, and Count = 3
  auto pDoc = pdfium::MakeUnique<CPDF_TestDocPagesWithoutKids>();
  EXPECT_TRUE(pDoc->GetPageDictionary(0));
  // Test GetPage does not fetch pages out of range
  for (int i = 1; i < 5; i++)
    EXPECT_FALSE(pDoc->GetPageDictionary(i));

  EXPECT_TRUE(pDoc->GetPageDictionary(0));
}
