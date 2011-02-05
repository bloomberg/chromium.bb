// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTED_PAGE_H_
#define PRINTING_PRINTED_PAGE_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "printing/native_metafile.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace printing {

// Contains the data to reproduce a printed page, either on screen or on
// paper. Once created, this object is immutable. It has no reference to the
// PrintedDocument containing this page.
// Note: May be accessed from many threads at the same time. This is an non
// issue since this object is immutable. The reason is that a page may be
// printed and be displayed at the same time.
class PrintedPage : public base::RefCountedThreadSafe<PrintedPage> {
 public:
  PrintedPage(int page_number,
              NativeMetafile* native_metafile,
              const gfx::Size& page_size,
              const gfx::Rect& page_content_rect,
              bool has_visible_overlays);

  // Getters
  int page_number() const { return page_number_; }
  const NativeMetafile* native_metafile() const;
  const gfx::Size& page_size() const { return page_size_; }
  const gfx::Rect& page_content_rect() const { return page_content_rect_; }
  bool has_visible_overlays() const { return has_visible_overlays_; }

  // Get page content rect adjusted based on
  // http://dev.w3.org/csswg/css3-page/#positioning-page-box
  void GetCenteredPageContentRect(const gfx::Size& paper_size,
                                  gfx::Rect* content_rect) const;

 private:
  friend class base::RefCountedThreadSafe<PrintedPage>;

  ~PrintedPage();

  // Page number inside the printed document.
  const int page_number_;

  // Actual paint data.
  const scoped_ptr<NativeMetafile> native_metafile_;

  // The physical page size. To support multiple page formats inside on print
  // job.
  const gfx::Size page_size_;

  // The printable area of the page.
  const gfx::Rect page_content_rect_;

  // True if the overlays should be visible in this page.
  bool has_visible_overlays_;

  DISALLOW_COPY_AND_ASSIGN(PrintedPage);
};

}  // namespace printing

#endif  // PRINTING_PRINTED_PAGE_H_
