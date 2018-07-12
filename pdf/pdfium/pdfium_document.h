// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_DOCUMENT_H_
#define PDF_PDFIUM_PDFIUM_DOCUMENT_H_

#include "base/macros.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_dataavail.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace chrome_pdf {

class DocumentLoader;

class PDFiumDocument {
 public:
  explicit PDFiumDocument(DocumentLoader* doc_loader);
  ~PDFiumDocument();

  FPDF_FILEACCESS& file_access() { return file_access_; }
  FX_FILEAVAIL& file_availability() { return file_availability_; }
  FX_DOWNLOADHINTS& download_hints() { return download_hints_; }

  FPDF_AVAIL fpdf_availability() const { return fpdf_availability_.get(); }
  FPDF_DOCUMENT doc() const { return doc_.get(); }
  FPDF_FORMHANDLE form() const { return form_.get(); }

  int form_status() const { return form_status_; }

  void CreateFPDFAvailability();
  void ResetFPDFAvailability();

  void LoadDocument(const char* password);

  void SetFormStatus();
  void InitializeForm(FPDF_FORMFILLINFO* form_info);

 private:
  struct FileAvail : public FX_FILEAVAIL {
    DocumentLoader* doc_loader;
  };

  struct DownloadHints : public FX_DOWNLOADHINTS {
    DocumentLoader* doc_loader;
  };

  // PDFium interface to get block of data.
  static int GetBlock(void* param,
                      unsigned long position,
                      unsigned char* buffer,
                      unsigned long size);

  // PDFium interface to check is block of data is available.
  static FPDF_BOOL IsDataAvail(FX_FILEAVAIL* param, size_t offset, size_t size);

  // PDFium interface to request download of the block of data.
  static void AddSegment(FX_DOWNLOADHINTS* param, size_t offset, size_t size);

  DocumentLoader* const doc_loader_;

  // Interface structure to provide access to document stream.
  FPDF_FILEACCESS file_access_;

  // Interface structure to check data availability in the document stream.
  FileAvail file_availability_;

  // Interface structure to request data chunks from the document stream.
  DownloadHints download_hints_;

  // Pointer to the document availability interface.
  ScopedFPDFAvail fpdf_availability_;

  // The PDFium wrapper object for the document. Must come after
  // |fpdf_availability_| to prevent outliving it.
  ScopedFPDFDocument doc_;

  // The PDFium wrapper for form data.  Used even if there are no form controls
  // on the page. Must come after |doc_| to prevent outliving it.
  ScopedFPDFFormHandle form_;

  // Current form availability status.
  int form_status_ = PDF_FORM_NOTAVAIL;

  DISALLOW_COPY_AND_ASSIGN(PDFiumDocument);
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_DOCUMENT_H_
