// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Functions for canonicalizing "path" URLs. Not to be confused with the path
// of a URL, these are URLs that have no authority section, only a path. For
// example, "javascript:" and "data:".

#include "url/url_canon.h"
#include "url/url_canon_internal.h"

namespace url_canon {

namespace {

template<typename CHAR, typename UCHAR>
bool DoCanonicalizePathURL(const URLComponentSource<CHAR>& source,
                           const url_parse::Parsed& parsed,
                           CanonOutput* output,
                           url_parse::Parsed* new_parsed) {
  // Scheme: this will append the colon.
  bool success = CanonicalizeScheme(source.scheme, parsed.scheme,
                                    output, &new_parsed->scheme);

  // We assume there's no authority for path URLs. Note that hosts should never
  // have -1 length.
  new_parsed->username.reset();
  new_parsed->password.reset();
  new_parsed->host.reset();
  new_parsed->port.reset();

  if (parsed.path.is_valid()) {
    // Copy the path using path URL's more lax escaping rules (think for
    // javascript:). We convert to UTF-8 and escape non-ASCII, but leave all
    // ASCII characters alone. This helps readability of JavaStript.
    new_parsed->path.begin = output->length();
    int end = parsed.path.end();
    for (int i = parsed.path.begin; i < end; i++) {
      UCHAR uch = static_cast<UCHAR>(source.path[i]);
      if (uch < 0x20 || uch >= 0x80)
        success &= AppendUTF8EscapedChar(source.path, &i, end, output);
      else
        output->push_back(static_cast<char>(uch));
    }
    new_parsed->path.len = output->length() - new_parsed->path.begin;
  } else {
    // Empty path.
    new_parsed->path.reset();
  }

  // Assume there's no query or ref.
  new_parsed->query.reset();
  new_parsed->ref.reset();

  return success;
}

}  // namespace

bool CanonicalizePathURL(const char* spec,
                         int spec_len,
                         const url_parse::Parsed& parsed,
                         CanonOutput* output,
                         url_parse::Parsed* new_parsed) {
  return DoCanonicalizePathURL<char, unsigned char>(
      URLComponentSource<char>(spec), parsed, output, new_parsed);
}

bool CanonicalizePathURL(const char16* spec,
                         int spec_len,
                         const url_parse::Parsed& parsed,
                         CanonOutput* output,
                         url_parse::Parsed* new_parsed) {
  return DoCanonicalizePathURL<char16, char16>(
      URLComponentSource<char16>(spec), parsed, output, new_parsed);
}

bool ReplacePathURL(const char* base,
                    const url_parse::Parsed& base_parsed,
                    const Replacements<char>& replacements,
                    CanonOutput* output,
                    url_parse::Parsed* new_parsed) {
  URLComponentSource<char> source(base);
  url_parse::Parsed parsed(base_parsed);
  SetupOverrideComponents(base, replacements, &source, &parsed);
  return DoCanonicalizePathURL<char, unsigned char>(
      source, parsed, output, new_parsed);
}

bool ReplacePathURL(const char* base,
                    const url_parse::Parsed& base_parsed,
                    const Replacements<char16>& replacements,
                    CanonOutput* output,
                    url_parse::Parsed* new_parsed) {
  RawCanonOutput<1024> utf8;
  URLComponentSource<char> source(base);
  url_parse::Parsed parsed(base_parsed);
  SetupUTF16OverrideComponents(base, replacements, &utf8, &source, &parsed);
  return DoCanonicalizePathURL<char, unsigned char>(
      source, parsed, output, new_parsed);
}

}  // namespace url_canon
