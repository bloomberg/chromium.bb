// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/names.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace service_manager {

bool IsValidName(const std::string& name) {
  std::vector<std::string> parts =
      base::SplitString(name, ":", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  if (parts.size() != 2)
    return false;

  if (parts.front().empty() || parts.front() != "service")
    return false;

  const std::string& path = parts.back();
  return !path.empty() &&
      !base::StartsWith(path, "//", base::CompareCase::INSENSITIVE_ASCII);
}

std::string GetNamePath(const std::string& name) {
  std::vector<std::string> parts =
      base::SplitString(name, ":", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  DCHECK(2 == parts.size());
  return parts.back();
}

}  // namespace service_manager
