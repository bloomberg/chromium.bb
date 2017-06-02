// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/SpellCheckTestBase.h"

#include "core/frame/LocalFrame.h"

namespace blink {

void SpellCheckTestBase::SetUp() {
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  spell_checker_client_ = WTF::WrapUnique(new DummySpellCheckerClient);
  page_clients.spell_checker_client = spell_checker_client_.get();
  SetupPageWithClients(&page_clients);
}

SpellChecker& SpellCheckTestBase::GetSpellChecker() const {
  return GetFrame().GetSpellChecker();
}

}  // namespace blink
