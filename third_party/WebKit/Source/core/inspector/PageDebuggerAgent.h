/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef PageDebuggerAgent_h
#define PageDebuggerAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorDebuggerAgent.h"

using blink::protocol::Runtime::ExceptionDetails;
using blink::protocol::Runtime::ScriptId;
using blink::protocol::Runtime::RemoteObject;

namespace blink {

class DocumentLoader;
class InspectedFrames;

class CORE_EXPORT PageDebuggerAgent final
    : public InspectorDebuggerAgent {
    WTF_MAKE_NONCOPYABLE(PageDebuggerAgent);
public:
    static PageDebuggerAgent* create(V8DebuggerAgent*, InspectedFrames*);
    ~PageDebuggerAgent() override;
    DECLARE_VIRTUAL_TRACE();

    void enable(ErrorString*) final;
    void disable(ErrorString*) final;
    void restore() final;

private:
    PageDebuggerAgent(V8DebuggerAgent*, InspectedFrames*);

    Member<InspectedFrames> m_inspectedFrames;
    HashMap<String, String> m_compiledScriptURLs;
};

} // namespace blink


#endif // !defined(PageDebuggerAgent_h)
