// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_document.h"

#include "pdf/pdfium/pdfium_engine.h"

namespace chrome_pdf {

PDFiumDocument::PDFiumDocument(DocumentLoader* doc_loader)
    : doc_loader_(doc_loader) {
  file_access_.m_FileLen = 0;
  file_access_.m_GetBlock = &GetBlock;
  file_access_.m_Param = doc_loader_;

  file_availability_.version = 1;
  file_availability_.IsDataAvail = &IsDataAvail;
  file_availability_.doc_loader = doc_loader_;

  download_hints_.version = 1;
  download_hints_.AddSegment = &AddSegment;
  download_hints_.doc_loader = doc_loader_;
}

PDFiumDocument::~PDFiumDocument() = default;

void PDFiumDocument::CreateFPDFAvailability() {
  fpdf_availability_.reset(
      FPDFAvail_Create(&file_availability_, &file_access_));
}

void PDFiumDocument::ResetFPDFAvailability() {
  fpdf_availability_.reset();
}

void PDFiumDocument::LoadDocument(const char* password) {
  if (doc_loader_->IsDocumentComplete() &&
      !FPDFAvail_IsLinearized(fpdf_availability())) {
    doc_.reset(FPDF_LoadCustomDocument(&file_access_, password));
  } else {
    doc_.reset(FPDFAvail_GetDocument(fpdf_availability(), password));
  }
}

void PDFiumDocument::SetFormStatus() {
  form_status_ = FPDFAvail_IsFormAvail(fpdf_availability(), &download_hints_);
}

void PDFiumDocument::InitializeForm(FPDF_FORMFILLINFO* form_info) {
  form_.reset(FPDFDOC_InitFormFillEnvironment(doc(), form_info));
}

// static
int PDFiumDocument::GetBlock(void* param,
                             unsigned long position,
                             unsigned char* buffer,
                             unsigned long size) {
  auto* doc_loader = static_cast<DocumentLoader*>(param);
  return doc_loader->GetBlock(position, size, buffer);
}

// static
FPDF_BOOL PDFiumDocument::IsDataAvail(FX_FILEAVAIL* param,
                                      size_t offset,
                                      size_t size) {
  auto* file_avail = static_cast<FileAvail*>(param);
  return file_avail->doc_loader->IsDataAvailable(offset, size);
}

// static
void PDFiumDocument::AddSegment(FX_DOWNLOADHINTS* param,
                                size_t offset,
                                size_t size) {
  auto* download_hints = static_cast<DownloadHints*>(param);
  return download_hints->doc_loader->RequestData(offset, size);
}

}  // namespace chrome_pdf
