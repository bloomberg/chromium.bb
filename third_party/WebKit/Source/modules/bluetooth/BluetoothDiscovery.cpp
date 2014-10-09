// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/BluetoothDiscovery.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

ScriptPromise BluetoothDiscovery::requestDevice(ScriptState* scriptState)
{
    // FIXME: Implement requestDevice: http://crbug.com/420284.
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

} // namespace blink
