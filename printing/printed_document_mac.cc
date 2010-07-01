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

  const printing::PageSetup& page_setup(
      immutable_.settings_.page_setup_device_units());
  gfx::Rect target_rect = page.page_content_rect();
  const gfx::Rect& physical_size = page_setup.physical_size();
  // http://dev.w3.org/csswg/css3-page/#positioning-page-box
  if (physical_size.width() > page.page_size().width()) {
    int diff = physical_size.width() - page.page_size().width();
    target_rect.set_x(target_rect.x() + diff / 2);
  }
  if (physical_size.height() > page.page_size().height()) {
    int diff = physical_size.height() - page.page_size().height();
    target_rect.set_y(target_rect.y() + diff / 2);
  }

  const printing::NativeMetafile* metafile = page.native_metafile();
  // Each NativeMetafile is a one-page PDF, and pages use 1-based indexing.
  const int page_number = 1;
  metafile->RenderPage(page_number, context, target_rect.ToCGRect(),
                       true, false, false, false);

  // TODO(stuartmorgan): Print the header and footer.
}

}  // namespace printing
