// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/template_expressions.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "net/base/escape.h"

namespace {
const char kLeader[] = "$i18n";
const size_t kLeaderSize = base::size(kLeader) - 1;
const char kKeyOpen = '{';
const char kKeyClose = '}';
const char kHtmlTemplateStart[] = "_template: html`";
const size_t kHtmlTemplateStartSize = base::size(kHtmlTemplateStart) - 1;

// Currently only legacy _template: html`...`, syntax is supported.
enum HtmlTemplateType { NONE = 0, LEGACY = 1 };

struct TemplatePosition {
  HtmlTemplateType type;
  base::StringPiece::size_type position;
};

TemplatePosition FindHtmlTemplateStart(const base::StringPiece& source) {
  base::StringPiece::size_type found = source.find(kHtmlTemplateStart);
  HtmlTemplateType type = found == base::StringPiece::npos ? NONE : LEGACY;
  return {type, found + kHtmlTemplateStartSize};
}

TemplatePosition FindHtmlTemplateEnd(const base::StringPiece& source) {
  enum State { OPEN, IN_ESCAPE, IN_TICK };
  State state = OPEN;

  for (base::StringPiece::size_type i = 0; i < source.length(); i++) {
    if (state == IN_ESCAPE) {
      state = OPEN;  // Consume
      continue;
    }

    switch (source[i]) {
      case '\\':
        state = IN_ESCAPE;
        break;
      case '`':
        state = IN_TICK;
        break;
      case ',':
        if (state == IN_TICK)
          return {LEGACY, i - 1};
        FALLTHROUGH;
      default:
        state = OPEN;
    }
  }
  return {NONE, base::StringPiece::npos};
}

// Escape quotes and backslashes ('"\).
std::string PolymerParameterEscape(const std::string& in_string) {
  std::string out;
  out.reserve(in_string.size() * 2);
  for (const char c : in_string) {
    switch (c) {
      case '\\':
        out.append("\\\\");
        break;
      case '\'':
        out.append("\\'");
        break;
      case '"':
        out.append("&quot;");
        break;
      case ',':
        out.append("\\,");
        break;
      default:
        out += c;
    }
  }
  return out;
}

bool EscapeForJS(const std::string& in_string,
                 base::Optional<char> in_previous,
                 std::string* out_string) {
  out_string->reserve(in_string.size() * 2);
  bool last_was_dollar = in_previous && in_previous.value() == '$';
  for (const char c : in_string) {
    switch (c) {
      case '`':
        out_string->append("\\`");
        break;
      case '{':
        // Do not allow "${".
        if (last_was_dollar)
          return false;
        *out_string += c;
        break;
      default:
        *out_string += c;
    }
    last_was_dollar = c == '$';
  }
  return true;
}

bool ReplaceTemplateExpressionsInternal(
    base::StringPiece source,
    const ui::TemplateReplacements& replacements,
    bool is_javascript,
    std::string* formatted) {
  const size_t kValueLengthGuess = 16;
  formatted->reserve(source.length() + replacements.size() * kValueLengthGuess);
  // Two position markers are used as cursors through the |source|.
  // The |current_pos| will follow behind |next_pos|.
  size_t current_pos = 0;
  while (true) {
    size_t next_pos = source.find(kLeader, current_pos);

    if (next_pos == std::string::npos) {
      source.substr(current_pos).AppendToString(formatted);
      break;
    }

    source.substr(current_pos, next_pos - current_pos)
        .AppendToString(formatted);
    current_pos = next_pos + kLeaderSize;

    size_t context_end = source.find(kKeyOpen, current_pos);
    CHECK_NE(context_end, std::string::npos);
    std::string context;
    source.substr(current_pos, context_end - current_pos)
        .AppendToString(&context);
    current_pos = context_end + sizeof(kKeyOpen);

    size_t key_end = source.find(kKeyClose, current_pos);
    CHECK_NE(key_end, std::string::npos);

    std::string key =
        source.substr(current_pos, key_end - current_pos).as_string();
    CHECK(!key.empty());

    auto value = replacements.find(key);
    CHECK(value != replacements.end()) << "$i18n replacement key \"" << key
                                       << "\" not found";

    std::string replacement = value->second;
    if (is_javascript) {
      // Run JS escaping first.
      base::Optional<char> last = formatted->empty()
                                      ? base::nullopt
                                      : base::make_optional(formatted->back());
      std::string escaped_replacement;
      if (!EscapeForJS(replacement, last, &escaped_replacement))
        return false;
      replacement = escaped_replacement;
    }

    if (context.empty()) {
      // Make the replacement HTML safe.
      replacement = net::EscapeForHTML(replacement);
    } else if (context == "Raw") {
      // Pass the replacement through unchanged.
    } else if (context == "Polymer") {
      // Escape quotes and backslash for '$i18nPolymer{}' use (i.e. quoted).
      replacement = PolymerParameterEscape(replacement);
    } else {
      CHECK(false) << "Unknown context " << context;
    }

    formatted->append(replacement);

    current_pos = key_end + sizeof(kKeyClose);
  }
  return true;
}

}  // namespace

namespace ui {

void TemplateReplacementsFromDictionaryValue(
    const base::DictionaryValue& dictionary,
    TemplateReplacements* replacements) {
  for (base::DictionaryValue::Iterator it(dictionary); !it.IsAtEnd();
       it.Advance()) {
    std::string str_value;
    if (it.value().GetAsString(&str_value))
      (*replacements)[it.key()] = str_value;
  }
}

bool ReplaceTemplateExpressionsInJS(base::StringPiece source,
                                    const TemplateReplacements& replacements,
                                    std::string* formatted) {
  // Replacement is only done in JS for the contents of the HTML _template
  // string.
  TemplatePosition start_result = FindHtmlTemplateStart(source);
  if (start_result.type == NONE) {
    *formatted = source.as_string();
    return true;
  }

  // Only one template allowed per file.
  TemplatePosition second_start_result =
      FindHtmlTemplateStart(source.substr(start_result.position));
  if (second_start_result.type != NONE)
    return false;

  TemplatePosition end_result =
      FindHtmlTemplateEnd(source.substr(start_result.position));

  // Template must be properly terminated.
  if (start_result.type != end_result.type)
    return false;

  // Retrieve the HTML portion of the source.
  base::StringPiece html_template =
      source.substr(start_result.position, end_result.position);

  // Perform replacements with JS escaping.
  std::string formatted_html;
  if (!ReplaceTemplateExpressionsInternal(html_template, replacements, true,
                                          &formatted_html)) {
    return false;
  }

  // Re-assemble the JS file.
  *formatted =
      source.substr(0, start_result.position).as_string() + formatted_html +
      source.substr(start_result.position + end_result.position).as_string();
  return true;
}

std::string ReplaceTemplateExpressions(
    base::StringPiece source,
    const TemplateReplacements& replacements) {
  std::string formatted;
  ReplaceTemplateExpressionsInternal(source, replacements, false, &formatted);
  return formatted;
}
}  // namespace ui
