// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_page.h"

#include "base/logging.h"
#include "base/test/gtest_util.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_utils.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "base/system/sys_info.h"
#endif

namespace chrome_pdf {

namespace {

TEST(PDFiumPageTest, ToPDFiumRotation) {
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kOriginal), 0);
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kClockwise90), 1);
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kClockwise180), 2);
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kClockwise270), 3);
}

TEST(PDFiumPageDeathTest, ToPDFiumRotation) {
  PageOrientation invalid_orientation = static_cast<PageOrientation>(-1);
#if DCHECK_IS_ON()
  EXPECT_DCHECK_DEATH(ToPDFiumRotation(invalid_orientation));
#else
  EXPECT_EQ(ToPDFiumRotation(invalid_orientation), 0);
#endif
}

}  // namespace

class PDFiumPageLinkTest : public PDFiumTestBase {
 public:
  PDFiumPageLinkTest() = default;
  ~PDFiumPageLinkTest() override = default;

  const std::vector<PDFiumPage::Link>& GetLinks(PDFiumEngine* engine,
                                                int page_index) {
    PDFiumPage* page = GetPDFiumPageForTest(engine, page_index);
    DCHECK(page);
    page->CalculateLinks();
    return page->links_;
  }

  DISALLOW_COPY_AND_ASSIGN(PDFiumPageLinkTest);
};

TEST_F(PDFiumPageLinkTest, TestLinkGeneration) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("weblinks.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  bool is_chromeos = false;
#if defined(OS_CHROMEOS)
  is_chromeos = base::SysInfo::IsRunningOnChromeOS();
#endif

  const std::vector<PDFiumPage::Link>& links = GetLinks(engine.get(), 0);
  ASSERT_EQ(3u, links.size());

  const PDFiumPage::Link& link = links[0];
  EXPECT_EQ("http://yahoo.com", link.url);
  EXPECT_EQ(7, link.start_char_index);
  EXPECT_EQ(16, link.char_count);
  ASSERT_EQ(1u, link.bounding_rects.size());
  if (is_chromeos) {
    CompareRect({75, 192, 110, 15}, link.bounding_rects[0]);
  } else {
    CompareRect({75, 191, 110, 16}, link.bounding_rects[0]);
  }

  const PDFiumPage::Link& second_link = links[1];
  EXPECT_EQ("http://bing.com", second_link.url);
  EXPECT_EQ(52, second_link.start_char_index);
  EXPECT_EQ(15, second_link.char_count);
  ASSERT_EQ(1u, second_link.bounding_rects.size());
  if (is_chromeos) {
    CompareRect({131, 120, 138, 22}, second_link.bounding_rects[0]);
  } else {
    CompareRect({131, 121, 138, 20}, second_link.bounding_rects[0]);
  }

  const PDFiumPage::Link& third_link = links[2];
  EXPECT_EQ("http://google.com", third_link.url);
  EXPECT_EQ(92, third_link.start_char_index);
  EXPECT_EQ(17, third_link.char_count);
  ASSERT_EQ(1u, third_link.bounding_rects.size());
  CompareRect({82, 67, 161, 21}, third_link.bounding_rects[0]);
}

using PDFiumPageImageTest = PDFiumTestBase;

TEST_F(PDFiumPageImageTest, TestCalculateImages) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("image_alt_text.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  PDFiumPage* page = GetPDFiumPageForTest(engine.get(), 0);
  ASSERT_TRUE(page);
  page->CalculateImages();
  ASSERT_EQ(3u, page->images_.size());
  CompareRect({380, 78, 67, 68}, page->images_[0].bounding_rect);
  CompareRect({380, 385, 27, 28}, page->images_[1].bounding_rect);
  CompareRect({380, 678, 1, 1}, page->images_[2].bounding_rect);
}

using PDFiumPageTextTest = PDFiumTestBase;

TEST_F(PDFiumPageTextTest, GetTextRunInfo) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("weblinks.pdf"));
  ASSERT_TRUE(engine);

  int current_char_index = 0;
  uint32_t text_run_length;
  double text_run_font_size;
  pp::FloatRect bounds;

  // The links span from [7, 22], [52, 66] and [92, 108] with 16, 15 and 17
  // text run lengths respectively. There are text runs preceding and
  // succeeding them.
  static constexpr uint32_t kExpectedTextRunLengths[] = {7,  16, 20, 9, 15,
                                                         20, 5,  17, 0};
  for (uint32_t expected_length : kExpectedTextRunLengths) {
    engine->GetTextRunInfo(0, current_char_index, &text_run_length,
                           &text_run_font_size, &bounds);
    EXPECT_EQ(expected_length, text_run_length);
    current_char_index += text_run_length;
  }
}

}  // namespace chrome_pdf
