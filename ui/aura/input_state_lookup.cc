// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/input_state_lookup.h"

#include "base/logging.h"

namespace aura {

// static
scoped_ptr<InputStateLookup> InputStateLookup::Create() {
#if !defined(OS_CHROMEOS)
  // TODO: need to create input_state_lookup_x11.
  NOTIMPLEMENTED();
#endif
  return scoped_ptr<InputStateLookup>();
}

}  // namespace aura
