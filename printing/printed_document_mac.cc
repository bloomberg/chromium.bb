// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
    const PrintedPage& page,
    printing::NativeDrawingContext context) const {
  DCHECK(context);

  // |mutable_.pages_| is 0-based, whereas the page number is 1-based.
  const int page_index = page.page_number() - 1;

  const MetafilePlayer* metafile;
  size_t metafile_page_number;
  {
    base::AutoLock lock(lock_);

    // Make sure |page| is from our list.
    DCHECK(&page == mutable_.pages_.find(page_index)->second.get());

    // Make sure the first page exists in the list too.
    DCHECK_GE(mutable_.first_page, 0);
    DCHECK_NE(mutable_.first_page, INT_MAX);
    DCHECK(mutable_.pages_.find(mutable_.first_page)->second.get());

    // Always use the metafile from the first page.
    metafile = mutable_.pages_.find(mutable_.first_page)->second->metafile();

    // Figure out the mapping from |page| to the page inside |metafile|.
    // e.g. If |metafile| contains 3 pages that corresponds to pages 1, 5, 7,
    // then page 5 has index 1, which is page number 2.
    // Use 1-based page number here because that is what RenderPage() takes.
    metafile_page_number = 1;
    for (const auto& it : mutable_.pages_) {
      if (it.first == page_index)
        break;
      ++metafile_page_number;
    }
  }

  const PageSetup& page_setup = immutable_.settings_.page_setup_device_units();
  gfx::Rect content_area =
      page.GetCenteredPageContentRect(page_setup.physical_size());

  struct Metafile::MacRenderPageParams params;
  params.autorotate = true;
  metafile->RenderPage(metafile_page_number, context, content_area.ToCGRect(),
                       params);
}

}  // namespace printing
