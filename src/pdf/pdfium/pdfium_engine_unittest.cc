// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_engine.h"

#include <stdint.h>

#include <utility>

#include "base/callback.h"
#include "base/hash/md5.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_move_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "pdf/document_attachment_info.h"
#include "pdf/document_layout.h"
#include "pdf/document_metadata.h"
#include "pdf/pdf_features.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/ppapi_migration/callback.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_document_loader.h"
#include "pdf/ui/thumbnail.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Return;
using ::testing::StrictMock;

MATCHER_P2(LayoutWithSize, width, height, "") {
  return arg.size() == gfx::Size(width, height);
}

MATCHER_P(LayoutWithOptions, options, "") {
  return arg.options() == options;
}

class MockTestClient : public TestClient {
 public:
  MockTestClient() {
    ON_CALL(*this, ProposeDocumentLayout)
        .WillByDefault([this](const DocumentLayout& layout) {
          TestClient::ProposeDocumentLayout(layout);
        });
  }

  MOCK_METHOD(void,
              ProposeDocumentLayout,
              (const DocumentLayout& layout),
              (override));
  MOCK_METHOD(void, ScrollToPage, (int page), (override));
  MOCK_METHOD(void,
              ScheduleTaskOnMainThread,
              (const base::Location& from_here,
               ResultCallback callback,
               int32_t result,
               base::TimeDelta delay),
              (override));
  MOCK_METHOD(void, DocumentFocusChanged, (bool), (override));
  MOCK_METHOD(void, SetLinkUnderCursor, (const std::string&), (override));
};

}  // namespace

class PDFiumEngineTest : public PDFiumTestBase {
 protected:
  void ExpectPageRect(const PDFiumEngine& engine,
                      size_t page_index,
                      const gfx::Rect& expected_rect) {
    const PDFiumPage& page = GetPDFiumPageForTest(engine, page_index);
    EXPECT_EQ(expected_rect, page.rect());
  }

  // Tries to load a PDF incrementally, returning `true` if the PDF actually was
  // loaded incrementally. Note that this function will return `false` if
  // incremental loading fails, but also if incremental loading is disabled.
  bool TryLoadIncrementally() {
    NiceMock<MockTestClient> client;
    InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
        &client, FILE_PATH_LITERAL("linearized.pdf"));
    if (!initialize_result.engine) {
      ADD_FAILURE();
      return false;
    }
    PDFiumEngine& engine = *initialize_result.engine;

    // Load enough for the document to become partially available.
    initialize_result.document_loader->SimulateLoadData(8192);

    bool loaded_incrementally;
    if (engine.GetNumberOfPages() == 0) {
      // This is not necessarily a test failure; it just indicates incremental
      // loading is not occurring.
      engine.PluginSizeUpdated({});
      loaded_incrementally = false;
    } else {
      // Note: Plugin size chosen so all pages of the document are visible. The
      // engine only updates availability incrementally for visible pages.
      EXPECT_EQ(0, CountAvailablePages(engine));
      engine.PluginSizeUpdated({1024, 4096});
      int available_pages = CountAvailablePages(engine);
      loaded_incrementally =
          0 < available_pages && available_pages < engine.GetNumberOfPages();
    }

    // Verify that loading can finish.
    initialize_result.FinishLoading();
    EXPECT_EQ(engine.GetNumberOfPages(), CountAvailablePages(engine));

    return loaded_incrementally;
  }

  void FinishWithPluginSizeUpdated(MockTestClient& client,
                                   PDFiumEngine& engine) {
    ResultCallback callback;
    EXPECT_CALL(client, ScheduleTaskOnMainThread)
        .WillOnce(MoveArg<1>(&callback));
    engine.PluginSizeUpdated({});

    ASSERT_TRUE(callback);
    std::move(callback).Run(0);
  }

  // Counts the number of available pages. Returns `int` instead of `size_t` for
  // consistency with `PDFiumEngine::GetNumberOfPages()`.
  int CountAvailablePages(const PDFiumEngine& engine) {
    int available_pages = 0;
    for (int i = 0; i < engine.GetNumberOfPages(); ++i) {
      if (GetPDFiumPageForTest(engine, i).available())
        ++available_pages;
    }
    return available_pages;
  }
};

TEST_F(PDFiumEngineTest, InitializeWithRectanglesMultiPagesPdf) {
  NiceMock<MockTestClient> client;

  // ProposeDocumentLayout() gets called twice during loading because
  // PDFiumEngine::ContinueLoadingDocument() calls LoadBody() (which eventually
  // triggers a layout proposal), and then calls FinishLoadingDocument() (since
  // the document is complete), which calls LoadBody() again. Coalescing these
  // proposals is not correct unless we address the issue covered by
  // PDFiumEngineTest.ProposeDocumentLayoutWithOverlap.
  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1664)))
      .Times(2);

  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(5, engine->GetNumberOfPages());

  ExpectPageRect(*engine, 0, {38, 3, 266, 333});
  ExpectPageRect(*engine, 1, {5, 350, 333, 266});
  ExpectPageRect(*engine, 2, {38, 630, 266, 333});
  ExpectPageRect(*engine, 3, {38, 977, 266, 333});
  ExpectPageRect(*engine, 4, {38, 1324, 266, 333});
}

TEST_F(PDFiumEngineTest, InitializeWithRectanglesMultiPagesPdfInTwoUpView) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  DocumentLayout::Options options;
  options.set_page_spread(DocumentLayout::PageSpread::kTwoUpOdd);
  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithOptions(options)))
      .WillOnce(Return());
  engine->SetTwoUpView(true);

  engine->ApplyDocumentLayout(options);

  ASSERT_EQ(5, engine->GetNumberOfPages());

  ExpectPageRect(*engine, 0, {72, 3, 266, 333});
  ExpectPageRect(*engine, 1, {340, 3, 333, 266});
  ExpectPageRect(*engine, 2, {72, 346, 266, 333});
  ExpectPageRect(*engine, 3, {340, 346, 266, 333});
  ExpectPageRect(*engine, 4, {68, 689, 266, 333});
}

TEST_F(PDFiumEngineTest, AppendBlankPagesWithFewerPages) {
  NiceMock<MockTestClient> client;
  {
    InSequence normal_then_append;
    EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1664)))
        .Times(2);
    EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(276, 1037)));
  }

  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  engine->AppendBlankPages(3);
  ASSERT_EQ(3, engine->GetNumberOfPages());

  ExpectPageRect(*engine, 0, {5, 3, 266, 333});
  ExpectPageRect(*engine, 1, {5, 350, 266, 333});
  ExpectPageRect(*engine, 2, {5, 697, 266, 333});
}

TEST_F(PDFiumEngineTest, AppendBlankPagesWithMorePages) {
  NiceMock<MockTestClient> client;
  {
    InSequence normal_then_append;
    EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1664)))
        .Times(2);
    EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(276, 2425)));
  }

  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  engine->AppendBlankPages(7);
  ASSERT_EQ(7, engine->GetNumberOfPages());

  ExpectPageRect(*engine, 0, {5, 3, 266, 333});
  ExpectPageRect(*engine, 1, {5, 350, 266, 333});
  ExpectPageRect(*engine, 2, {5, 697, 266, 333});
  ExpectPageRect(*engine, 3, {5, 1044, 266, 333});
  ExpectPageRect(*engine, 4, {5, 1391, 266, 333});
  ExpectPageRect(*engine, 5, {5, 1738, 266, 333});
  ExpectPageRect(*engine, 6, {5, 2085, 266, 333});
}

TEST_F(PDFiumEngineTest, ProposeDocumentLayoutWithOverlap) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1463)))
      .WillOnce(Return());
  engine->RotateClockwise();

  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1664)))
      .WillOnce(Return());
  engine->RotateCounterclockwise();
}

TEST_F(PDFiumEngineTest, ApplyDocumentLayoutBeforePluginSizeUpdated) {
  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  initialize_result.FinishLoading();
  PDFiumEngine& engine = *initialize_result.engine;

  DocumentLayout::Options options;
  options.RotatePagesClockwise();
  EXPECT_CALL(client, ScrollToPage(-1)).Times(0);
  EXPECT_EQ(gfx::Size(343, 1664), engine.ApplyDocumentLayout(options));

  EXPECT_CALL(client, ScrollToPage(-1)).Times(1);
  ASSERT_NO_FATAL_FAILURE(FinishWithPluginSizeUpdated(client, engine));
}

TEST_F(PDFiumEngineTest, ApplyDocumentLayoutAvoidsInfiniteLoop) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  DocumentLayout::Options options;
  EXPECT_CALL(client, ScrollToPage(-1)).Times(0);
  EXPECT_EQ(gfx::Size(343, 1664), engine->ApplyDocumentLayout(options));

  options.RotatePagesClockwise();
  EXPECT_CALL(client, ScrollToPage(-1)).Times(1);
  EXPECT_EQ(gfx::Size(343, 1463), engine->ApplyDocumentLayout(options));
  EXPECT_EQ(gfx::Size(343, 1463), engine->ApplyDocumentLayout(options));
}

TEST_F(PDFiumEngineTest, GetDocumentAttachments) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("embedded_attachments.pdf"));
  ASSERT_TRUE(engine);

  const std::vector<DocumentAttachmentInfo>& attachments =
      engine->GetDocumentAttachmentInfoList();
  ASSERT_EQ(3u, attachments.size());

  {
    const DocumentAttachmentInfo& attachment = attachments[0];
    EXPECT_EQ("1.txt", base::UTF16ToUTF8(attachment.name));
    EXPECT_TRUE(attachment.is_readable);
    EXPECT_EQ(4u, attachment.size_bytes);
    EXPECT_EQ("D:20170712214438-07'00'",
              base::UTF16ToUTF8(attachment.creation_date));
    EXPECT_EQ("D:20160115091400", base::UTF16ToUTF8(attachment.modified_date));

    std::vector<uint8_t> content = engine->GetAttachmentData(0);
    ASSERT_EQ(attachment.size_bytes, content.size());
    std::string content_str(content.begin(), content.end());
    EXPECT_EQ("test", content_str);
  }

  {
    static constexpr char kCheckSum[] = "72afcddedf554dda63c0c88e06f1ce18";
    const DocumentAttachmentInfo& attachment = attachments[1];
    EXPECT_EQ("attached.pdf", base::UTF16ToUTF8(attachment.name));
    EXPECT_TRUE(attachment.is_readable);
    EXPECT_EQ(5869u, attachment.size_bytes);
    EXPECT_EQ("D:20170712214443-07'00'",
              base::UTF16ToUTF8(attachment.creation_date));
    EXPECT_EQ("D:20170712214410", base::UTF16ToUTF8(attachment.modified_date));

    std::vector<uint8_t> content = engine->GetAttachmentData(1);
    ASSERT_EQ(attachment.size_bytes, content.size());
    // The whole attachment content is too long to do string comparison.
    // Instead, we only verify the checksum value here.
    base::MD5Digest hash;
    base::MD5Sum(content.data(), content.size(), &hash);
    EXPECT_EQ(kCheckSum, base::MD5DigestToBase16(hash));
  }

  {
    // Test attachments with no creation date or last modified date.
    const DocumentAttachmentInfo& attachment = attachments[2];
    EXPECT_EQ("附錄.txt", base::UTF16ToUTF8(attachment.name));
    EXPECT_TRUE(attachment.is_readable);
    EXPECT_EQ(5u, attachment.size_bytes);
    EXPECT_THAT(attachment.creation_date, IsEmpty());
    EXPECT_THAT(attachment.modified_date, IsEmpty());

    std::vector<uint8_t> content = engine->GetAttachmentData(2);
    ASSERT_EQ(attachment.size_bytes, content.size());
    std::string content_str(content.begin(), content.end());
    EXPECT_EQ("test\n", content_str);
  }
}

TEST_F(PDFiumEngineTest, GetInvalidDocumentAttachment) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("invalid_attachment.pdf"));
  ASSERT_TRUE(engine);

  // Test on a document with one invalid attachment, which can make
  // FPDFDoc_GetAttachment() fail. This particular attachment is invalid due
  // to its key value violating the `Limits` entry.
  const std::vector<DocumentAttachmentInfo>& attachments =
      engine->GetDocumentAttachmentInfoList();
  ASSERT_EQ(1u, attachments.size());

  const DocumentAttachmentInfo& attachment = attachments[0];
  EXPECT_THAT(attachment.name, IsEmpty());
  EXPECT_FALSE(attachment.is_readable);
  EXPECT_EQ(0u, attachment.size_bytes);
  EXPECT_THAT(attachment.creation_date, IsEmpty());
  EXPECT_THAT(attachment.modified_date, IsEmpty());
}

TEST_F(PDFiumEngineTest, GetDocumentAttachmentWithInvalidData) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("embedded_attachments_invalid_data.pdf"));
  ASSERT_TRUE(engine);

  const std::vector<DocumentAttachmentInfo>& attachments =
      engine->GetDocumentAttachmentInfoList();
  ASSERT_EQ(1u, attachments.size());

  // Test on an attachment which FPDFAttachment_GetFile() fails to retrieve data
  // from.
  const DocumentAttachmentInfo& attachment = attachments[0];
  EXPECT_EQ("1.txt", base::UTF16ToUTF8(attachment.name));
  EXPECT_FALSE(attachment.is_readable);
  EXPECT_EQ(0u, attachment.size_bytes);
  EXPECT_THAT(attachment.creation_date, IsEmpty());
  EXPECT_THAT(attachment.modified_date, IsEmpty());
}

TEST_F(PDFiumEngineTest, NoDocumentAttachmentInfo) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_EQ(0u, engine->GetDocumentAttachmentInfoList().size());
}

TEST_F(PDFiumEngineTest, GetDocumentMetadata) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("document_info.pdf"));
  ASSERT_TRUE(engine);

  const DocumentMetadata& doc_metadata = engine->GetDocumentMetadata();

  EXPECT_EQ(PdfVersion::k1_7, doc_metadata.version);
  EXPECT_EQ(714u, doc_metadata.size_bytes);
  EXPECT_FALSE(doc_metadata.linearized);
  EXPECT_EQ("Sample PDF Document Info", doc_metadata.title);
  EXPECT_EQ("Chromium Authors", doc_metadata.author);
  EXPECT_EQ("Testing", doc_metadata.subject);
  EXPECT_EQ("testing,chromium,pdfium,document,info", doc_metadata.keywords);
  EXPECT_EQ("Your Preferred Text Editor", doc_metadata.creator);
  EXPECT_EQ("fixup_pdf_template.py", doc_metadata.producer);

  base::Time expected_creation_date;
  ASSERT_TRUE(base::Time::FromUTCString("2020-02-05 15:39:12",
                                        &expected_creation_date));
  EXPECT_EQ(expected_creation_date, doc_metadata.creation_date);

  base::Time expected_mod_date;
  ASSERT_TRUE(
      base::Time::FromUTCString("2020-02-06 09:42:34", &expected_mod_date));
  EXPECT_EQ(expected_mod_date, doc_metadata.mod_date);
}

TEST_F(PDFiumEngineTest, GetEmptyDocumentMetadata) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  const DocumentMetadata& doc_metadata = engine->GetDocumentMetadata();

  EXPECT_EQ(PdfVersion::k1_7, doc_metadata.version);
  EXPECT_EQ(786u, doc_metadata.size_bytes);
  EXPECT_FALSE(doc_metadata.linearized);
  EXPECT_THAT(doc_metadata.title, IsEmpty());
  EXPECT_THAT(doc_metadata.author, IsEmpty());
  EXPECT_THAT(doc_metadata.subject, IsEmpty());
  EXPECT_THAT(doc_metadata.keywords, IsEmpty());
  EXPECT_THAT(doc_metadata.creator, IsEmpty());
  EXPECT_THAT(doc_metadata.producer, IsEmpty());
  EXPECT_TRUE(doc_metadata.creation_date.is_null());
  EXPECT_TRUE(doc_metadata.mod_date.is_null());
}

TEST_F(PDFiumEngineTest, GetLinearizedDocumentMetadata) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("linearized.pdf"));
  ASSERT_TRUE(engine);
  EXPECT_TRUE(engine->GetDocumentMetadata().linearized);
}

TEST_F(PDFiumEngineTest, GetBadPdfVersion) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("bad_version.pdf"));
  ASSERT_TRUE(engine);

  const DocumentMetadata& doc_metadata = engine->GetDocumentMetadata();
  EXPECT_EQ(PdfVersion::kUnknown, doc_metadata.version);
}

TEST_F(PDFiumEngineTest, PluginSizeUpdatedBeforeLoad) {
  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  PDFiumEngine& engine = *initialize_result.engine;

  engine.PluginSizeUpdated({});
  initialize_result.FinishLoading();

  EXPECT_EQ(engine.GetNumberOfPages(), CountAvailablePages(engine));
}

TEST_F(PDFiumEngineTest, PluginSizeUpdatedDuringLoad) {
  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  PDFiumEngine& engine = *initialize_result.engine;

  EXPECT_TRUE(initialize_result.document_loader->SimulateLoadData(1024));
  engine.PluginSizeUpdated({});
  initialize_result.FinishLoading();

  EXPECT_EQ(engine.GetNumberOfPages(), CountAvailablePages(engine));
}

TEST_F(PDFiumEngineTest, PluginSizeUpdatedAfterLoad) {
  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  PDFiumEngine& engine = *initialize_result.engine;

  initialize_result.FinishLoading();
  ASSERT_NO_FATAL_FAILURE(FinishWithPluginSizeUpdated(client, engine));

  EXPECT_EQ(engine.GetNumberOfPages(), CountAvailablePages(engine));
}

TEST_F(PDFiumEngineTest, OnLeftMouseDownBeforePluginSizeUpdated) {
  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  initialize_result.FinishLoading();
  PDFiumEngine& engine = *initialize_result.engine;

  EXPECT_TRUE(engine.HandleInputEvent(blink::WebMouseEvent(
      blink::WebInputEvent::Type::kMouseDown, {0, 0}, {100, 200},
      blink::WebPointerProperties::Button::kLeft, /*click_count_param=*/1,
      blink::WebInputEvent::Modifiers::kLeftButtonDown,
      blink::WebInputEvent::GetStaticTimeStampForTests())));
}

TEST_F(PDFiumEngineTest, OnLeftMouseDownAfterPluginSizeUpdated) {
  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  initialize_result.FinishLoading();
  PDFiumEngine& engine = *initialize_result.engine;

  engine.PluginSizeUpdated({300, 400});
  EXPECT_TRUE(engine.HandleInputEvent(blink::WebMouseEvent(
      blink::WebInputEvent::Type::kMouseDown, {0, 0}, {100, 200},
      blink::WebPointerProperties::Button::kLeft, /*click_count_param=*/1,
      blink::WebInputEvent::Modifiers::kLeftButtonDown,
      blink::WebInputEvent::GetStaticTimeStampForTests())));
}

TEST_F(PDFiumEngineTest, IncrementalLoadingFeatureDefault) {
  EXPECT_FALSE(TryLoadIncrementally());
}

TEST_F(PDFiumEngineTest, IncrementalLoadingFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPdfIncrementalLoading);
  EXPECT_TRUE(TryLoadIncrementally());
}

TEST_F(PDFiumEngineTest, IncrementalLoadingFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kPdfIncrementalLoading);
  EXPECT_FALSE(TryLoadIncrementally());
}

TEST_F(PDFiumEngineTest, RequestThumbnail) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  const int num_pages = engine->GetNumberOfPages();
  ASSERT_EQ(5, num_pages);
  ASSERT_EQ(num_pages, CountAvailablePages(*engine));

  // Each page should immediately return a thumbnail.
  for (int i = 0; i < num_pages; ++i) {
    base::MockCallback<SendThumbnailCallback> send_callback;
    EXPECT_CALL(send_callback, Run);
    engine->RequestThumbnail(/*page_index=*/i, /*device_pixel_ratio=*/1,
                             send_callback.Get());
  }
}

TEST_F(PDFiumEngineTest, RequestThumbnailLinearized) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPdfIncrementalLoading);

  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("linearized.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  PDFiumEngine& engine = *initialize_result.engine;

  // Load only some pages.
  initialize_result.document_loader->SimulateLoadData(8192);

  // Note: Plugin size chosen so all pages of the document are visible. The
  // engine only updates availability incrementally for visible pages.
  engine.PluginSizeUpdated({1024, 4096});

  const int num_pages = engine.GetNumberOfPages();
  ASSERT_EQ(3, num_pages);
  const int available_pages = CountAvailablePages(engine);
  ASSERT_LT(0, available_pages);
  ASSERT_GT(num_pages, available_pages);

  // Initialize callbacks for first and last pages.
  base::MockCallback<SendThumbnailCallback> first_loaded;
  base::MockCallback<SendThumbnailCallback> last_loaded;

  // When the document is partially loaded, `SendThumbnailCallback` is only run
  // for the loaded page even though `RequestThumbnail()` gets called for both
  // pages.
  EXPECT_CALL(first_loaded, Run);
  engine.RequestThumbnail(/*page_index=*/0, /*device_pixel_ratio=*/1,
                          first_loaded.Get());
  engine.RequestThumbnail(/*page_index=*/num_pages - 1,
                          /*device_pixel_ratio=*/1, last_loaded.Get());

  // Finish loading the document. `SendThumbnailCallback` should be run for the
  // last page.
  EXPECT_CALL(last_loaded, Run);
  initialize_result.FinishLoading();
}

TEST_F(PDFiumEngineTest, HandleInputEventKeyDown) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);
  EXPECT_CALL(client, DocumentFocusChanged(true));

  blink::WebKeyboardEvent key_down_event(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_down_event.windows_key_code = ui::VKEY_TAB;
  EXPECT_TRUE(engine->HandleInputEvent(key_down_event));
}

TEST_F(PDFiumEngineTest, HandleInputEventRawKeyDown) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);
  EXPECT_CALL(client, DocumentFocusChanged(true));

  blink::WebKeyboardEvent raw_key_down_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  raw_key_down_event.windows_key_code = ui::VKEY_TAB;
  EXPECT_TRUE(engine->HandleInputEvent(raw_key_down_event));
}

using PDFiumEngineDeathTest = PDFiumEngineTest;

TEST_F(PDFiumEngineDeathTest, RequestThumbnailRedundant) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPdfIncrementalLoading);

  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("linearized.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  PDFiumEngine& engine = *initialize_result.engine;

  // Load only some pages.
  initialize_result.document_loader->SimulateLoadData(8192);

  // Twice request a thumbnail for the second page, which is not loaded. The
  // second call should crash.
  base::MockCallback<SendThumbnailCallback> mock_callback;
  engine.RequestThumbnail(/*page_index=*/1, /*device_pixel_ratio=*/1,
                          mock_callback.Get());
  EXPECT_DCHECK_DEATH(engine.RequestThumbnail(
      /*page_index=*/1, /*device_pixel_ratio=*/1, mock_callback.Get()));
}

class PDFiumEngineTabbingTest : public PDFiumTestBase {
 public:
  PDFiumEngineTabbingTest() = default;
  ~PDFiumEngineTabbingTest() override = default;
  PDFiumEngineTabbingTest(const PDFiumEngineTabbingTest&) = delete;
  PDFiumEngineTabbingTest& operator=(const PDFiumEngineTabbingTest&) = delete;

  bool HandleTabEvent(PDFiumEngine* engine, int modifiers) {
    return engine->HandleTabEvent(modifiers);
  }

  PDFiumEngine::FocusElementType GetFocusedElementType(PDFiumEngine* engine) {
    return engine->focus_item_type_;
  }

  int GetLastFocusedPage(PDFiumEngine* engine) {
    return engine->last_focused_page_;
  }

  PDFiumEngine::FocusElementType GetLastFocusedElementType(
      PDFiumEngine* engine) {
    return engine->last_focused_item_type_;
  }

  int GetLastFocusedAnnotationIndex(PDFiumEngine* engine) {
    return engine->last_focused_annot_index_;
  }

  bool IsInFormTextArea(PDFiumEngine* engine) {
    return engine->in_form_text_area_;
  }

  size_t GetSelectionSize(PDFiumEngine* engine) {
    return engine->selection_.size();
  }

  void ScrollFocusedAnnotationIntoView(PDFiumEngine* engine) {
    engine->ScrollFocusedAnnotationIntoView();
  }
};

TEST_F(PDFiumEngineTabbingTest, LinkUnderCursorTest) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Widget annotation
   * ++++ Widget annotation
   * ++++ Highlight annotation
   * ++++ Link annotation
   */
  // Enable feature flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chrome_pdf::features::kTabAcrossPDFAnnotations);

  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("annots.pdf"));
  ASSERT_TRUE(engine);

  // Tab to right before the first non-link annotation.
  EXPECT_CALL(client, DocumentFocusChanged(true));
  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));

  // Tab through non-link annotations and validate link under cursor.
  {
    InSequence sequence;
    EXPECT_CALL(client, SetLinkUnderCursor(""));
    EXPECT_CALL(client, DocumentFocusChanged(false));
    EXPECT_CALL(client, SetLinkUnderCursor("")).Times(2);
  }

  for (int i = 0; i < 3; i++)
    ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));

  // Tab to Link annotation.
  EXPECT_CALL(client, SetLinkUnderCursor("https://www.google.com/"));
  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));

  // Tab to previous annotation.
  EXPECT_CALL(client, SetLinkUnderCursor(""));
  ASSERT_TRUE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
}

TEST_F(PDFiumEngineTabbingTest, TabbingSupportedAnnots) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Widget annotation
   * ++++ Widget annotation
   * ++++ Highlight annotation
   * ++++ Link annotation
   */

  // Enable feature flag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chrome_pdf::features::kTabAcrossPDFAnnotations);

  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("annots.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(1, engine->GetNumberOfPages());

  ASSERT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_FALSE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
}

TEST_F(PDFiumEngineTabbingTest, TabbingForwardTest) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Annotation
   * ++++ Annotation
   * ++ Page 2
   * ++++ Annotation
   */
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(2, engine->GetNumberOfPages());

  static constexpr bool kExpectedFocusState[] = {true, false};
  {
    InSequence sequence;
    for (auto focused : kExpectedFocusState)
      EXPECT_CALL(client, DocumentFocusChanged(focused));
  }

  ASSERT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(1, GetLastFocusedPage(engine.get()));

  ASSERT_FALSE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
}

TEST_F(PDFiumEngineTabbingTest, TabbingBackwardTest) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Annotation
   * ++++ Annotation
   * ++ Page 2
   * ++++ Annotation
   */
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(2, engine->GetNumberOfPages());

  static constexpr bool kExpectedFocusState[] = {true, false};
  {
    InSequence sequence;
    for (auto focused : kExpectedFocusState)
      EXPECT_CALL(client, DocumentFocusChanged(focused));
  }

  ASSERT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(1, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  ASSERT_FALSE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
}

TEST_F(PDFiumEngineTabbingTest, TabbingWithModifiers) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Annotation
   * ++++ Annotation
   * ++ Page 2
   * ++++ Annotation
   */
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(2, engine->GetNumberOfPages());

  ASSERT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Tabbing with ctrl modifier.
  ASSERT_FALSE(HandleTabEvent(engine.get(),
                              blink::WebInputEvent::Modifiers::kControlKey));
  // Tabbing with alt modifier.
  ASSERT_FALSE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kAltKey));

  // Tab to bring document into focus.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  // Tabbing with ctrl modifier.
  ASSERT_FALSE(HandleTabEvent(engine.get(),
                              blink::WebInputEvent::Modifiers::kControlKey));
  // Tabbing with alt modifier.
  ASSERT_FALSE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kAltKey));

  // Tab to bring first page into focus.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));

  // Tabbing with ctrl modifier.
  ASSERT_FALSE(HandleTabEvent(engine.get(),
                              blink::WebInputEvent::Modifiers::kControlKey));
  // Tabbing with alt modifier.
  ASSERT_FALSE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kAltKey));
}

TEST_F(PDFiumEngineTabbingTest, NoFocusableItemTabbingTest) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++ Page 2
   */
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(2, engine->GetNumberOfPages());

  static constexpr bool kExpectedFocusState[] = {true, false, true, false};
  {
    InSequence sequence;
    for (auto focused : kExpectedFocusState)
      EXPECT_CALL(client, DocumentFocusChanged(focused));
  }

  ASSERT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Tabbing forward.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  ASSERT_FALSE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));

  // Tabbing backward.
  ASSERT_TRUE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  ASSERT_FALSE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
}

TEST_F(PDFiumEngineTabbingTest, RestoringDocumentFocusTest) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Annotation
   * ++++ Annotation
   * ++ Page 2
   * ++++ Annotation
   */
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(2, engine->GetNumberOfPages());

  static constexpr bool kExpectedFocusState[] = {true, false, true};
  {
    InSequence sequence;
    for (auto focused : kExpectedFocusState)
      EXPECT_CALL(client, DocumentFocusChanged(focused));
  }

  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Tabbing to bring the document into focus.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  engine->UpdateFocus(/*has_focus=*/false);
  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetLastFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedAnnotationIndex(engine.get()));

  engine->UpdateFocus(/*has_focus=*/true);
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));
}

TEST_F(PDFiumEngineTabbingTest, RestoringAnnotFocusTest) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Annotation
   * ++++ Annotation
   * ++ Page 2
   * ++++ Annotation
   */
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(2, engine->GetNumberOfPages());

  static constexpr bool kExpectedFocusState[] = {true, false};
  {
    InSequence sequence;
    for (auto focused : kExpectedFocusState)
      EXPECT_CALL(client, DocumentFocusChanged(focused));
  }

  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Tabbing to bring last annotation of page 0 into focus.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));

  engine->UpdateFocus(/*has_focus=*/false);
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetLastFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedAnnotationIndex(engine.get()));

  engine->UpdateFocus(/*has_focus=*/true);
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  // Tabbing now should bring the second page's annotation to focus.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(1, GetLastFocusedPage(engine.get()));
}

TEST_F(PDFiumEngineTabbingTest, VerifyFormFieldStatesOnTabbing) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Annotation (Text Field)
   * ++++ Annotation (Radio Button)
   */
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("annots.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  // Bring focus to the text field.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));
  EXPECT_TRUE(IsInFormTextArea(engine.get()));
  EXPECT_TRUE(engine->CanEditText());

  // Bring focus to the button.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));
  EXPECT_FALSE(IsInFormTextArea(engine.get()));
  EXPECT_FALSE(engine->CanEditText());
}

TEST_F(PDFiumEngineTabbingTest, ClearSelectionOnFocusInFormTextArea) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("form_text_fields.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Select all text.
  engine->SelectAll();
  EXPECT_EQ(1u, GetSelectionSize(engine.get()));

  // Tab to bring focus to a form text area annotation.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));
  EXPECT_EQ(0u, GetSelectionSize(engine.get()));
}

TEST_F(PDFiumEngineTabbingTest, RetainSelectionOnFocusNotInFormTextArea) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("annots.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Select all text.
  engine->SelectAll();
  EXPECT_EQ(1u, GetSelectionSize(engine.get()));

  // Tab to bring focus to a non form text area annotation (Button).
  ASSERT_TRUE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));
  EXPECT_EQ(1u, GetSelectionSize(engine.get()));
}

class ScrollingTestClient : public TestClient {
 public:
  ScrollingTestClient() = default;
  ~ScrollingTestClient() override = default;
  ScrollingTestClient(const ScrollingTestClient&) = delete;
  ScrollingTestClient& operator=(const ScrollingTestClient&) = delete;

  // Mock PDFEngine::Client methods.
  MOCK_METHOD(void, ScrollToX, (int), (override));
  MOCK_METHOD(void, ScrollToY, (int), (override));
};

TEST_F(PDFiumEngineTabbingTest, MaintainViewportWhenFocusIsUpdated) {
  StrictMock<ScrollingTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(2, engine->GetNumberOfPages());
  engine->PluginSizeUpdated(gfx::Size(60, 40));

  {
    InSequence sequence;
    static constexpr gfx::Point kScrollValue = {510, 478};
    EXPECT_CALL(client, ScrollToY(kScrollValue.y()))
        .WillOnce(Invoke(
            [&engine]() { engine->ScrolledToYPosition(kScrollValue.y()); }));
    EXPECT_CALL(client, ScrollToX(kScrollValue.x()))
        .WillOnce(Invoke(
            [&engine]() { engine->ScrolledToXPosition(kScrollValue.x()); }));
  }

  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Tabbing to bring the document into focus.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  // Tab to an annotation.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));

  // Scroll focused annotation out of viewport.
  static constexpr gfx::Point kScrollPosition = {242, 746};
  engine->ScrolledToXPosition(kScrollPosition.x());
  engine->ScrolledToYPosition(kScrollPosition.y());

  engine->UpdateFocus(/*has_focus=*/false);
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetLastFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(1, GetLastFocusedAnnotationIndex(engine.get()));

  // Restore focus, we shouldn't have any calls to scroll viewport.
  engine->UpdateFocus(/*has_focus=*/true);
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));
}

TEST_F(PDFiumEngineTabbingTest, ScrollFocusedAnnotationIntoView) {
  StrictMock<ScrollingTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(2, engine->GetNumberOfPages());
  engine->PluginSizeUpdated(gfx::Size(60, 40));

  {
    InSequence sequence;
    static constexpr gfx::Point kScrollValues[] = {{510, 478}, {510, 478}};

    for (const auto& scroll_value : kScrollValues) {
      EXPECT_CALL(client, ScrollToY(scroll_value.y()))
          .WillOnce(Invoke([&engine, &scroll_value]() {
            engine->ScrolledToYPosition(scroll_value.y());
          }));
      EXPECT_CALL(client, ScrollToX(scroll_value.x()))
          .WillOnce(Invoke([&engine, &scroll_value]() {
            engine->ScrolledToXPosition(scroll_value.x());
          }));
    }
  }

  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Tabbing to bring the document into focus.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  // Tab to an annotation.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));

  // Scroll focused annotation out of viewport.
  static constexpr gfx::Point kScrollPosition = {242, 746};
  engine->ScrolledToXPosition(kScrollPosition.x());
  engine->ScrolledToYPosition(kScrollPosition.y());

  // Scroll the focused annotation into view.
  ScrollFocusedAnnotationIntoView(engine.get());
}

class ReadOnlyTestClient : public TestClient {
 public:
  ReadOnlyTestClient() = default;
  ~ReadOnlyTestClient() override = default;
  ReadOnlyTestClient(const ReadOnlyTestClient&) = delete;
  ReadOnlyTestClient& operator=(const ReadOnlyTestClient&) = delete;

  // Mock PDFEngine::Client methods.
  MOCK_METHOD(void, FormTextFieldFocusChange, (bool), (override));
  MOCK_METHOD(void, SetSelectedText, (const std::string&), (override));
};

using PDFiumEngineReadOnlyTest = PDFiumTestBase;

TEST_F(PDFiumEngineReadOnlyTest, KillFormFocus) {
  NiceMock<ReadOnlyTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  // Setting read-only mode should kill form focus.
  EXPECT_FALSE(engine->IsReadOnly());
  EXPECT_CALL(client, FormTextFieldFocusChange(false));
  engine->SetReadOnly(true);

  // Attempting to focus during read-only mode should once more trigger a
  // killing of form focus.
  EXPECT_TRUE(engine->IsReadOnly());
  EXPECT_CALL(client, FormTextFieldFocusChange(false));
  engine->UpdateFocus(true);
}

TEST_F(PDFiumEngineReadOnlyTest, UnselectText) {
  NiceMock<ReadOnlyTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Update the plugin size so that all the text is visible by
  // `SelectionChangeInvalidator`.
  engine->PluginSizeUpdated({500, 500});

  // Select text before going into read-only mode.
  EXPECT_FALSE(engine->IsReadOnly());
  EXPECT_CALL(client, SetSelectedText(Not(IsEmpty())));
  engine->SelectAll();

  // Setting read-only mode should unselect the text.
  EXPECT_CALL(client, SetSelectedText(IsEmpty()));
  engine->SetReadOnly(true);
}

}  // namespace chrome_pdf
