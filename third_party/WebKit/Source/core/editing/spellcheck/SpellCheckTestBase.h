// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SpellCheckTestBase_h
#define SpellCheckTestBase_h

#include "core/editing/testing/EditingTestBase.h"

namespace blink {

class SpellChecker;

class SpellCheckTestBase : public EditingTestBase {
 protected:
  void SetUp() override;

  SpellChecker& GetSpellChecker() const;
};

}  // namespace blink

#endif  // SpellCheckTestBase_h
