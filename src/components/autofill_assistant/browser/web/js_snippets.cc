// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/js_snippets.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace autofill_assistant {

JsSnippet::JsSnippet() = default;
JsSnippet::~JsSnippet() = default;
std::string JsSnippet::ToString() const {
  return base::JoinString(lines_, "\n");
}

void JsSnippet::AddLine(const std::string& line) {
  lines_.emplace_back(line);
}

void JsSnippet::AddLine(const std::vector<std::string>& line) {
  lines_.emplace_back(base::StrCat(line));
}

void AddReturnIfOnTop(JsSnippet* out,
                      const std::string& element_var,
                      const std::string& on_top,
                      const std::string& not_on_top,
                      const std::string& not_in_view) {
  // clang-format off
  out->AddLine({"const bounds = ", element_var, R"(.getBoundingClientRect();
  const x = bounds.x + bounds.width / 2;
  const y = bounds.y + bounds.height / 2;
  const targets = [)", element_var, R"(];
  const labels = )", element_var, R"(.labels;
  if (labels) {
    for (let i = 0; i < labels.length; i++) {
       targets.push(labels[i]);
    }
  }
  let root = document;
  while (root) {
    const atPoint = root.elementFromPoint(x, y);
    if (!atPoint) {
      return )", not_in_view, R"(;
    }
    for (const target of targets) {
      if (target === atPoint || target.contains(atPoint)) {
        return )", on_top , R"(;
      }
    }
    root = atPoint.shadowRoot;
  }
  return )", not_on_top, ";"});
  // clang-format on
}
}  // namespace autofill_assistant
