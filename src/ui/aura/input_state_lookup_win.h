// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_INPUT_STATE_LOOKUP_WIN_H_
#define UI_AURA_INPUT_STATE_LOOKUP_WIN_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/aura/input_state_lookup.h"

namespace aura {

// Windows implementation of InputStateLookup.
class AURA_EXPORT InputStateLookupWin : public InputStateLookup {
 public:
  InputStateLookupWin();
  ~InputStateLookupWin() override;

  // InputStateLookup overrides:
  bool IsMouseButtonDown() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputStateLookupWin);
};

}  // namespace aura

#endif  // UI_AURA_INPUT_STATE_LOOKUP_WIN_H_
