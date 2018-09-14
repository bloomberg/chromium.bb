/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/clipboard/clipboard_utilities.h"

#include "net/base/escape.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

void ReplaceNBSPWithSpace(String& str) {
  static const UChar kNonBreakingSpaceCharacter = 0xA0;
  static const UChar kSpaceCharacter = ' ';
  str.Replace(kNonBreakingSpaceCharacter, kSpaceCharacter);
}

String ConvertURIListToURL(const String& uri_list) {
  Vector<String> items;
  // Line separator is \r\n per RFC 2483 - however, for compatibility
  // reasons we allow just \n here.
  uri_list.Split('\n', items);
  // Process the input and return the first valid URL. In case no URLs can
  // be found, return an empty string. This is in line with the HTML5 spec.
  for (wtf_size_t i = 0; i < items.size(); ++i) {
    String& line = items[i];
    line = line.StripWhiteSpace();
    if (line.IsEmpty())
      continue;
    if (line[0] == '#')
      continue;
    KURL url = KURL(line);
    if (url.IsValid())
      return url;
  }
  return String();
}

static String EscapeForHTML(const String& str) {
  std::string output =
      net::EscapeForHTML(StringUTF8Adaptor(str).AsStringPiece());
  return String(output.c_str());
}

String URLToImageMarkup(const KURL& url, const String& title) {
  StringBuilder builder;
  builder.Append("<img src=\"");
  builder.Append(EscapeForHTML(url.GetString()));
  builder.Append("\"");
  if (!title.IsEmpty()) {
    builder.Append(" alt=\"");
    builder.Append(EscapeForHTML(title));
    builder.Append("\"");
  }
  builder.Append("/>");
  return builder.ToString();
}

}  // namespace blink
