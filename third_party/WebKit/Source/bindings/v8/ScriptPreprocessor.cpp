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

#include "config.h"
#include "bindings/v8/ScriptPreprocessor.h"

#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "core/page/PageConsole.h"
#include "wtf/TemporaryChange.h"

namespace WebCore {

ScriptPreprocessor::ScriptPreprocessor(const ScriptSourceCode& preprocessorSourceCode, ScriptController* controller, PageConsole& console)
    : m_isPreprocessing(false)
{
    v8::TryCatch tryCatch;
    tryCatch.SetVerbose(true);
    Vector<ScriptSourceCode> sources;
    sources.append(preprocessorSourceCode);
    Vector<ScriptValue> scriptResults;
    controller->executeScriptInIsolatedWorld(ScriptPreprocessorIsolatedWorldId, sources, DOMWrapperWorld::mainWorldExtensionGroup, &scriptResults);

    if (scriptResults.size() != 1) {
        console.addMessage(JSMessageSource, ErrorMessageLevel, "ScriptPreprocessor internal error, one ScriptSourceCode must give exactly one result.");
        return;
    }

    ScriptValue preprocessorFunction = scriptResults[0];
    if (!preprocessorFunction.isFunction()) {
        console.addMessage(JSMessageSource, ErrorMessageLevel, "The preprocessor must compile to a function.");
        return;
    }

    m_world = DOMWrapperWorld::ensureIsolatedWorld(ScriptPreprocessorIsolatedWorldId, DOMWrapperWorld::mainWorldExtensionGroup);
    v8::Local<v8::Context> context = m_world->context(controller);
    m_isolate = context->GetIsolate();

    m_context.set(m_isolate, context);
    m_preprocessorFunction.set(m_isolate, v8::Handle<v8::Function>::Cast(preprocessorFunction.v8Value()));
}

String ScriptPreprocessor::preprocessSourceCode(const String& sourceCode, const String& sourceName)
{
    if (!isValid())
        return sourceCode;

    return preprocessSourceCode(sourceCode, sourceName, v8::Undefined());
}

String ScriptPreprocessor::preprocessSourceCode(const String& sourceCode, const String& sourceName, const String& functionName)
{
    if (!isValid())
        return sourceCode;

    v8::Handle<v8::String> functionNameString = v8String(functionName, m_isolate);
    return preprocessSourceCode(sourceCode, sourceName, functionNameString);
}

String ScriptPreprocessor::preprocessSourceCode(const String& sourceCode, const String& sourceName, v8::Handle<v8::Value> functionName)
{
    if (!isValid())
        return sourceCode;

    v8::HandleScope handleScope(m_isolate);
    v8::Context::Scope contextScope(m_context.newLocal(m_isolate));

    v8::Handle<v8::String> sourceCodeString = v8String(sourceCode, m_isolate);
    v8::Handle<v8::String> sourceNameString = v8String(sourceName, m_isolate);
    v8::Handle<v8::Value> argv[] = { sourceCodeString, sourceNameString, functionName};

    v8::TryCatch tryCatch;
    tryCatch.SetVerbose(true);
    TemporaryChange<bool> isPreprocessing(m_isPreprocessing, true);
    v8::Handle<v8::Value> resultValue = V8ScriptRunner::callAsFunction(m_preprocessorFunction.newLocal(m_isolate), m_context.newLocal(m_isolate)->Global(), WTF_ARRAY_LENGTH(argv), argv);

    if (!resultValue.IsEmpty() && resultValue->IsString())
        return toWebCoreStringWithNullCheck(resultValue);

    return sourceCode;
}

} // namespace WebCore
