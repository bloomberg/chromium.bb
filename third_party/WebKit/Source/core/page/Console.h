/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Console_h
#define Console_h

#include "bindings/v8/ScriptState.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/inspector/ScriptProfile.h"
#include "core/page/DOMWindowProperty.h"
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Frame;
class MemoryInfo;
class Page;
class ScriptArguments;

typedef Vector<RefPtr<ScriptProfile> > ProfilesArray;

class Console : public ScriptWrappable, public RefCounted<Console>, public DOMWindowProperty {
public:
    static PassRefPtr<Console> create(Frame* frame) { return adoptRef(new Console(frame)); }
    virtual ~Console();

    void debug(ScriptState*, PassRefPtr<ScriptArguments>);
    void error(ScriptState*, PassRefPtr<ScriptArguments>);
    void info(ScriptState*, PassRefPtr<ScriptArguments>);
    void log(ScriptState*, PassRefPtr<ScriptArguments>);
    void clear(ScriptState*, PassRefPtr<ScriptArguments>);
    void warn(ScriptState*, PassRefPtr<ScriptArguments>);
    void dir(ScriptState*, PassRefPtr<ScriptArguments>);
    void dirxml(ScriptState*, PassRefPtr<ScriptArguments>);
    void table(ScriptState*, PassRefPtr<ScriptArguments>);
    void trace(ScriptState*, PassRefPtr<ScriptArguments>);
    void assertCondition(ScriptState*, PassRefPtr<ScriptArguments>, bool condition);
    void count(ScriptState*, PassRefPtr<ScriptArguments>);
    void markTimeline(PassRefPtr<ScriptArguments>);
    const ProfilesArray& profiles() const { return m_profiles; }
    void profile(const String&, ScriptState*);
    void profileEnd(const String&, ScriptState*);
    void time(const String&);
    void timeEnd(ScriptState*, const String&);
    void timeStamp(PassRefPtr<ScriptArguments>);
    void group(ScriptState*, PassRefPtr<ScriptArguments>);
    void groupCollapsed(ScriptState*, PassRefPtr<ScriptArguments>);
    void groupEnd();

    PassRefPtr<MemoryInfo> memory() const;

private:
    inline Page* page() const;

    explicit Console(Frame*);

    ProfilesArray m_profiles;
};

} // namespace WebCore

#endif // Console_h
