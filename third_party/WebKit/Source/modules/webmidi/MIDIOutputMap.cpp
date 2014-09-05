// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/webmidi/MIDIOutputMap.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/modules/v8/V8MIDIOutput.h"

namespace blink {

MIDIOutputMap::MIDIOutputMap(HeapHashMap<String, Member<MIDIOutput> > map)
    : MIDIPortMap<MIDIOutput>(map)
{
    ScriptWrappable::init(this);
}

ScriptValue MIDIOutputMap::getForBinding(ScriptState* scriptState, const String& id)
{
    MIDIOutput* result = get(id);
    if (result)
        return ScriptValue(scriptState, toV8(result, scriptState->context()->Global(), scriptState->isolate()));
    return ScriptValue(scriptState, v8::Undefined(scriptState->isolate()));
}

} // namespace blink
