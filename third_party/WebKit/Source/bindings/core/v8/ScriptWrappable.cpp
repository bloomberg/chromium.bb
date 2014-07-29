// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/ScriptWrappable.h"

namespace blink {

struct SameSizeAsScriptWrappableBase { };

COMPILE_ASSERT(sizeof(ScriptWrappableBase) <= sizeof(SameSizeAsScriptWrappableBase), ScriptWrappableBase_should_stay_small);

struct SameSizeAsScriptWrappable : public ScriptWrappableBase {
    virtual ~SameSizeAsScriptWrappable() { }
    uintptr_t m_wrapperOrTypeInfo;
};

COMPILE_ASSERT(sizeof(ScriptWrappable) <= sizeof(SameSizeAsScriptWrappable), ScriptWrappable_should_stay_small);

} // namespace blink
