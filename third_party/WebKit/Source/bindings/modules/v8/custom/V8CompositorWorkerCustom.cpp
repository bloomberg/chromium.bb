// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/modules/v8/V8CompositorWorker.h"

#include "bindings/core/v8/PostMessage.h"
#include "bindings/core/v8/V8Binding.h"

namespace blink {

// TODO(sadrul): This stub should be replaced by generated code.
void V8CompositorWorker::postMessageMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    postMessageMethodCommon("CompositorWorker", V8CompositorWorker::toImpl(info.Holder()), info);
}

} // namespace blink
