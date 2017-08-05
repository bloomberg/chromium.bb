/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef V8EventListenerHelper_h
#define V8EventListenerHelper_h

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8EventListener.h"
#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"
#include "v8/include/v8.h"

namespace blink {

class V8EventListener;
class V8ErrorHandler;

enum ListenerLookupType {
  kListenerFindOnly,
  kListenerFindOrCreate,
};

// This is a container for V8EventListener objects that uses hidden properties
// of v8::Object to speed up lookups.
class V8EventListenerHelper {
  STATIC_ONLY(V8EventListenerHelper);

 public:
  CORE_EXPORT static V8EventListener* GetEventListener(ScriptState*,
                                                       v8::Local<v8::Value>,
                                                       bool is_attribute,
                                                       ListenerLookupType);

  CORE_EXPORT static V8ErrorHandler* EnsureErrorHandler(ScriptState*,
                                                        v8::Local<v8::Value>);
};

}  // namespace blink

#endif  // V8EventListenerHelper_h
