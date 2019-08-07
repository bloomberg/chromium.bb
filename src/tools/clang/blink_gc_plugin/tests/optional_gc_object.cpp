// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "optional_gc_object.h"

namespace blink {

void DisallowedUseOfUniquePtr() {
  base::Optional<Base> optional_base;
  (void)optional_base;

  base::Optional<Derived> optional_derived;
  (void)optional_derived;

  new base::Optional<Base>;
}

}  // namespace blink
