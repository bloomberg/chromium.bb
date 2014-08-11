// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformCredential_h
#define PlatformCredential_h

#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class WebPlatformCredential;

class PLATFORM_EXPORT PlatformCredential : public GarbageCollectedFinalized<PlatformCredential> {
    WTF_MAKE_NONCOPYABLE(PlatformCredential);
public:
    static PlatformCredential* create(const String& id, const String& name, const String& avatarURL);
    virtual ~PlatformCredential();

    const String& id() const { return m_id; }
    const String& name() const { return m_name; }
    const String& avatarURL() const { return m_avatarURL; }

    virtual void trace(Visitor*) { }

protected:
    PlatformCredential(const String& id, const String& name, const String& avatarURL);

private:
    String m_id;
    String m_name;
    String m_avatarURL;
};

} // namespace blink

#endif // PlatformCredential_h
