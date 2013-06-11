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
#include "modules/webmidi/NavigatorWebMIDI.h"

#include "core/dom/Document.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/page/Frame.h"
#include "core/page/Navigator.h"
#include "modules/webmidi/MIDIAccessPromise.h"

namespace WebCore {

NavigatorWebMIDI::NavigatorWebMIDI(Frame* frame)
    : DOMWindowProperty(frame)
{
}

NavigatorWebMIDI::~NavigatorWebMIDI()
{
}

const char* NavigatorWebMIDI::supplementName()
{
    return "NavigatorWebMIDI";
}

NavigatorWebMIDI* NavigatorWebMIDI::from(Navigator* navigator)
{
    NavigatorWebMIDI* supplement = static_cast<NavigatorWebMIDI*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorWebMIDI(navigator->frame());
        provideTo(navigator, supplementName(), adoptPtr(supplement));
    }
    return supplement;
}

PassRefPtr<MIDIAccessPromise> NavigatorWebMIDI::requestMIDIAccess(Navigator* navigator, const Dictionary& options)
{
    return NavigatorWebMIDI::from(navigator)->requestMIDIAccess(options);
}

PassRefPtr<MIDIAccessPromise> NavigatorWebMIDI::requestMIDIAccess(const Dictionary& options)
{
    if (!frame())
        return 0;

    ScriptExecutionContext* context = frame()->document();
    ASSERT(context);

    RefPtr<MIDIAccessPromise> promise = MIDIAccessPromise::create(context, options);
    return promise;
}

} // namespace WebCore
