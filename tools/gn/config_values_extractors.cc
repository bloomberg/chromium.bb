// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/config_values_extractors.h"

namespace {

struct StringWriter {
  void operator()(const std::string& s, std::ostream& out) const {
    out << " " << s;
  }
};

}  // namespace

void RecursiveTargetConfigStringsToStream(
    const Target* target,
    const std::vector<std::string>& (ConfigValues::* getter)() const,
    std::ostream& out) {
  RecursiveTargetConfigToStream(target, getter, StringWriter(), out);
}
