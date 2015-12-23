// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/template_expressions.h"

#include <stddef.h>

#include "base/logging.h"

namespace ui {

std::string ReplaceTemplateExpressions(
    base::StringPiece format_string,
    const std::map<base::StringPiece, std::string>& substitutions) {
  std::string formatted;
  const size_t kValueLengthGuess = 16;
  formatted.reserve(format_string.length() +
                    substitutions.size() * kValueLengthGuess);
  base::StringPiece::const_iterator i = format_string.begin();
  while (i < format_string.end()) {
    if (*i == '$' && i + 2 < format_string.end() && i[1] == '{' &&
        i[2] != '}') {
      size_t key_start = i + strlen("${") - format_string.begin();
      size_t key_length = format_string.find('}', key_start);
      if (key_length == base::StringPiece::npos)
        NOTREACHED() << "TemplateExpression missing ending brace '}'";
      key_length -= key_start;
      base::StringPiece key(format_string.begin() + key_start, key_length);
      const auto& replacement = substitutions.find(key);
      if (replacement != substitutions.end()) {
        formatted.append(replacement->second);
        i += strlen("${") + key_length + strlen("}");
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
