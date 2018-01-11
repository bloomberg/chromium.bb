// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/NavigatorLanguage.h"

#include "platform/Language.h"

namespace blink {

NavigatorLanguage::NavigatorLanguage() = default;

AtomicString NavigatorLanguage::language() {
  return AtomicString(languages().front());
}

bool NavigatorLanguage::hasLanguagesChanged() const {
  return languages_changed_;
}

void NavigatorLanguage::SetLanguagesChanged() {
  languages_changed_ = true;
}

}  // namespace blink
