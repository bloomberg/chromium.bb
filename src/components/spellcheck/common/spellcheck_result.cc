// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/common/spellcheck_result.h"
#include <vector>

SpellCheckResult::SpellCheckResult(Decoration d,
                                   int loc,
                                   int len,
                                   const std::vector<base::string16>& rep)
    : decoration(d), location(loc), length(len), replacements(rep) {}

SpellCheckResult::SpellCheckResult(Decoration d,
                                   int loc,
                                   int len,
                                   const base::string16& rep)
    : decoration(d),
      location(loc),
      length(len),
      replacements(std::vector<base::string16>({rep})) {}

SpellCheckResult::~SpellCheckResult() = default;

SpellCheckResult::SpellCheckResult(const SpellCheckResult&) = default;
