// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_ps_metafile_cairo.h"

#include <fcntl.h>
#include <string>
#include <vector>

#include "base/file_descriptor_posix.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef struct _cairo cairo_t;

class PdfPsTest : public testing::Test {
 protected:
  base::FileDescriptor DevNullFD() {
    return base::FileDescriptor(open("/dev/null", O_WRONLY), true);
  }
};

TEST_F(PdfPsTest, Pdf) {
  // Tests in-renderer constructor.
  printing::PdfPsMetafile pdf;
  EXPECT_TRUE(pdf.Init());

  // Renders page 1.
  cairo_t* context = pdf.StartPage(72, 72, 1, 2, 3, 4);
  EXPECT_TRUE(context != NULL);
  EXPECT_EQ(printing::PdfPsMetafile::FromCairoContext(context), &pdf);
  // In theory, we should use Cairo to draw something on |context|.
  EXPECT_TRUE(pdf.FinishPage());

  // Renders page 2.
  context = pdf.StartPage(64, 64, 1, 2, 3, 4);
  EXPECT_TRUE(context != NULL);
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
  EXPECT_TRUE(pdf2.Init(&buffer.front(), size));

  // Tries to get the first 4 characters from pdf2.
  std::vector<char> buffer2(4, 0x00);
  pdf2.GetData(&buffer2.front(), 4);

  // Tests if the header begins with "%PDF".
  std::string header(&buffer2.front(), 4);
  EXPECT_EQ(header.find("%PDF", 0), 0u);

  // Tests if we can save data.
  EXPECT_TRUE(pdf.SaveTo(DevNullFD()));

  // Test overriding the metafile with raw data.
  printing::PdfPsMetafile pdf3;
  EXPECT_TRUE(pdf3.Init());
  context = pdf3.StartPage(72, 72, 1, 2, 3, 4);
  EXPECT_TRUE(context != NULL);
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
