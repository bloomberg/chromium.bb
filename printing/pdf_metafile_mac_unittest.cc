// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_metafile_mac.h"

#import <ApplicationServices/ApplicationServices.h>

#include <string>
#include <vector>

#include "base/gfx/rect.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PdfMetafileTest, Pdf) {
  // Test in-renderer constructor.
  printing::PdfMetafile pdf;
  CGContextRef context = pdf.Init();
  EXPECT_TRUE(context != NULL);

  // Render page 1.
  pdf.StartPage(540, 720, 1.25);
  pdf.FinishPage();

  // Render page 2.
  pdf.StartPage(720, 540, 2.0);
  pdf.FinishPage();

  pdf.Close();

  // Check data size.
  unsigned int size = pdf.GetDataSize();
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
