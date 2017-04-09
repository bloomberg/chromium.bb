// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SpellCheckTestBase_h
#define SpellCheckTestBase_h

#include "core/editing/EditingTestBase.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"

namespace blink {

class SpellCheckTestBase : public EditingTestBase {
 protected:
  void SetUp() override;

 private:
  class DummySpellCheckerClient : public EmptySpellCheckerClient {
   public:
    virtual ~DummySpellCheckerClient() {}

    bool IsSpellCheckingEnabled() override { return true; }
  };

  std::unique_ptr<DummySpellCheckerClient> spell_checker_client_;
};

}  // namespace blink

#endif  // SpellCheckTestBase_h
