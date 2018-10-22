// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_ELEMENT_SELECTOR_H_
#define IOS_WEB_PUBLIC_TEST_ELEMENT_SELECTOR_H_

#include <string>

namespace web {
namespace test {

// An ElementSelector is used to generate the proper javascript to retrieve an
// element on a web page. It encapsulates the various means of finding an
// element and is intended to be passed around.
class ElementSelector {
 public:
  // Returns an ElementSelector to retrieve an element by ID.
  static const ElementSelector ElementSelectorId(const std::string element_id);

  // Returns an ElementSelector to retrieve an element by a CSS selector.
  static const ElementSelector ElementSelectorCss(
      const std::string css_selector);

  // Returns an ElementSelector to retrieve an element by a xpath query.
  static const ElementSelector ElementSelectorXPath(
      const std::string xpath_selector);

  ElementSelector(const ElementSelector& that) = default;
  ElementSelector(ElementSelector&& that) = default;
  ~ElementSelector() = default;

  // Returns the javascript to invoke on a page to retrieve the element.
  const std::string GetSelectorScript() const;

  // Returns a human readable description of the query.
  const std::string GetSelectorDescription() const;

 private:
  ElementSelector(const std::string&& script, const std::string&& description);

  const std::string script_;
  const std::string description_;
};

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_ELEMENT_SELECTOR_H_
