// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/SpellCheckTestBase.h"

namespace blink {

void SpellCheckTestBase::SetUp() {
  Page::PageClients pageClients;
  fillWithEmptyClients(pageClients);
  m_spellCheckerClient = WTF::wrapUnique(new DummySpellCheckerClient);
  pageClients.spellCheckerClient = m_spellCheckerClient.get();
  setupPageWithClients(&pageClients);
}

}  // namespace blink
