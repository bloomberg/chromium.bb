// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEXT_TEXT_ELIDER_H_
#define UI_BASE_TEXT_TEXT_ELIDER_H_
#pragma once

#include <unicode/coll.h>
#include <unicode/uchar.h>

#include "base/basictypes.h"
#include "base/string16.h"
#include "ui/gfx/font.h"

class FilePath;
class GURL;

// TODO(port): this file should deal in string16s rather than wstrings.
namespace ui {

// This function takes a GURL object and elides it. It returns a string
// which composed of parts from subdomain, domain, path, filename and query.
// A "..." is added automatically at the end if the elided string is bigger
// than the available pixel width. For available pixel width = 0, empty
// string is returned. |languages| is a comma separted list of ISO 639
// language codes and is used to determine what characters are understood
// by a user. It should come from |prefs::kAcceptLanguages|.
//
// Note: in RTL locales, if the URL returned by this function is going to be
// displayed in the UI, then it is likely that the string needs to be marked
// as an LTR string (using base::i18n::WrapStringWithLTRFormatting()) so that it
// is displayed properly in an RTL context. Please refer to
// http://crbug.com/6487 for more information.
string16 ElideUrl(const GURL& url,
                  const gfx::Font& font,
                  int available_pixel_width,
                  const std::wstring& languages);

// Elides |text| to fit in |available_pixel_width|.  If |elide_in_middle| is
// set the ellipsis is placed in the middle of the string; otherwise it is
// placed at the end.
string16 ElideText(const string16& text,
                   const gfx::Font& font,
                   int available_pixel_width,
                   bool elide_in_middle);

// Elide a filename to fit a given pixel width, with an emphasis on not hiding
// the extension unless we have to. If filename contains a path, the path will
// be removed if filename doesn't fit into available_pixel_width. The elided
// filename is forced to have LTR directionality, which means that in RTL UI
// the elided filename is wrapped with LRE (Left-To-Right Embedding) mark and
// PDF (Pop Directional Formatting) mark.
string16 ElideFilename(const FilePath& filename,
                       const gfx::Font& font,
                       int available_pixel_width);

// SortedDisplayURL maintains a string from a URL suitable for display to the
// use. SortedDisplayURL also provides a function used for comparing two
// SortedDisplayURLs for use in visually ordering the SortedDisplayURLs.
//
// SortedDisplayURL is relatively cheap and supports value semantics.
class SortedDisplayURL {
 public:
  SortedDisplayURL(const GURL& url, const std::wstring& languages);
  SortedDisplayURL();
  ~SortedDisplayURL();

  // Compares this SortedDisplayURL to |url| using |collator|. Returns a value
  // < 0, = 1 or > 0 as to whether this url is less then, equal to or greater
  // than the supplied url.
  int Compare(const SortedDisplayURL& other, icu::Collator* collator) const;

  // Returns the display string for the URL.
  const string16& display_url() const { return display_url_; }

 private:
  // Returns everything after the host. This is used by Compare if the hosts
  // match.
  string16 AfterHost() const;

  // Host name minus 'www.'. Used by Compare.
  string16 sort_host_;

  // End of the prefix (spec and separator) in display_url_.
  size_t prefix_end_;

  string16 display_url_;
};

// Functions to elide strings when the font information is unknown.  As
// opposed to the above functions, the ElideString() and
// ElideRectangleString() functions operate in terms of character units,
// not pixels.

// If the size of |input| is more than |max_len|, this function returns
// true and |input| is shortened into |output| by removing chars in the
// middle (they are replaced with up to 3 dots, as size permits).
// Ex: ElideString(L"Hello", 10, &str) puts Hello in str and returns false.
// ElideString(L"Hello my name is Tom", 10, &str) puts "Hell...Tom" in str
// and returns true.
// TODO(tsepez): Doesn't handle UTF-16 surrogate pairs properly.
// TODO(tsepez): Doesn't handle bidi properly
bool ElideString(const std::wstring& input, int max_len, std::wstring* output);

// Reformat |input| into |output| so that it fits into a |max_rows| by
// |max_cols| rectangle of characters.  Input newlines are respected, but
// lines that are too long are broken into pieces, first at naturally
// occuring whitespace boundaries, and then intra-word (respecting UTF-16
// surrogate pairs) as necssary. Truncation (indicated by an added 3 dots)
// occurs if the result is still too long.  Returns true if the input had
// to be truncated (and not just reformatted).
bool ElideRectangleString(const string16& input, size_t max_rows,
                          size_t max_cols, string16* output);


} // namespace ui

#endif  // UI_BASE_TEXT_TEXT_ELIDER_H_
