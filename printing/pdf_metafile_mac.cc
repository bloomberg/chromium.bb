// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_metafile_mac.h"

#include "base/logging.h"

namespace printing {

PdfMetafile::PdfMetafile()
    : page_is_open_(false) {
}

CGContextRef PdfMetafile::Init() {
  // Ensure that Init hasn't already been called.
  DCHECK(!context_.get());
  DCHECK(!pdf_data_.get());

  pdf_data_.reset(CFDataCreateMutable(kCFAllocatorDefault, 0));
  if (!pdf_data_.get()) {
    LOG(ERROR) << "Failed to create pdf data for metafile";
    return NULL;
  }
  scoped_cftyperef<CGDataConsumerRef> pdf_consumer(
      CGDataConsumerCreateWithCFData(pdf_data_));
  if (!pdf_consumer.get()) {
    LOG(ERROR) << "Failed to create data consumer for metafile";
    pdf_data_.reset(NULL);
    return NULL;
  }
  context_.reset(CGPDFContextCreate(pdf_consumer, NULL, NULL));
  if (!context_.get()) {
    LOG(ERROR) << "Failed to create pdf context for metafile";
    pdf_data_.reset(NULL);
  }

  return context_.get();
}

bool PdfMetafile::Init(const void* src_buffer, size_t src_buffer_size) {
  DCHECK(!context_.get());
  DCHECK(!pdf_data_.get());

  if (!src_buffer || src_buffer_size == 0) {
    return false;
  }

  pdf_data_.reset(CFDataCreateMutable(kCFAllocatorDefault, src_buffer_size));
  CFDataAppendBytes(pdf_data_, static_cast<const UInt8*>(src_buffer),
                    src_buffer_size);

  return true;
}

bool PdfMetafile::CreateFromData(const void* src_buffer,
                                 size_t src_buffer_size) {
  return Init(src_buffer, src_buffer_size);
}

void PdfMetafile::StartPage(double width, double height, double scale_factor) {
  DCHECK(context_.get());
  DCHECK(!page_is_open_);

  CGRect bounds = CGRectMake(0, 0, width, height);
  CGContextBeginPage(context_, &bounds);
  page_is_open_ = true;
  CGContextSaveGState(context_);

  // Flip the context.
  CGContextTranslateCTM(context_, 0, height);
  CGContextScaleCTM(context_, scale_factor, -scale_factor);
}

void PdfMetafile::FinishPage() {
  DCHECK(context_.get());
  DCHECK(page_is_open_);

  CGContextRestoreGState(context_);
  CGContextEndPage(context_);
  page_is_open_ = false;
}

void PdfMetafile::Close() {
  DCHECK(context_.get());
  DCHECK(!page_is_open_);

  context_.reset(NULL);
}

unsigned int PdfMetafile::GetDataSize() const {
  // PDF data is only valid/complete once the context is released.
  DCHECK(!context_);

  if (!pdf_data_)
    return 0;
  return static_cast<unsigned int>(CFDataGetLength(pdf_data_));
}

bool PdfMetafile::GetData(void* dst_buffer, size_t dst_buffer_size) const {
  // PDF data is only valid/complete once the context is released.
  DCHECK(!context_);
  DCHECK(pdf_data_);
  DCHECK(dst_buffer);
  DCHECK_GT(dst_buffer_size, 0U);

  size_t data_size = GetDataSize();
  if (dst_buffer_size > data_size) {
    return false;
  }

  CFDataGetBytes(pdf_data_, CFRangeMake(0, dst_buffer_size),
                 static_cast<UInt8*>(dst_buffer));
  return true;
}

}  // namespace printing
