// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_ALIASES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_ALIASES_H_

#include "base/types/strong_alias.h"

namespace autofill {

// TODO(crbug.com/1326518): Use strong aliases for other primitives in mojom
// files.

using TouchToFillEligible =
    base::StrongAlias<struct TouchToFillEligibleTag, bool>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_ALIASES_H_
