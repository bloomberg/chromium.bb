// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_ps_metafile_cairo.h"

#include <fcntl.h>
#include <string>
#include <vector>

#include "base/file_descriptor_posix.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"
#include "ui/gfx/point.h"

typedef struct _cairo cairo_t;

namespace {

class PdfPsTest : public testing::Test {};

}  // namespace

namespace printing {

TEST_F(PdfPsTest, Pdf) {
  // Tests in-renderer constructor.
  printing::PdfPsMetafile pdf;
  EXPECT_TRUE(pdf.Init());

  // Renders page 1.
  EXPECT_TRUE(pdf.StartPage(gfx::Size(72, 73), gfx::Point(4, 5), 1));
  EXPECT_EQ(printing::PdfPsMetafile::FromCairoContext(pdf.context()), &pdf);
  // In theory, we should use Cairo to draw something on |context|.
  EXPECT_TRUE(pdf.FinishPage());

  // Renders page 2.
  EXPECT_TRUE(pdf.StartPage(gfx::Size(72, 73), gfx::Point(4, 5), 1));
  // In theory, we should use Cairo to draw something on |context|.
  EXPECT_TRUE(pdf.FinishPage());

  // Closes the file.
  pdf.Close();

  // Checks data size.
  uint32 size = pdf.GetDataSize();
  EXPECT_GT(size, 0u);

  // Gets resulting data.
  std::vector<char> buffer(size, 0x00);
  pdf.GetData(&buffer.front(), size);

  // Tests another constructor.
  printing::PdfPsMetafile pdf2;
  EXPECT_TRUE(pdf2.InitFromData(&buffer.front(), size));

  // Tries to get the first 4 characters from pdf2.
  std::vector<char> buffer2(4, 0x00);
  pdf2.GetData(&buffer2.front(), 4);

  // Tests if the header begins with "%PDF".
  std::string header(&buffer2.front(), 4);
  EXPECT_EQ(header.find("%PDF", 0), 0u);

  // Tests if we can save data.
  EXPECT_TRUE(pdf.SaveTo(FilePath("/dev/null")));

  // Test overriding the metafile with raw data.
  printing::PdfPsMetafile pdf3;
  EXPECT_TRUE(pdf3.Init());
  EXPECT_TRUE(pdf3.StartPage(gfx::Size(72, 73), gfx::Point(4, 5), 1));
  std::string test_raw_data = "Dummy PDF";
  EXPECT_TRUE(pdf3.SetRawData(test_raw_data.c_str(), test_raw_data.size()));
  EXPECT_TRUE(pdf3.FinishPage());
  pdf3.Close();
  size = pdf3.GetDataSize();
  EXPECT_EQ(test_raw_data.size(), size);
  std::string output;
  pdf3.GetData(WriteInto(&output, size + 1), size);
  EXPECT_EQ(test_raw_data, output);
}

}  // namespace printing
