// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>

#include "base/logging.h"
#include "printing/page_number.h"
#include "printing/pdf_metafile_cg_mac.h"
#include "printing/printed_page.h"

namespace printing {

void PrintedDocument::RenderPrintedPage(
    const PrintedPage& page,
    skia::NativeDrawingContext context) const {
#ifndef NDEBUG
  {
    // Make sure the page is from our list.
    base::AutoLock lock(lock_);
    DCHECK(&page == mutable_.pages_.find(page.page_number() - 1)->second.get());
  }
#endif

  DCHECK(context);

  const PageSetup& page_setup(immutable_.settings_.page_setup_device_units());
  gfx::Rect content_area;
  page.GetCenteredPageContentRect(page_setup.physical_size(), &content_area);

  const MetafilePlayer* metafile = page.metafile();

  // Each Metafile is a one-page PDF, and pages use 1-based indexing.
  const int page_number = 1;
  PdfMetafileCg::RenderPageParams params;
  params.autorotate = true;
  std::vector<char> buffer;
  if (metafile->GetDataAsVector(&buffer)) {
    PdfMetafileCg::RenderPage(buffer, page_number, context,
                              content_area.ToCGRect(), params);
  }
}

}  // namespace printing
