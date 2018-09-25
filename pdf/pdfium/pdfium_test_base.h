// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_TEST_BASE_H_
#define PDF_PDFIUM_PDFIUM_TEST_BASE_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

class PDFiumTestBase : public testing::Test {
 public:
  PDFiumTestBase();
  ~PDFiumTestBase() override;

 protected:
  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Sets the PDF to load for a test. This must be called for tests that use
  // TestDocumentLoader. See TestDocumentLoader for more info.
  void SetDocumentForTest(const base::FilePath::CharType* pdf_name);

 private:
  void InitializePDFium();

  DISALLOW_COPY_AND_ASSIGN(PDFiumTestBase);
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_TEST_BASE_H_
