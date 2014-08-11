// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformLocalCredential_h
#define PlatformLocalCredential_h

#include "platform/credentialmanager/PlatformCredential.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT PlatformLocalCredential FINAL : public PlatformCredential {
    WTF_MAKE_NONCOPYABLE(PlatformLocalCredential);
public:
    static PlatformLocalCredential* create(const String& id, const String& name, const String& avatarURL, const String& password);
    virtual ~PlatformLocalCredential();

    const String& password() const { return m_password; }

private:
    PlatformLocalCredential(const String& id, const String& name, const String& avatarURL, const String& password);

    String m_password;
};

} // namespace blink

#endif // PlatformLocalCredential_h
