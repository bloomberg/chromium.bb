/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/webmidi/MIDIOutput.h"

namespace WebCore {

PassRefPtr<MIDIOutput> MIDIOutput::create(ScriptExecutionContext* context, const String& id, const String& manufacturer, const String& name, const String& version)
{
    return adoptRef(new MIDIOutput(context, id, manufacturer, name, version));
}

MIDIOutput::MIDIOutput(ScriptExecutionContext* context, const String& id, const String& manufacturer, const String& name, const String& version)
    : MIDIPort(context, id, manufacturer, name, MIDIPortTypeOutput, version)
{
    ScriptWrappable::init(this);
}

void MIDIOutput::send(Uint8Array* data, double timestamp)
{
    // FIXME: Implement MIDI protocol validation here. System exclusive
    // messages must be checked at the same time.
    // Actual sending operation will be implemented in core/platform/midi.
}

void MIDIOutput::send(Vector<unsigned>, double timestamp)
{
    // FIXME: Ditto. Implementation will be shared between these two send
    // functions.
}

} // namespace WebCore
