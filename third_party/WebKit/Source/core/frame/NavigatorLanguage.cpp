// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/NavigatorLanguage.h"

#include "platform/Language.h"

namespace blink {

NavigatorLanguage::NavigatorLanguage() : languages_changed_(true) {}

AtomicString NavigatorLanguage::language() {
  return DefaultLanguage();
}

bool NavigatorLanguage::hasLanguagesChanged() {
  if (!languages_changed_)
    return false;

  languages_changed_ = false;
  return true;
}

void NavigatorLanguage::SetLanguagesChanged() {
  languages_changed_ = true;
}

}  // namespace blink
