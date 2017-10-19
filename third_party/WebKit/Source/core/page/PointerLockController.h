/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef PointerLockController_h
#define PointerLockController_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class Element;
class Document;
class Page;
class WebMouseEvent;

class CORE_EXPORT PointerLockController final
    : public GarbageCollected<PointerLockController> {
  WTF_MAKE_NONCOPYABLE(PointerLockController);

 public:
  static PointerLockController* Create(Page*);

  void RequestPointerLock(Element* target);
  void RequestPointerUnlock();
  void ElementRemoved(Element*);
  void DocumentDetached(Document*);
  bool LockPending() const;
  Element* GetElement() const;

  void DidAcquirePointerLock();
  void DidNotAcquirePointerLock();
  void DidLosePointerLock();
  void DispatchLockedMouseEvent(const WebMouseEvent&,
                                const AtomicString& event_type);

  void Trace(blink::Visitor*);

 private:
  explicit PointerLockController(Page*);
  void ClearElement();
  void EnqueueEvent(const AtomicString& type, Element*);
  void EnqueueEvent(const AtomicString& type, Document*);

  Member<Page> page_;
  bool lock_pending_;
  Member<Element> element_;
  Member<Document> document_of_removed_element_while_waiting_for_unlock_;
};

}  // namespace blink

#endif  // PointerLockController_h
