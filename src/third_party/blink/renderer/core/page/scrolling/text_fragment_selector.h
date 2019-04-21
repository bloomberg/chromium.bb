// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// TextFragmentSelector represents a single targetText=... selector of a
// TextFragmentAnchor, parsed into its components.
class CORE_EXPORT TextFragmentSelector final {
 public:
  static TextFragmentSelector Create(const String& target_text);

  enum SelectorType {
    // An exact selector on the string start_.
    kExact,
    // A range selector on a text range start_ to end_.
    kRange,
  };

  TextFragmentSelector(SelectorType type, String start, String end);
  ~TextFragmentSelector() = default;

  SelectorType Type() const { return type_; }
  String Start() const { return start_; }
  String End() const { return end_; }

 private:
  const SelectorType type_;
  String start_;
  String end_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_H_
