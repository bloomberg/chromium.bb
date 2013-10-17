// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/trusted/plugin/nacl_http_response_headers.h"

#include <algorithm>
#include <sstream>

namespace {

// TODO(jvoung): Use Tokenize from base/strings/string_util.h when this moves
// to Chromium.
void SplitString(const std::string& str,
                 char delim,
                 std::vector<std::string>* elems) {
  std::stringstream ss(str);
  std::string item;
  while (std::getline(ss, item, delim)) {
    elems->push_back(item);
  }
}

bool SplitOnce(const std::string& str,
               char delim,
               std::vector<std::string>* elems) {
  size_t pos = str.find(delim);
  if (pos != std::string::npos) {
    elems->push_back(str.substr(0, pos));
    elems->push_back(str.substr(pos + 1));
    return true;
  }
  return false;
}

}  // namespace

namespace plugin {

NaClHttpResponseHeaders::NaClHttpResponseHeaders() {}

NaClHttpResponseHeaders::~NaClHttpResponseHeaders() {}

void NaClHttpResponseHeaders::Parse(const std::string& headers_str) {
  // PPAPI response headers are \n delimited. Separate out the lines.
  std::vector<std::string> lines;
  SplitString(headers_str, '\n', &lines);

  for (size_t i = 0; i < lines.size(); ++i) {
    std::vector<std::string> tokens;
    // Split along the key-value pair separator char.
    if (!SplitOnce(lines[i], ':', &tokens)) {
      // Ignore funny header lines that don't have the key-value separator.
      continue;
    }
    std::string key = tokens[0];
    // Also ignore keys that start with white-space (they are invalid).
    // See: HttpResponseHeadersTest.NormalizeHeadersLeadingWhitespace.
    if (key.length() == 0 || key[0] == ' ' || key[0] == '\t')
      continue;
    // TODO(jvoung): replace some of this with TrimWhitespaceASCII when
    // we move code to chromium.
    // Strip trailing whitespace from the key to normalize.
    size_t pos = key.find_last_not_of(" \t");
    if (pos != std::string::npos)
      key.erase(pos + 1);
    // Strip leading whitespace from the value to normalize.
    std::string value = tokens[1];
    value.erase(0, value.find_first_not_of(" \t"));
    header_entries_.push_back(Entry(key, value));
  }
}

std::string NaClHttpResponseHeaders::GetHeader(const std::string& name) {
  for (size_t i = 0; i < header_entries_.size(); ++i) {
    const Entry& entry = header_entries_[i];
    std::string key = entry.first;
    // TODO(jvoung): replace with LowerCaseEqualsASCII later.
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    if (key.compare(name) == 0)
      return entry.second;
  }
  return std::string();
}

std::string NaClHttpResponseHeaders::GetCacheValidators() {
  std::string result = GetHeader("etag");
  if (!result.empty())
    result = "etag:" + result;
  std::string lm = GetHeader("last-modified");
  if (!lm.empty()) {
    if (!result.empty())
      result += "&";
    result += "last-modified:" + lm;
  }
  return result;
}

bool NaClHttpResponseHeaders::CacheControlNoStore() {
  for (size_t i = 0; i < header_entries_.size(); ++i) {
    const Entry& entry = header_entries_[i];
    std::string key = entry.first;
    // TODO(jvoung): replace with LowerCaseEqualsASCII later.
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    if (key.compare("cache-control") == 0) {
      std::string cc = entry.second;
      std::transform(cc.begin(), cc.end(), cc.begin(), ::tolower);
      if (entry.second.find("no-store") != std::string::npos)
        return true;
    }
  }
  return false;
}

}  // namespace plugin
