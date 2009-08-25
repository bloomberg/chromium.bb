// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_ps_metafile_linux.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef struct _cairo cairo_t;

TEST(PdfTest, Basic) {
  // Tests in-renderer constructor.
  printing::PdfPsMetafile pdf(printing::PdfPsMetafile::PDF);
  cairo_t* context = pdf.GetPageContext();
  EXPECT_TRUE(context == NULL);

  // Renders page 1.
  EXPECT_TRUE(pdf.StartPage(72, 72));
  context = pdf.GetPageContext();
  EXPECT_TRUE(context != NULL);
  // In theory, we should use Cairo to draw something on |context|.
  pdf.FinishPage(1.5);
  context = pdf.GetPageContext();
  EXPECT_TRUE(context == NULL);

  // Renders page 2.
  EXPECT_TRUE(pdf.StartPage(64, 64));
  context = pdf.GetPageContext();
  EXPECT_TRUE(context != NULL);
  // In theory, we should use Cairo to draw something on |context|.
  pdf.FinishPage(0.5);
  context = pdf.GetPageContext();
  EXPECT_TRUE(context == NULL);

  // Closes the file.
  pdf.Close();
  context = pdf.GetPageContext();
  EXPECT_TRUE(context == NULL);

  // Checks data size.
  unsigned int size = pdf.GetDataSize();
  EXPECT_GT(size, 0u);

  // Gets resulting data.
  std::vector<char> buffer(size, 0x00);
  pdf.GetData(&buffer.front(), size);

  // Tests another constructor.
  printing::PdfPsMetafile pdf2(printing::PdfPsMetafile::PDF,
                               &buffer.front(),
                               size);

  // Tries to get the first 4 characters from pdf2.
  std::vector<char> buffer2(4, 0x00);
  pdf2.GetData(&buffer2.front(), 4);

  // Tests if the header begins with "%PDF".
  std::string header(&buffer2.front(), 4);
  EXPECT_EQ(header.find("%PDF", 0), 0u);

  // Tests if we can save data.
  EXPECT_TRUE(pdf.SaveTo(FilePath("/dev/null")));
}

TEST(PsTest, Basic) {
  // Tests in-renderer constructor.
  printing::PdfPsMetafile ps(printing::PdfPsMetafile::PS);
  cairo_t* context = ps.GetPageContext();
  EXPECT_TRUE(context == NULL);

  // Renders page 1.
  EXPECT_TRUE(ps.StartPage(72, 72));
  context = ps.GetPageContext();
  EXPECT_TRUE(context != NULL);
  // In theory, we should use Cairo to draw something on |context|.
  ps.FinishPage(1.5);
  context = ps.GetPageContext();
  EXPECT_TRUE(context == NULL);

  // Renders page 2.
  EXPECT_TRUE(ps.StartPage(64, 64));
  context = ps.GetPageContext();
  EXPECT_TRUE(context != NULL);
  // In theory, we should use Cairo to draw something on |context|.
  ps.FinishPage(0.5);
  context = ps.GetPageContext();
  EXPECT_TRUE(context == NULL);

  // Closes the file.
  ps.Close();
  context = ps.GetPageContext();
  EXPECT_TRUE(context == NULL);

  // Checks data size.
  unsigned int size = ps.GetDataSize();
  EXPECT_GT(size, 0u);

  // Gets resulting data.
  std::vector<char> buffer(size, 0x00);
  ps.GetData(&buffer.front(), size);

  // Tests another constructor.
  printing::PdfPsMetafile ps2(printing::PdfPsMetafile::PS,
                              &buffer.front(),
                              size);

  // Tries to get the first 4 characters from ps2.
  std::vector<char> buffer2(4, 0x00);
  ps2.GetData(&buffer2.front(), 4);

  // Tests if the header begins with "%!PS".
  std::string header(&buffer2.front(), 4);
  EXPECT_EQ(header.find("%!PS", 0), 0u);

  // Tests if we can save data.
  EXPECT_TRUE(ps.SaveTo(FilePath("/dev/null")));
}

