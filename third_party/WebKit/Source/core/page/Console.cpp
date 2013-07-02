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

#include "config.h"
#include "core/page/Console.h"

#include "bindings/v8/ScriptCallStackFactory.h"
#include "bindings/v8/ScriptProfiler.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/inspector/InspectorConsoleInstrumentation.h"
#include "core/inspector/ScriptArguments.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/inspector/ScriptProfile.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/ConsoleTypes.h"
#include "core/page/Frame.h"
#include "core/page/MemoryInfo.h"
#include "core/page/Page.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

#include "core/platform/chromium/TraceEvent.h"

namespace WebCore {

Console::Console(Frame* frame)
    : DOMWindowProperty(frame)
{
    ScriptWrappable::init(this);
}

Console::~Console()
{
}

static void internalAddMessage(Page* page, MessageType type, MessageLevel level, ScriptState* state, PassRefPtr<ScriptArguments> prpArguments, bool acceptNoArguments = false, bool printTrace = false)
{
    RefPtr<ScriptArguments> arguments = prpArguments;

    if (!page)
        return;

    if (!acceptNoArguments && !arguments->argumentCount())
        return;

    size_t stackSize = printTrace ? ScriptCallStack::maxCallStackSizeToCapture : 1;
    RefPtr<ScriptCallStack> callStack(createScriptCallStack(state, stackSize));
    const ScriptCallFrame& lastCaller = callStack->at(0);

    String message;
    bool gotMessage = arguments->getFirstArgumentAsString(message);
    InspectorInstrumentation::addMessageToConsole(page, ConsoleAPIMessageSource, type, level, message, state, arguments);

    if (gotMessage)
        page->chrome().client()->addMessageToConsole(ConsoleAPIMessageSource, type, level, message, lastCaller.lineNumber(), lastCaller.sourceURL());
}

void Console::debug(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), LogMessageType, DebugMessageLevel, state, arguments);
}

void Console::error(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), LogMessageType, ErrorMessageLevel, state, arguments);
}

void Console::info(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    log(state, arguments);
}

void Console::log(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), LogMessageType, LogMessageLevel, state, arguments);
}

void Console::warn(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), LogMessageType, WarningMessageLevel, state, arguments);
}

void Console::dir(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), DirMessageType, LogMessageLevel, state, arguments);
}

void Console::dirxml(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), DirXMLMessageType, LogMessageLevel, state, arguments);
}

void Console::table(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), TableMessageType, LogMessageLevel, state, arguments);
}

void Console::clear(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), ClearMessageType, LogMessageLevel, state, arguments, true);
}

void Console::trace(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), TraceMessageType, LogMessageLevel, state, arguments, true, true);
}

void Console::assertCondition(ScriptState* state, PassRefPtr<ScriptArguments> arguments, bool condition)
{
    if (condition)
        return;

    internalAddMessage(page(), AssertMessageType, ErrorMessageLevel, state, arguments, true);
}

void Console::count(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::consoleCount(page(), state, arguments);
}

void Console::markTimeline(PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::consoleTimeStamp(m_frame, arguments);
}


void Console::profile(ScriptState* state, const String& title)
{
    Page* page = this->page();
    if (!page)
        return;

    // FIXME: log a console message when profiling is disabled.
    if (!InspectorInstrumentation::profilerEnabled(page))
        return;

    String resolvedTitle = title;
    if (title.isNull()) // no title so give it the next user initiated profile title.
        resolvedTitle = InspectorInstrumentation::getCurrentUserInitiatedProfileName(page, true);

    ScriptProfiler::start(resolvedTitle);

    RefPtr<ScriptCallStack> callStack(createScriptCallStack(state, 1));
    const ScriptCallFrame& lastCaller = callStack->at(0);
    InspectorInstrumentation::addStartProfilingMessageToConsole(page, resolvedTitle, lastCaller.lineNumber(), lastCaller.sourceURL());
}

void Console::profileEnd(ScriptState* state, const String& title)
{
    Page* page = this->page();
    if (!page)
        return;

    if (!InspectorInstrumentation::profilerEnabled(page))
        return;

    RefPtr<ScriptProfile> profile = ScriptProfiler::stop(title);
    if (!profile)
        return;

    RefPtr<ScriptCallStack> callStack(createScriptCallStack(state, 1));
    InspectorInstrumentation::addProfile(page, profile, callStack);
}


void Console::time(const String& title)
{
    InspectorInstrumentation::startConsoleTiming(m_frame, title);
    TRACE_EVENT_COPY_ASYNC_BEGIN0("webkit", title.utf8().data(), this);
}

void Console::timeEnd(ScriptState* state, const String& title)
{
    TRACE_EVENT_COPY_ASYNC_END0("webkit", title.utf8().data(), this);
    RefPtr<ScriptCallStack> callStack(createScriptCallStackForConsole(state));
    InspectorInstrumentation::stopConsoleTiming(m_frame, title, callStack.release());
}

void Console::timeStamp(PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::consoleTimeStamp(m_frame, arguments);
}

void Console::group(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::addMessageToConsole(page(), ConsoleAPIMessageSource, StartGroupMessageType, LogMessageLevel, String(), state, arguments);
}

void Console::groupCollapsed(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::addMessageToConsole(page(), ConsoleAPIMessageSource, StartGroupCollapsedMessageType, LogMessageLevel, String(), state, arguments);
}

void Console::groupEnd()
{
    InspectorInstrumentation::addMessageToConsole(page(), ConsoleAPIMessageSource, EndGroupMessageType, LogMessageLevel, String(), String(), 0, 0);
}

PassRefPtr<MemoryInfo> Console::memory() const
{
    // FIXME: Because we create a new object here each time,
    // console.memory !== console.memory, which seems wrong.
    return MemoryInfo::create(m_frame);
}

Page* Console::page() const
{
    if (!m_frame)
        return 0;
    return m_frame->page();
}

} // namespace WebCore
