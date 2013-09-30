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
#include "WebMIDIClientMock.h"

#include "modules/webmidi/MIDIAccess.h"
#include "modules/webmidi/MIDIClientMock.h"
#include "public/web/WebMIDIPermissionRequest.h"

namespace WebKit {

WebMIDIClientMock::WebMIDIClientMock()
{
    m_clientMock.reset(new WebCore::MIDIClientMock());
}

WebMIDIClientMock* WebMIDIClientMock::create()
{
    return new WebMIDIClientMock();
}

void WebMIDIClientMock::setSysExPermission(bool allowed)
{
    m_clientMock->setSysExPermission(allowed);
}

void WebMIDIClientMock::resetMock()
{
    m_clientMock->resetMock();
}

void WebMIDIClientMock::requestSysExPermission(const WebMIDIPermissionRequest& request)
{
    m_clientMock->requestSysExPermission(request.midiAccess());
}

void WebMIDIClientMock::cancelSysExPermissionRequest(const WebMIDIPermissionRequest& request)
{
    m_clientMock->cancelSysExPermissionRequest(request.midiAccess());
}

void WebMIDIClientMock::reset()
{
    m_clientMock.reset(0);
}

} // WebKit
