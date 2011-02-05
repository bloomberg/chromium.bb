// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_metafile_mac.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "ui/gfx/rect.h"

using base::mac::ScopedCFTypeRef;

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
  ScopedCFTypeRef<CGDataConsumerRef> pdf_consumer(
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

bool PdfMetafile::Init(const void* src_buffer, uint32 src_buffer_size) {
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

CGContextRef PdfMetafile::StartPage(const gfx::Size& page_size,
    const gfx::Point& content_origin, const float& scale_factor) {
  DCHECK(context_.get());
  DCHECK(!page_is_open_);

  double height = page_size.height();
  double width = page_size.width();

  CGRect bounds = CGRectMake(0, 0, width, height);
  CGContextBeginPage(context_, &bounds);
  page_is_open_ = true;
  CGContextSaveGState(context_);

  // Flip the context.
  CGContextTranslateCTM(context_, 0, height);
  CGContextScaleCTM(context_, scale_factor, -scale_factor);

  // Move the context to origin.
  CGContextTranslateCTM(context_, content_origin.x(), content_origin.y());

  return context_.get();
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

#ifndef NDEBUG
  // Check that the context will be torn down properly; if it's not, pdf_data_
  // will be incomplete and generate invalid PDF files/documents.
  if (context_.get()) {
    CFIndex extra_retain_count = CFGetRetainCount(context_.get()) - 1;
    if (extra_retain_count > 0) {
      LOG(ERROR) << "Metafile context has " << extra_retain_count
                 << " extra retain(s) on Close";
    }
  }
#endif
  CGPDFContextClose(context_.get());
  context_.reset(NULL);
}

bool PdfMetafile::RenderPage(unsigned int page_number, CGContextRef context,
                             const CGRect rect, bool shrink_to_fit,
                             bool stretch_to_fit,
                             bool center_horizontally,
                             bool center_vertically) const {
  CGPDFDocumentRef pdf_doc = GetPDFDocument();
  if (!pdf_doc) {
    LOG(ERROR) << "Unable to create PDF document from data";
    return false;
  }
  CGPDFPageRef pdf_page = CGPDFDocumentGetPage(pdf_doc, page_number);
  CGRect source_rect = CGPDFPageGetBoxRect(pdf_page, kCGPDFMediaBox);
  float scaling_factor = 1.0;
  // See if we need to scale the output.
  bool scaling_needed =
      (shrink_to_fit && ((source_rect.size.width > rect.size.width) ||
                         (source_rect.size.height > rect.size.height))) ||
      (stretch_to_fit && (source_rect.size.width < rect.size.width) &&
          (source_rect.size.height < rect.size.height));
  if (scaling_needed) {
    float x_scaling_factor = rect.size.width / source_rect.size.width;
    float y_scaling_factor = rect.size.height / source_rect.size.height;
    if (x_scaling_factor > y_scaling_factor) {
      scaling_factor = y_scaling_factor;
    } else {
      scaling_factor = x_scaling_factor;
    }
  }
  // Some PDFs have a non-zero origin. Need to take that into account.
  float x_offset = rect.origin.x - (source_rect.origin.x * scaling_factor);
  float y_offset = rect.origin.y - (source_rect.origin.y * scaling_factor);

  if (center_vertically) {
    x_offset += (rect.size.width -
                     (source_rect.size.width * scaling_factor))/2;
  }
  if (center_horizontally) {
    y_offset += (rect.size.height -
                     (source_rect.size.height * scaling_factor))/2;
  } else {
    // Since 0 y begins at the bottom, we need to adjust so the output appears
    // nearer the top if we are not centering horizontally.
    y_offset += rect.size.height - (source_rect.size.height * scaling_factor);
  }
  CGContextSaveGState(context);
  CGContextTranslateCTM(context, x_offset, y_offset);
  CGContextScaleCTM(context, scaling_factor, scaling_factor);
  CGContextDrawPDFPage(context, pdf_page);
  CGContextRestoreGState(context);
  return true;
}

unsigned int PdfMetafile::GetPageCount() const {
  CGPDFDocumentRef pdf_doc = GetPDFDocument();
  return pdf_doc ? CGPDFDocumentGetNumberOfPages(pdf_doc) : 0;
}

gfx::Rect PdfMetafile::GetPageBounds(unsigned int page_number) const {
  CGPDFDocumentRef pdf_doc = GetPDFDocument();
  if (!pdf_doc) {
    LOG(ERROR) << "Unable to create PDF document from data";
    return gfx::Rect();
  }
  if (page_number > GetPageCount()) {
    LOG(ERROR) << "Invalid page number: " << page_number;
    return gfx::Rect();
  }
  CGPDFPageRef pdf_page = CGPDFDocumentGetPage(pdf_doc, page_number);
  CGRect page_rect = CGPDFPageGetBoxRect(pdf_page, kCGPDFMediaBox);
  return gfx::Rect(page_rect);
}

uint32 PdfMetafile::GetDataSize() const {
  // PDF data is only valid/complete once the context is released.
  DCHECK(!context_);

  if (!pdf_data_)
    return 0;
  return static_cast<uint32>(CFDataGetLength(pdf_data_));
}

bool PdfMetafile::GetData(void* dst_buffer, uint32 dst_buffer_size) const {
  // PDF data is only valid/complete once the context is released.
  DCHECK(!context_);
  DCHECK(pdf_data_);
  DCHECK(dst_buffer);
  DCHECK_GT(dst_buffer_size, 0U);

  uint32 data_size = GetDataSize();
  if (dst_buffer_size > data_size) {
    return false;
  }

  CFDataGetBytes(pdf_data_, CFRangeMake(0, dst_buffer_size),
                 static_cast<UInt8*>(dst_buffer));
  return true;
}

bool PdfMetafile::SaveTo(const FilePath& file_path) const {
  DCHECK(pdf_data_.get());
  DCHECK(!context_.get());

  std::string path_string = file_path.value();
  ScopedCFTypeRef<CFURLRef> path_url(CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(path_string.c_str()),
      path_string.length(), false));
  SInt32 error_code;
  CFURLWriteDataAndPropertiesToResource(path_url, pdf_data_, NULL, &error_code);
  return error_code == 0;
}

CGPDFDocumentRef PdfMetafile::GetPDFDocument() const {
  // Make sure that we have data, and that it's not being modified any more.
  DCHECK(pdf_data_.get());
  DCHECK(!context_.get());

  if (!pdf_doc_.get()) {
    ScopedCFTypeRef<CGDataProviderRef> pdf_data_provider(
        CGDataProviderCreateWithCFData(pdf_data_));
    pdf_doc_.reset(CGPDFDocumentCreateWithProvider(pdf_data_provider));
  }
  return pdf_doc_.get();
}

}  // namespace printing
