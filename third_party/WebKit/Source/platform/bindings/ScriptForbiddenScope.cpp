// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/bindings/ScriptForbiddenScope.h"

#include "platform/wtf/Assertions.h"

namespace blink {

unsigned ScriptForbiddenScope::script_forbidden_count_ = 0;

}  // namespace blink
