// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "printing/page_overlays.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "printing/printed_page.h"
#include "printing/printed_pages_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct Keys {
  const wchar_t* key;
  const wchar_t* expected;
};

const Keys kOverlayKeys[] = {
  { printing::PageOverlays::kTitle, L"Foobar Document" },
  { printing::PageOverlays::kTime, L"" },
  { printing::PageOverlays::kDate, L"" },
  { printing::PageOverlays::kPage, L"1" },
  { printing::PageOverlays::kPageCount, L"2" },
  { printing::PageOverlays::kPageOnTotal, L"1/2" },
  { printing::PageOverlays::kUrl, L"http://www.perdu.com/" },
};

class PagesSource : public printing::PrintedPagesSource {
 public:
  virtual string16 RenderSourceName() {
    return ASCIIToUTF16("Foobar Document");
  }

  virtual GURL RenderSourceUrl() {
    return GURL("http://www.perdu.com");
  }
};

}  // namespace

class PageOverlaysTest : public testing::Test {
 private:
  MessageLoop message_loop_;
};

TEST_F(PageOverlaysTest, StringConversion) {
  printing::PageOverlays overlays;
  overlays.GetOverlay(printing::PageOverlays::LEFT,
                      printing::PageOverlays::BOTTOM);
  printing::PrintSettings settings;
  PagesSource source;
  int cookie = 1;
  scoped_refptr<printing::PrintedDocument> doc(
      new printing::PrintedDocument(settings, &source, cookie));
  doc->set_page_count(2);
  gfx::Size page_size(100, 100);
  gfx::Rect page_content_area(5, 5, 90, 90);
  scoped_refptr<printing::PrintedPage> page(
      new printing::PrintedPage(1, NULL, page_size, page_content_area, true));

  std::wstring input;
  std::wstring out;
  for (size_t i = 0; i < arraysize(kOverlayKeys); ++i) {
    input = base::StringPrintf(L"foo%lsbar", kOverlayKeys[i].key);
    out = printing::PageOverlays::ReplaceVariables(input, *doc.get(),
                                                   *page.get());
    EXPECT_FALSE(out.empty());
    if (wcslen(kOverlayKeys[i].expected) == 0)
      continue;
    std::wstring expected =
        base::StringPrintf(L"foo%lsbar", kOverlayKeys[i].expected);
    EXPECT_EQ(expected, out) << kOverlayKeys[i].key;
  }

  // Check if SetOverlay really sets the page overlay.
  overlays.SetOverlay(printing::PageOverlays::LEFT,
                      printing::PageOverlays::TOP,
                      L"Page {page}");
  input = overlays.GetOverlay(printing::PageOverlays::LEFT,
                              printing::PageOverlays::TOP);
  EXPECT_EQ(input, L"Page {page}");

  // Replace the variables to see if the page number is correct.
  out = printing::PageOverlays::ReplaceVariables(input, *doc.get(),
                                                 *page.get());
  EXPECT_EQ(out, L"Page 1");
}
