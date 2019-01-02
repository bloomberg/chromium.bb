// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/element_selector.h"

#include "base/strings/stringprintf.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace test {

// Static.
const ElementSelector ElementSelector::ElementSelectorId(
    const std::string element_id) {
  return ElementSelector(
      base::StringPrintf("document.getElementById('%s')", element_id.c_str()),
      base::StringPrintf("with ID %s", element_id.c_str()));
}

// Static.
const ElementSelector ElementSelector::ElementSelectorCss(
    const std::string css_selector) {
  const std::string script(base::StringPrintf("document.querySelector(\"%s\")",
                                              css_selector.c_str()));
  const std::string description(
      base::StringPrintf("with CSS selector '%s'", css_selector.c_str()));
  return ElementSelector(std::move(script), std::move(description));
}

// Static.
const ElementSelector ElementSelector::ElementSelectorXPath(
    const std::string xpath_selector) {
  const std::string script(base::StringPrintf(
      "document.evaluate(`%s`, document, "
      "null,XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue",
      xpath_selector.c_str()));
  const std::string description(
      base::StringPrintf("with xpath '%s'", xpath_selector.c_str()));
  return ElementSelector(std::move(script), std::move(description));
}

ElementSelector::ElementSelector(const std::string&& script,
                                 const std::string&& description)
    : script_(script), description_(description) {}

const std::string ElementSelector::GetSelectorScript() const {
  return script_;
}

const std::string ElementSelector::GetSelectorDescription() const {
  return description_;
}

}  // namespace test
}  // namespace web
