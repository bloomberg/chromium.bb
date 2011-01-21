// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#import <ApplicationServices/ApplicationServices.h>
#import <CoreFoundation/CoreFoundation.h>

#include "base/logging.h"
#include "printing/page_number.h"
#include "printing/printed_page.h"

namespace printing {

void PrintedDocument::RenderPrintedPage(
    const PrintedPage& page, gfx::NativeDrawingContext context) const {
#ifndef NDEBUG
  {
    // Make sure the page is from our list.
    base::AutoLock lock(lock_);
    DCHECK(&page == mutable_.pages_.find(page.page_number() - 1)->second.get());
  }
#endif

  DCHECK(context);

  const printing::PageSetup& page_setup(
      immutable_.settings_.page_setup_device_units());
  gfx::Rect content_area;
  page.GetCenteredPageContentRect(page_setup.physical_size(), &content_area);

  const printing::NativeMetafile* metafile = page.native_metafile();
  // Each NativeMetafile is a one-page PDF, and pages use 1-based indexing.
  const int page_number = 1;
  metafile->RenderPage(page_number, context, content_area.ToCGRect(),
                       false, false, false, false);

  // TODO(stuartmorgan): Print the header and footer.
}

void PrintedDocument::DrawHeaderFooter(gfx::NativeDrawingContext context,
                                       std::wstring text,
                                       gfx::Rect bounds) const {
  NOTIMPLEMENTED();
}

}  // namespace printing
