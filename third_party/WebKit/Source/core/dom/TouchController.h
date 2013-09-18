/*
* Copyright (C) 2013 Google, Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*  * Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef TouchController_h
#define TouchController_h

#include "core/dom/DocumentLifecycleObserver.h"
#include "core/dom/Event.h"
#include "core/dom/Node.h"
#include "core/page/DOMWindowLifecycleObserver.h"
#include "core/platform/Supplementable.h"
#include "wtf/HashSet.h"

namespace WebCore {

typedef HashCountedSet<Node*> TouchEventTargetSet;

class Document;
class DOMWindow;

class TouchController : public Supplement<ScriptExecutionContext>, public DOMWindowLifecycleObserver, public DocumentLifecycleObserver {

public:
    virtual ~TouchController();

    static const char* supplementName();
    static TouchController* from(Document*);

    bool hasTouchEventHandlers() const { return m_touchEventTargets ? m_touchEventTargets->size() : false; }

    void didAddTouchEventHandler(Document*, Node*);
    void didRemoveTouchEventHandler(Document*, Node*);
    void didRemoveEventTargetNode(Document*, Node*);

    const TouchEventTargetSet* touchEventTargets() const { return m_touchEventTargets.get(); }

    // Inherited from DOMWindowLifecycleObserver
    virtual void didAddEventListener(DOMWindow*, const AtomicString&) OVERRIDE;
    virtual void didRemoveEventListener(DOMWindow*, const AtomicString&) OVERRIDE;
    virtual void didRemoveAllEventListeners(DOMWindow*) OVERRIDE;

    // Inherited from DocumentLifecycleObserver
    virtual void documentWasDetached() OVERRIDE;
    virtual void documentBeingDestroyed() OVERRIDE;

private:
    explicit TouchController(Document*);

    OwnPtr<TouchEventTargetSet> m_touchEventTargets;
};

} // namespace WebCore

#endif // TouchController_h
