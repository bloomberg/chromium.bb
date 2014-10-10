// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformCredential_h
#define PlatformCredential_h

#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT PlatformCredential : public GarbageCollectedFinalized<PlatformCredential> {
    WTF_MAKE_NONCOPYABLE(PlatformCredential);
public:
    static PlatformCredential* create(const String& id, const String& name, const KURL& avatarURL);
    virtual ~PlatformCredential();

    const String& id() const { return m_id; }
    const String& name() const { return m_name; }
    const KURL& avatarURL() const { return m_avatarURL; }

    virtual bool isLocal() { return false; }
    virtual bool isFederated() { return false; }

    virtual void trace(Visitor*) { }

protected:
    PlatformCredential(const String& id, const String& name, const KURL& avatarURL);

private:
    String m_id;
    String m_name;
    KURL m_avatarURL;
};

} // namespace blink

#endif // PlatformCredential_h
