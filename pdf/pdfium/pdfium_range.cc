// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_range.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"

namespace chrome_pdf {

PDFiumRange::PDFiumRange(PDFiumPage* page, int char_index, int char_count)
    : page_(page),
      char_index_(char_index),
      char_count_(char_count),
      cached_screen_rects_zoom_(0) {}

PDFiumRange::PDFiumRange(const PDFiumRange& that) = default;

PDFiumRange::~PDFiumRange() = default;

void PDFiumRange::SetCharCount(int char_count) {
  char_count_ = char_count;

  cached_screen_rects_offset_ = pp::Point();
  cached_screen_rects_zoom_ = 0;
}

std::vector<pp::Rect> PDFiumRange::GetScreenRects(const pp::Point& offset,
                                                  double zoom,
                                                  int rotation) {
  if (offset == cached_screen_rects_offset_ &&
      zoom == cached_screen_rects_zoom_) {
    return cached_screen_rects_;
  }

  cached_screen_rects_.clear();
  cached_screen_rects_offset_ = offset;
  cached_screen_rects_zoom_ = zoom;

  int char_index = char_index_;
  int char_count = char_count_;
  if (char_count < 0) {
    char_count *= -1;
    char_index -= char_count - 1;
  }

  int count = FPDFText_CountRects(page_->GetTextPage(), char_index, char_count);
  for (int i = 0; i < count; ++i) {
    double left, top, right, bottom;
    FPDFText_GetRect(page_->GetTextPage(), i, &left, &top, &right, &bottom);
    cached_screen_rects_.push_back(
        page_->PageToScreen(offset, zoom, left, top, right, bottom, rotation));
  }

  return cached_screen_rects_;
}

base::string16 PDFiumRange::GetText() const {
  int index = char_index_;
  int count = char_count_;
  base::string16 rv;
  if (count < 0) {
    count *= -1;
    index -= count - 1;
  }

  if (count > 0) {
    // Adding +1 to count to account for the string terminator that is included.
    PDFiumAPIStringBufferAdapter<base::string16> api_string_adapter(
        &rv, count + 1, false);
    unsigned short* data =
        reinterpret_cast<unsigned short*>(api_string_adapter.GetData());
    int written =
        FPDFText_GetText(page_->GetTextPage(), index, count + 1, data);
    api_string_adapter.Close(written);
  }

  return rv;
}

}  // namespace chrome_pdf
