/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef V0CustomElementScheduler_h
#define V0CustomElementScheduler_h

#include "core/html/custom/V0CustomElementCallbackQueue.h"
#include "core/html/custom/V0CustomElementLifecycleCallbacks.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class V0CustomElementDescriptor;
class V0CustomElementMicrotaskImportStep;
class V0CustomElementMicrotaskStep;
class V0CustomElementRegistrationContext;
class HTMLImportChild;

class V0CustomElementScheduler final
    : public GarbageCollected<V0CustomElementScheduler> {
 public:
  static void ScheduleCallback(V0CustomElementLifecycleCallbacks*,
                               Element*,
                               V0CustomElementLifecycleCallbacks::CallbackType);
  static void ScheduleAttributeChangedCallback(
      V0CustomElementLifecycleCallbacks*,
      Element*,
      const AtomicString& name,
      const AtomicString& old_value,
      const AtomicString& new_value);

  static void ResolveOrScheduleResolution(V0CustomElementRegistrationContext*,
                                          Element*,
                                          const V0CustomElementDescriptor&);
  static V0CustomElementMicrotaskImportStep* ScheduleImport(HTMLImportChild*);

  static void MicrotaskDispatcherDidFinish();
  static void CallbackDispatcherDidFinish();

 private:
  V0CustomElementScheduler() {}

  static void EnqueueMicrotaskStep(Document&,
                                   V0CustomElementMicrotaskStep*,
                                   bool import_is_sync = true);
};

}  // namespace blink

#endif  // V0CustomElementScheduler_h
