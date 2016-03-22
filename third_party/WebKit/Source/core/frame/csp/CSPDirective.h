// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSPDirective_h
#define CSPDirective_h

#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ContentSecurityPolicy;

class CSPDirective : public NoBaseWillBeGarbageCollectedFinalized<CSPDirective> {
    USING_FAST_MALLOC_WILL_BE_REMOVED(CSPDirective);
    WTF_MAKE_NONCOPYABLE(CSPDirective);
public:
    CSPDirective(const String& name, const String& value, ContentSecurityPolicy* policy)
        : m_name(name)
        , m_text(name + ' ' + value)
        , m_policy(policy)
    {
    }
    virtual ~CSPDirective() { }
    DEFINE_INLINE_VIRTUAL_TRACE() { visitor->trace(m_policy); }

    const String& text() const { return m_text; }

protected:
    ContentSecurityPolicy* policy() const { return m_policy; }

private:
    String m_name;
    String m_text;
    RawPtrWillBeMember<ContentSecurityPolicy> m_policy;
};

} // namespace blink

#endif
