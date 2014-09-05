// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/webmidi/MIDIInputMap.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/modules/v8/V8MIDIInput.h"

namespace blink {

MIDIInputMap::MIDIInputMap(const HeapHashMap<String, Member<MIDIInput> >map)
    : MIDIPortMap<MIDIInput>(map)
{
    ScriptWrappable::init(this);
}

ScriptValue MIDIInputMap::getForBinding(ScriptState* scriptState, const String& id)
{
    MIDIInput* result = get(id);
    if (result)
        return ScriptValue(scriptState, toV8(result, scriptState->context()->Global(), scriptState->isolate()));
    return ScriptValue(scriptState, v8::Undefined(scriptState->isolate()));
}

} // namespace blink

