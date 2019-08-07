// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_HTTP_STRUCTURED_HEADER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_HTTP_STRUCTURED_HEADER_H_

#include <map>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"

namespace content {
namespace http_structured_header {

struct CONTENT_EXPORT ParameterisedIdentifier {
  using Parameters = std::map<std::string, std::string>;

  std::string identifier;
  Parameters params;

  ParameterisedIdentifier(const ParameterisedIdentifier&);
  ParameterisedIdentifier(const std::string&, const Parameters&);
  ~ParameterisedIdentifier();
};

using ParameterisedList = std::vector<ParameterisedIdentifier>;
using ListOfLists = std::vector<std::vector<std::string>>;

CONTENT_EXPORT base::Optional<std::string> ParseItem(
    const base::StringPiece& str);
CONTENT_EXPORT base::Optional<ParameterisedList> ParseParameterisedList(
    const base::StringPiece& str);
CONTENT_EXPORT base::Optional<ListOfLists> ParseListOfLists(
    const base::StringPiece& str);

}  // namespace http_structured_header
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_HTTP_STRUCTURED_HEADER_H_
