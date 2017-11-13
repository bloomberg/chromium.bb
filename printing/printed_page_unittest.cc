// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_page.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(PrintedPageTest, GetCenteredPageContentRect) {
  scoped_refptr<PrintedPage> page;
  gfx::Rect page_content;

  // No centering.
  page = base::MakeRefCounted<PrintedPage>(1, std::unique_ptr<MetafilePlayer>(),
                                           gfx::Size(1200, 1200),
                                           gfx::Rect(0, 0, 400, 1100));
  page_content = page->GetCenteredPageContentRect(gfx::Size(1000, 1000));
  EXPECT_EQ(0, page_content.x());
  EXPECT_EQ(0, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());

  // X centered.
  page = base::MakeRefCounted<PrintedPage>(1, std::unique_ptr<MetafilePlayer>(),
                                           gfx::Size(500, 1200),
                                           gfx::Rect(0, 0, 400, 1100));
  page_content = page->GetCenteredPageContentRect(gfx::Size(1000, 1000));
  EXPECT_EQ(250, page_content.x());
  EXPECT_EQ(0, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());

  // Y centered.
  page = base::MakeRefCounted<PrintedPage>(1, std::unique_ptr<MetafilePlayer>(),
                                           gfx::Size(1200, 500),
                                           gfx::Rect(0, 0, 400, 1100));
  page_content = page->GetCenteredPageContentRect(gfx::Size(1000, 1000));
  EXPECT_EQ(0, page_content.x());
  EXPECT_EQ(250, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());

  // Both X and Y centered.
  page = base::MakeRefCounted<PrintedPage>(1, std::unique_ptr<MetafilePlayer>(),
                                           gfx::Size(500, 500),
                                           gfx::Rect(0, 0, 400, 1100));
  page_content = page->GetCenteredPageContentRect(gfx::Size(1000, 1000));
  EXPECT_EQ(250, page_content.x());
  EXPECT_EQ(250, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());
}

#if defined(OS_WIN)
TEST(PrintedPageTest, Shrink) {
  scoped_refptr<PrintedPage> page = base::MakeRefCounted<PrintedPage>(
      1, std::unique_ptr<MetafilePlayer>(), gfx::Size(1200, 1200),
      gfx::Rect(0, 0, 400, 1100));
  EXPECT_EQ(0.0f, page->shrink_factor());
  page->set_shrink_factor(0.2f);
  EXPECT_EQ(0.2f, page->shrink_factor());
  page->set_shrink_factor(0.7f);
  EXPECT_EQ(0.7f, page->shrink_factor());
}
#endif

}  // namespace printing
