// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/testing/PrivateScriptTest.h"

#include "bindings/core/v8/PrivateScriptRunner.h"
#include "core/frame/LocalFrame.h"
#include <v8.h>

namespace blink {

PrivateScriptTest::PrivateScriptTest(LocalFrame* frame)
{
    v8::Handle<v8::Value> classObject = PrivateScriptRunner::installClassIfNeeded(frame, "PrivateScriptTest");
    RELEASE_ASSERT(!classObject.IsEmpty());
}

} // namespace blink
