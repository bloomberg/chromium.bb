// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_metafile_cg_mac.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "base/threading/thread_local.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

using base::mac::ScopedCFTypeRef;

namespace {

// What is up with this ugly hack? <http://crbug.com/64641>, that's what.
// The bug: Printing certain PDFs crashes. The cause: When printing, the
// renderer process assembles pages one at a time, in PDF format, to send to the
// browser process. When printing a PDF, the PDF plugin returns output in PDF
// format. There is a bug in 10.5 and 10.6 (<rdar://9018916>,
// <http://www.openradar.me/9018916>) where reference counting is broken when
// drawing certain PDFs into PDF contexts. So at the high-level, a PdfMetafileCg
// is used to hold the destination context, and then about five layers down on
// the callstack, a PdfMetafileCg is used to hold the source PDF. If the source
// PDF is drawn into the destination PDF context and then released, accessing
// the destination PDF context will crash. So the outermost instantiation of
// PdfMetafileCg creates a pool for deeper instantiations to dump their used
// PDFs into rather than releasing them. When the top-level PDF is closed, then
// it's safe to clear the pool. A thread local is used to allow this to work in
// single-process mode. TODO(avi): This Apple bug appears fixed in 10.7; when
// 10.7 is the minimum required version for Chromium, remove this hack.

base::ThreadLocalPointer<struct __CFSet> thread_pdf_docs;

}  // namespace

namespace printing {

PdfMetafileCg::PdfMetafileCg()
    : page_is_open_(false),
      thread_pdf_docs_owned_(false) {
  if (!thread_pdf_docs.Get() && base::mac::IsOSSnowLeopardOrEarlier()) {
    thread_pdf_docs_owned_ = true;
    thread_pdf_docs.Set(CFSetCreateMutable(kCFAllocatorDefault,
                                           0,
                                           &kCFTypeSetCallBacks));
  }
}

PdfMetafileCg::~PdfMetafileCg() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (pdf_doc_ && thread_pdf_docs.Get()) {
    // Transfer ownership to the pool.
    CFSetAddValue(thread_pdf_docs.Get(), pdf_doc_);
  }

  if (thread_pdf_docs_owned_) {
    CFRelease(thread_pdf_docs.Get());
    thread_pdf_docs.Set(NULL);
  }
}

bool PdfMetafileCg::Init() {
  // Ensure that Init hasn't already been called.
  DCHECK(!context_.get());
  DCHECK(!pdf_data_.get());

  pdf_data_.reset(CFDataCreateMutable(kCFAllocatorDefault, 0));
  if (!pdf_data_.get()) {
    LOG(ERROR) << "Failed to create pdf data for metafile";
    return false;
  }
  ScopedCFTypeRef<CGDataConsumerRef> pdf_consumer(
      CGDataConsumerCreateWithCFData(pdf_data_));
  if (!pdf_consumer.get()) {
    LOG(ERROR) << "Failed to create data consumer for metafile";
    pdf_data_.reset(NULL);
    return false;
  }
  context_.reset(CGPDFContextCreate(pdf_consumer, NULL, NULL));
  if (!context_.get()) {
    LOG(ERROR) << "Failed to create pdf context for metafile";
    pdf_data_.reset(NULL);
  }

  return true;
}

bool PdfMetafileCg::InitFromData(const void* src_buffer,
                                 uint32 src_buffer_size) {
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

SkDevice* PdfMetafileCg::StartPageForVectorCanvas(
    const gfx::Size& page_size, const gfx::Rect& content_area,
    const float& scale_factor) {
  NOTIMPLEMENTED();
  return NULL;
}

bool PdfMetafileCg::StartPage(const gfx::Size& page_size,
                              const gfx::Rect& content_area,
                              const float& scale_factor) {
  DCHECK(context_.get());
  DCHECK(!page_is_open_);

  double height = page_size.height();
  double width = page_size.width();

  CGRect bounds = CGRectMake(0, 0, width, height);
  CGContextBeginPage(context_, &bounds);
  page_is_open_ = true;
  CGContextSaveGState(context_);

  // Move to the context origin.
  CGContextTranslateCTM(context_, content_area.x(), -content_area.y());

  // Flip the context.
  CGContextTranslateCTM(context_, 0, height);
  CGContextScaleCTM(context_, scale_factor, -scale_factor);

  return context_.get() != NULL;
}

bool PdfMetafileCg::FinishPage() {
  DCHECK(context_.get());
  DCHECK(page_is_open_);

  CGContextRestoreGState(context_);
  CGContextEndPage(context_);
  page_is_open_ = false;
  return true;
}

bool PdfMetafileCg::FinishDocument() {
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
  return true;
}

bool PdfMetafileCg::RenderPage(unsigned int page_number,
                               CGContextRef context,
                               const CGRect rect,
                               bool shrink_to_fit,
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

unsigned int PdfMetafileCg::GetPageCount() const {
  CGPDFDocumentRef pdf_doc = GetPDFDocument();
  return pdf_doc ? CGPDFDocumentGetNumberOfPages(pdf_doc) : 0;
}

gfx::Rect PdfMetafileCg::GetPageBounds(unsigned int page_number) const {
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

uint32 PdfMetafileCg::GetDataSize() const {
  // PDF data is only valid/complete once the context is released.
  DCHECK(!context_);

  if (!pdf_data_)
    return 0;
  return static_cast<uint32>(CFDataGetLength(pdf_data_));
}

bool PdfMetafileCg::GetData(void* dst_buffer, uint32 dst_buffer_size) const {
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

bool PdfMetafileCg::SaveTo(const FilePath& file_path) const {
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

CGContextRef PdfMetafileCg::context() const {
  return context_.get();
}

CGPDFDocumentRef PdfMetafileCg::GetPDFDocument() const {
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
