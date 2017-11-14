// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/SpellCheckTestBase.h"

#include "core/frame/LocalFrame.h"

namespace blink {

void SpellCheckTestBase::SetUp() {
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  SetupPageWithClients(&page_clients);
  GetPage().SetSpellCheckStatus(Page::SpellCheckStatus::kForcedOn);
}

SpellChecker& SpellCheckTestBase::GetSpellChecker() const {
  return GetFrame().GetSpellChecker();
}

}  // namespace blink
