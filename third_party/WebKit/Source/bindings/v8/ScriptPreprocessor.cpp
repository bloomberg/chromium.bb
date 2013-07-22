/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "bindings/v8/ScriptDebugServer.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "bindings/v8/V8WindowShell.h"
#include "core/page/DOMWindow.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/PageConsole.h"
#include "wtf/TemporaryChange.h"

namespace WebCore {

ScriptPreprocessor::ScriptPreprocessor(const String& preprocessorScript, ScriptController* controller, PageConsole* console)
    : m_isPreprocessing(false)
{
    v8::TryCatch tryCatch;
    tryCatch.SetVerbose(true);
    ScriptSourceCode preprocessor(preprocessorScript);
    Vector<ScriptSourceCode> sources;
    sources.append(preprocessor);
    Vector<ScriptValue> scriptResults;
    controller->executeScriptInIsolatedWorld(ScriptPreprocessorIsolatedWorldId, sources, DOMWrapperWorld::mainWorldExtensionGroup, &scriptResults);

    if (scriptResults.size() != 1) {
        console->addMessage(JSMessageSource, ErrorMessageLevel, "ScriptPreprocessor internal error, one ScriptSourceCode must give exactly one result.");
        return;
    }

    v8::Handle<v8::Value> preprocessorFunction = scriptResults[0].v8Value();
    if (!preprocessorFunction->IsFunction()) {
        console->addMessage(JSMessageSource, ErrorMessageLevel, "The preprocessor must compile to a function.");
        return;
    }

    RefPtr<DOMWrapperWorld> world = DOMWrapperWorld::ensureIsolatedWorld(ScriptPreprocessorIsolatedWorldId, DOMWrapperWorld::mainWorldExtensionGroup);
    V8WindowShell* isolatedWorldShell = controller->windowShell(world.get());
    v8::Local<v8::Context> context = isolatedWorldShell->context();
    m_isolate = context->GetIsolate();

    m_context.set(m_isolate, context);
    m_preprocessorFunction.set(m_isolate, v8::Handle<v8::Function>::Cast(preprocessorFunction));
}

String ScriptPreprocessor::preprocessSourceCode(const String& sourceCode, const String& sourceName)
{
    if (m_preprocessorFunction.isEmpty())
        return sourceCode;

    v8::HandleScope handleScope(m_isolate);
    v8::Context::Scope contextScope(m_context.newLocal(m_isolate));

    v8::Handle<v8::String> sourceCodeString = v8String(sourceCode, m_isolate);
    v8::Handle<v8::String> sourceNameString = v8String(sourceName, m_isolate);
    v8::Handle<v8::Value> argv[] = { sourceCodeString, sourceNameString };

    v8::TryCatch tryCatch;
    tryCatch.SetVerbose(true);
    TemporaryChange<bool> isPreprocessing(m_isPreprocessing, true);
    v8::Handle<v8::Value> resultValue = V8ScriptRunner::callAsFunction(m_preprocessorFunction.newLocal(m_isolate), m_context.newLocal(m_isolate)->Global(), WTF_ARRAY_LENGTH(argv), argv);

    if (!resultValue.IsEmpty() && resultValue->IsString())
        return toWebCoreStringWithNullCheck(resultValue);

    return sourceCode;
}

void ScriptPreprocessor::preprocessEval(ScriptDebugServer* debugServer, v8::Handle<v8::Object> eventData)
{
    if (m_isPreprocessing)
        return;

    v8::Local<v8::Context> debugContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debugContext);

    // <script> tag source and attribute value source are preprocessed before we enter V8.
    // Avoid preprocessing any internal scripts by processing only eval source in this V8 event handler.
    v8::Handle<v8::Value> argvEventData[] = { eventData };
    String typeInfo = toWebCoreStringWithUndefinedOrNullCheck(debugServer->callDebuggerMethod("getScriptCompilationTypeInfo", WTF_ARRAY_LENGTH(argvEventData), argvEventData));
    if (!typeInfo.startsWith("eval"))
        return;

    // The name and source are in the JS event data.
    String scriptName = toWebCoreStringWithUndefinedOrNullCheck(debugServer->callDebuggerMethod("getScriptName", WTF_ARRAY_LENGTH(argvEventData), argvEventData));
    String script = toWebCoreStringWithUndefinedOrNullCheck(debugServer->callDebuggerMethod("getScriptSource", WTF_ARRAY_LENGTH(argvEventData), argvEventData));

    String patchedScript = preprocessSourceCode(script, scriptName);

    v8::Handle<v8::Value> argvPatchedScript[] = { eventData, v8String(patchedScript, debugContext->GetIsolate()) };
    debugServer->callDebuggerMethod("setScriptSource", WTF_ARRAY_LENGTH(argvPatchedScript), argvPatchedScript);
}


} // namespace WebCore
