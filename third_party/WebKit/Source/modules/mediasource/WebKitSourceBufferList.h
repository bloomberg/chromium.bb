/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebKitSourceBufferList_h
#define WebKitSourceBufferList_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/events/EventTarget.h"
#include "heap/Handle.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

class WebKitSourceBuffer;
class GenericEventQueue;

class WebKitSourceBufferList FINAL : public RefCountedWillBeRefCountedGarbageCollected<WebKitSourceBufferList>, public ScriptWrappable, public EventTargetWithInlineData {
    DEFINE_EVENT_TARGET_REFCOUNTING(RefCountedWillBeRefCountedGarbageCollected<WebKitSourceBufferList>);
public:
    static PassRefPtrWillBeRawPtr<WebKitSourceBufferList> create(ExecutionContext* context, GenericEventQueue* asyncEventQueue)
    {
        return adoptRefWillBeRefCountedGarbageCollected(new WebKitSourceBufferList(context, asyncEventQueue));
    }
    virtual ~WebKitSourceBufferList() { }

    unsigned long length() const;
    WebKitSourceBuffer* item(unsigned index) const;

    void add(PassRefPtrWillBeRawPtr<WebKitSourceBuffer>);
    bool remove(WebKitSourceBuffer*);
    void clear();

    // EventTarget interface
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    void trace(Visitor*);

private:
    WebKitSourceBufferList(ExecutionContext*, GenericEventQueue*);

    void createAndFireEvent(const AtomicString&);

    ExecutionContext* m_executionContext;
    GenericEventQueue* m_asyncEventQueue;

    WillBeHeapVector<RefPtrWillBeMember<WebKitSourceBuffer> > m_list;
};

} // namespace WebCore

#endif
