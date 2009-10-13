// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#import <ApplicationServices/ApplicationServices.h>
#import <CoreFoundation/CoreFoundation.h>

#include "base/logging.h"
#include "base/scoped_cftyperef.h"
#include "printing/page_number.h"
#include "printing/printed_page.h"

namespace printing {

void PrintedDocument::RenderPrintedPage(
    const PrintedPage& page, gfx::NativeDrawingContext context) const {
#ifndef NDEBUG
  {
    // Make sure the page is from our list.
    AutoLock lock(lock_);
    DCHECK(&page == mutable_.pages_.find(page.page_number() - 1)->second.get());
  }
#endif

  const printing::NativeMetafile* metafile = page.native_metafile();
  unsigned int data_length = metafile->GetDataSize();
  scoped_cftyperef<CFMutableDataRef> pdf_data(
      CFDataCreateMutable(kCFAllocatorDefault, data_length));
  CFDataIncreaseLength(pdf_data, data_length);
  metafile->GetData(CFDataGetMutableBytePtr(pdf_data), data_length);
  scoped_cftyperef<CGDataProviderRef> pdf_data_provider(
      CGDataProviderCreateWithCFData(pdf_data));
  scoped_cftyperef<CGPDFDocumentRef> pdf_doc(
      CGPDFDocumentCreateWithProvider(pdf_data_provider));
  if (!pdf_doc.get()) {
    NOTREACHED() << "Unable to create PDF document from print data";
    return;
  }

  const printing::PageSetup& page_setup(
      immutable_.settings_.page_setup_pixels());
  CGRect target_rect = page_setup.content_area().ToCGRect();

  // Each NativeMetafile is a one-page PDF.
  const int page_number = 1;
  CGContextDrawPDFDocument(context, target_rect, pdf_doc, page_number);

  // TODO(stuartmorgan): Print the header and footer.
}

}  // namespace printing
