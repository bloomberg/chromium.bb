// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/i18n/time_formatting.h"
#include "gfx/font.h"
#include "printing/page_number.h"
#include "printing/page_overlays.h"
#include "printing/printed_pages_source.h"
#include "printing/printed_page.h"
#include "printing/units.h"
#include "skia/ext/platform_device.h"
#include "ui/base/text/text_elider.h"

namespace {

struct PrintDebugDumpPath {
  PrintDebugDumpPath()
    : enabled(false) {
  }

  bool enabled;
  FilePath debug_dump_path;
};

static base::LazyInstance<PrintDebugDumpPath> g_debug_dump_info(
    base::LINKER_INITIALIZED);

}  // namespace

namespace printing {

PrintedDocument::PrintedDocument(const PrintSettings& settings,
                                 PrintedPagesSource* source,
                                 int cookie)
    : mutable_(source),
      immutable_(settings, source, cookie) {

  // Records the expected page count if a range is setup.
  if (!settings.ranges.empty()) {
    // If there is a range, set the number of page
    for (unsigned i = 0; i < settings.ranges.size(); ++i) {
      const PageRange& range = settings.ranges[i];
      mutable_.expected_page_count_ += range.to - range.from + 1;
    }
  }
}

PrintedDocument::~PrintedDocument() {
}

void PrintedDocument::SetPage(int page_number,
                              NativeMetafile* metafile,
                              double shrink,
                              const gfx::Size& paper_size,
                              const gfx::Rect& page_rect,
                              bool has_visible_overlays) {
  // Notice the page_number + 1, the reason is that this is the value that will
  // be shown. Users dislike 0-based counting.
  scoped_refptr<PrintedPage> page(
      new PrintedPage(page_number + 1,
                      metafile,
                      paper_size,
                      page_rect,
                      has_visible_overlays));
  {
    AutoLock lock(lock_);
    mutable_.pages_[page_number] = page;
    if (mutable_.shrink_factor == 0) {
      mutable_.shrink_factor = shrink;
    } else {
      DCHECK_EQ(mutable_.shrink_factor, shrink);
    }
  }
  DebugDump(*page);
}

bool PrintedDocument::GetPage(int page_number,
                              scoped_refptr<PrintedPage>* page) {
  AutoLock lock(lock_);
  PrintedPages::const_iterator itr = mutable_.pages_.find(page_number);
  if (itr != mutable_.pages_.end()) {
    if (itr->second.get()) {
      *page = itr->second;
      return true;
    }
  }
  return false;
}

bool PrintedDocument::IsComplete() const {
  AutoLock lock(lock_);
  if (!mutable_.page_count_)
    return false;
  PageNumber page(immutable_.settings_, mutable_.page_count_);
  if (page == PageNumber::npos())
    return false;
  for (; page != PageNumber::npos(); ++page) {
    PrintedPages::const_iterator itr = mutable_.pages_.find(page.ToInt());
    if (itr == mutable_.pages_.end() || !itr->second.get() ||
        !itr->second->native_metafile())
      return false;
  }
  return true;
}

void PrintedDocument::DisconnectSource() {
  AutoLock lock(lock_);
  mutable_.source_ = NULL;
}

uint32 PrintedDocument::MemoryUsage() const {
  std::vector< scoped_refptr<PrintedPage> > pages_copy;
  {
    AutoLock lock(lock_);
    pages_copy.reserve(mutable_.pages_.size());
    PrintedPages::const_iterator end = mutable_.pages_.end();
    for (PrintedPages::const_iterator itr = mutable_.pages_.begin();
         itr != end; ++itr) {
      if (itr->second.get()) {
        pages_copy.push_back(itr->second);
      }
    }
  }
  uint32 total = 0;
  for (size_t i = 0; i < pages_copy.size(); ++i) {
    total += pages_copy[i]->native_metafile()->GetDataSize();
  }
  return total;
}

void PrintedDocument::set_page_count(int max_page) {
  AutoLock lock(lock_);
  DCHECK_EQ(0, mutable_.page_count_);
  mutable_.page_count_ = max_page;
  if (immutable_.settings_.ranges.empty()) {
    mutable_.expected_page_count_ = max_page;
  } else {
    // If there is a range, don't bother since expected_page_count_ is already
    // initialized.
    DCHECK_NE(mutable_.expected_page_count_, 0);
  }
}

int PrintedDocument::page_count() const {
  AutoLock lock(lock_);
  return mutable_.page_count_;
}

int PrintedDocument::expected_page_count() const {
  AutoLock lock(lock_);
  return mutable_.expected_page_count_;
}

void PrintedDocument::PrintHeaderFooter(gfx::NativeDrawingContext context,
                                        const PrintedPage& page,
                                        PageOverlays::HorizontalPosition x,
                                        PageOverlays::VerticalPosition y,
                                        const gfx::Font& font) const {
  const PrintSettings& settings = immutable_.settings_;
  if (!settings.use_overlays || !page.has_visible_overlays()) {
    return;
  }
  const std::wstring& line = settings.overlays.GetOverlay(x, y);
  if (line.empty()) {
    return;
  }
  std::wstring output(PageOverlays::ReplaceVariables(line, *this, page));
  if (output.empty()) {
    // May happen if document name or url is empty.
    return;
  }
  const gfx::Size string_size(font.GetStringWidth(WideToUTF16Hack(output)),
                              font.GetHeight());
  gfx::Rect bounding;
  bounding.set_height(string_size.height());
  const gfx::Rect& overlay_area(
      settings.page_setup_device_units().overlay_area());
  // Hard code .25 cm interstice between overlays. Make sure that some space is
  // kept between each headers.
  const int interstice = ConvertUnit(250, kHundrethsMMPerInch,
                                     settings.device_units_per_inch());
  const int max_width = overlay_area.width() / 3 - interstice;
  const int actual_width = std::min(string_size.width(), max_width);
  switch (x) {
    case PageOverlays::LEFT:
      bounding.set_x(overlay_area.x());
      bounding.set_width(max_width);
      break;
    case PageOverlays::CENTER:
      bounding.set_x(overlay_area.x() +
                     (overlay_area.width() - actual_width) / 2);
      bounding.set_width(actual_width);
      break;
    case PageOverlays::RIGHT:
      bounding.set_x(overlay_area.right() - actual_width);
      bounding.set_width(actual_width);
      break;
  }

  DCHECK_LE(bounding.right(), overlay_area.right());

  switch (y) {
    case PageOverlays::BOTTOM:
      bounding.set_y(overlay_area.bottom() - string_size.height());
      break;
    case PageOverlays::TOP:
      bounding.set_y(overlay_area.y());
      break;
  }

  if (string_size.width() > bounding.width()) {
    if (line == PageOverlays::kUrl) {
      output = UTF16ToWideHack(ui::ElideUrl(url(), font, bounding.width(),
                                            std::wstring()));
    } else {
      output = UTF16ToWideHack(ui::ElideText(WideToUTF16Hack(output),
          font, bounding.width(), false));
    }
  }

  DrawHeaderFooter(context, output, bounding);
}

void PrintedDocument::DebugDump(const PrintedPage& page) {
  if (!g_debug_dump_info.Get().enabled)
    return;

  string16 filename;
  filename += date();
  filename += ASCIIToUTF16("_");
  filename += time();
  filename += ASCIIToUTF16("_");
  filename += name();
  filename += ASCIIToUTF16("_");
  filename += ASCIIToUTF16(StringPrintf("%02d", page.page_number()));
  filename += ASCIIToUTF16("_.emf");
#if defined(OS_WIN)
  page.native_metafile()->SaveTo(
      g_debug_dump_info.Get().debug_dump_path.Append(filename).ToWStringHack());
#else  // OS_WIN
  NOTIMPLEMENTED();
#endif  // OS_WIN
}

void PrintedDocument::set_debug_dump_path(const FilePath& debug_dump_path) {
  g_debug_dump_info.Get().enabled = !debug_dump_path.empty();
  g_debug_dump_info.Get().debug_dump_path = debug_dump_path;
}

const FilePath& PrintedDocument::debug_dump_path() {
  return g_debug_dump_info.Get().debug_dump_path;
}

PrintedDocument::Mutable::Mutable(PrintedPagesSource* source)
    : source_(source),
      expected_page_count_(0),
      page_count_(0),
      shrink_factor(0) {
}

PrintedDocument::Mutable::~Mutable() {
}

PrintedDocument::Immutable::Immutable(const PrintSettings& settings,
                                      PrintedPagesSource* source,
                                      int cookie)
    : settings_(settings),
      source_message_loop_(MessageLoop::current()),
      name_(source->RenderSourceName()),
      url_(source->RenderSourceUrl()),
      cookie_(cookie) {
  SetDocumentDate();
}

PrintedDocument::Immutable::~Immutable() {
}

}  // namespace printing
