// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_metafile_mac.h"

#import <ApplicationServices/ApplicationServices.h>

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"

TEST(PdfMetafileTest, Pdf) {
  // Test in-renderer constructor.
  printing::PdfMetafile pdf;
  EXPECT_TRUE(pdf.Init() != NULL);

  // Render page 1 at origin (10.0, 10.0).
  gfx::Point origin_1(10.0f, 10.0f);
  gfx::Size size_1(540, 720);
  pdf.StartPage(size_1, origin_1, 1.25);
  pdf.FinishPage();

  // Render page 2 at origin (10.0, 10.0).
  gfx::Point origin_2(10.0f, 10.0f);
  gfx::Size size_2(720, 540);
  pdf.StartPage(size_2, origin_2, 2.0);
  pdf.FinishPage();

  pdf.Close();

  // Check data size.
  uint32 size = pdf.GetDataSize();
  EXPECT_GT(size, 0U);

  // Get resulting data.
  std::vector<char> buffer(size, 0);
  pdf.GetData(&buffer.front(), size);

  // Test browser-side constructor.
  printing::PdfMetafile pdf2;
  EXPECT_TRUE(pdf2.Init(&buffer.front(), size));

  // Get the first 4 characters from pdf2.
  std::vector<char> buffer2(4, 0);
  pdf2.GetData(&buffer2.front(), 4);

  // Test that the header begins with "%PDF".
  std::string header(&buffer2.front(), 4);
  EXPECT_EQ(0U, header.find("%PDF", 0));

  // Test that the PDF is correctly reconstructed.
  EXPECT_EQ(2U, pdf2.GetPageCount());
  gfx::Size page_size = pdf2.GetPageBounds(1).size();
  EXPECT_EQ(540, page_size.width());
  EXPECT_EQ(720, page_size.height());
  page_size = pdf2.GetPageBounds(2).size();
  EXPECT_EQ(720, page_size.width());
  EXPECT_EQ(540, page_size.height());
}
