/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaKeys_h
#define MediaKeys_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/events/EventTarget.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Uint8Array.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ContentDecryptionModule;
class MediaKeySession;
class HTMLMediaElement;
class ExceptionState;

// References are held by JS and HTMLMediaElement.
// The ContentDecryptionModule has the same lifetime as this object.
// Maintains a reference to all MediaKeySessions created to ensure they live as
// long as this object unless explicitly close()'d.
class MediaKeys : public RefCounted<MediaKeys>, public ScriptWrappable {
public:
    static PassRefPtr<MediaKeys> create(const String& keySystem, ExceptionState&);
    ~MediaKeys();

    PassRefPtr<MediaKeySession> createSession(ScriptExecutionContext*, const String& mimeType, Uint8Array* initData, ExceptionState&);

    const String& keySystem() const { return m_keySystem; }

    void setMediaElement(HTMLMediaElement*);

protected:
    MediaKeys(const String& keySystem, PassOwnPtr<ContentDecryptionModule>);

    Vector<RefPtr<MediaKeySession> > m_sessions;

    HTMLMediaElement* m_mediaElement;
    const String m_keySystem;
    OwnPtr<ContentDecryptionModule> m_cdm;
};

}

#endif // MediaKeys_h
