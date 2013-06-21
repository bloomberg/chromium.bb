/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef V8CustomElementCallback_h
#define V8CustomElementCallback_h

#include "bindings/v8/ActiveDOMCallback.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScopedPersistent.h"
#include "core/dom/CustomElementCallback.h"
#include <v8.h>

namespace WebCore {

class Element;
class ScriptExecutionContext;

class V8CustomElementCallback : public CustomElementCallback, ActiveDOMCallback {
public:
    static PassRefPtr<V8CustomElementCallback> create(ScriptExecutionContext*, v8::Handle<v8::Object> owner, v8::Handle<v8::Function> ready);

    virtual ~V8CustomElementCallback() { }

private:
    V8CustomElementCallback(ScriptExecutionContext*, v8::Handle<v8::Function> ready);

    virtual void ready(Element*) OVERRIDE;

    RefPtr<DOMWrapperWorld> m_world;
    ScopedPersistent<v8::Function> m_ready;
};

}

#endif // CustomElementCallback_h
