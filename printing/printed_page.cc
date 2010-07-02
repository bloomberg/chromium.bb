// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_page.h"

namespace printing {

PrintedPage::PrintedPage(int page_number,
                         NativeMetafile* native_metafile,
                         const gfx::Size& page_size,
                         const gfx::Rect& page_content_rect,
                         bool has_visible_overlays)
    : page_number_(page_number),
      native_metafile_(native_metafile),
      page_size_(page_size),
      page_content_rect_(page_content_rect),
      has_visible_overlays_(has_visible_overlays) {
}

PrintedPage::~PrintedPage() {
}

const NativeMetafile* PrintedPage::native_metafile() const {
  return native_metafile_.get();
}

}  // namespace printing
