// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/template_expressions.h"

#include <stddef.h>

#include "base/logging.h"

namespace {
const char kLeader[] = "$i18n{";
const size_t kLeaderSize = arraysize(kLeader) - 1;
const char kTail[] = "}";
const size_t kTailSize = arraysize(kTail) - 1;
}   // namespace

namespace ui {

std::string ReplaceTemplateExpressions(
    base::StringPiece format_string,
    const TemplateReplacements& substitutions) {
  std::string formatted;
  const size_t kValueLengthGuess = 16;
  formatted.reserve(format_string.length() +
                    substitutions.size() * kValueLengthGuess);
  base::StringPiece::const_iterator i = format_string.begin();
  while (i < format_string.end()) {
    if (base::StringPiece(i).starts_with(kLeader)) {
      size_t key_start = i + kLeaderSize - format_string.begin();
      size_t key_length = format_string.find(kTail, key_start);
      if (key_length == base::StringPiece::npos)
        NOTREACHED() << "TemplateExpression missing ending tag";
      key_length -= key_start;
      std::string key(format_string.begin() + key_start, key_length);
      DCHECK(!key.empty());
      const auto& replacement = substitutions.find(key);
      if (replacement != substitutions.end()) {
        formatted.append(replacement->second);
        i += kLeaderSize + key_length + kTailSize;
        continue;
      } else {
        NOTREACHED() << "TemplateExpression key not found: " << key;
      }
    }
    formatted.push_back(*i);
    ++i;
  }
  return formatted;
}

}  // namespace ui
