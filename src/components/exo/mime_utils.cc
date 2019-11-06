// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/mime_utils.h"

namespace {

constexpr char kCharset[] = ";charset=";
constexpr char kDefaultCharset[] = "US-ASCII";

}  // namespace

namespace exo {

std::string GetCharset(const std::string& mime_type) {
  auto pos = mime_type.find(kCharset);
  if (pos == std::string::npos)
    return std::string(kDefaultCharset);
  return mime_type.substr(pos + sizeof(kCharset) - 1);
}

}  // namespace exo
