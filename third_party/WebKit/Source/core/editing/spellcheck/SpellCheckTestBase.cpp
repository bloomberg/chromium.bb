// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/SpellCheckTestBase.h"

#include "core/frame/LocalFrame.h"
#include "core/loader/EmptyClients.h"
#include "public/web/WebTextCheckClient.h"

namespace blink {

namespace {

class EnabledTextCheckerClient : public WebTextCheckClient {
 public:
  EnabledTextCheckerClient() = default;
  ~EnabledTextCheckerClient() override = default;
  bool IsSpellCheckingEnabled() const override { return true; }
};

EnabledTextCheckerClient* GetEnabledTextCheckerClient() {
  DEFINE_STATIC_LOCAL(EnabledTextCheckerClient, client, ());
  return &client;
}

}  // namespace

void SpellCheckTestBase::SetUp() {
  EditingTestBase::SetUp();

  EmptyLocalFrameClient* frame_client =
      static_cast<EmptyLocalFrameClient*>(GetFrame().Client());
  frame_client->SetTextCheckerClientForTesting(GetEnabledTextCheckerClient());
}

SpellChecker& SpellCheckTestBase::GetSpellChecker() const {
  return GetFrame().GetSpellChecker();
}

}  // namespace blink
