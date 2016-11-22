// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/tests/sim/SimPage.h"

#include "core/page/FocusController.h"
#include "core/page/Page.h"

namespace blink {

SimPage::SimPage() : m_page(nullptr) {}

SimPage::~SimPage() {}

void SimPage::setPage(Page* page) {
  m_page = page;
}

void SimPage::setFocused(bool value) {
  m_page->focusController().setFocused(value);
}

bool SimPage::isFocused() const {
  return m_page->focusController().isFocused();
}

}  // namespace blink
