// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/sim/SimPage.h"

#include "core/page/FocusController.h"
#include "core/page/Page.h"

namespace blink {

SimPage::SimPage() : page_(nullptr) {}

SimPage::~SimPage() {}

void SimPage::SetPage(Page* page) {
  page_ = page;
}

void SimPage::SetFocused(bool value) {
  page_->GetFocusController().SetFocused(value);
}

bool SimPage::IsFocused() const {
  return page_->GetFocusController().IsFocused();
}

}  // namespace blink
