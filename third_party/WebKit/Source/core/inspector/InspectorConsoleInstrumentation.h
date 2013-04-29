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

#ifndef InspectorConsoleInstrumentation_h
#define InspectorConsoleInstrumentation_h

#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ScriptArguments.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/inspector/ScriptProfile.h"
#include <wtf/PassRefPtr.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

namespace InspectorInstrumentation {

void addMessageToConsoleImpl(InstrumentingAgents*, MessageSource, MessageType, MessageLevel, const String& message, ScriptState*, PassRefPtr<ScriptArguments>, unsigned long requestIdentifier);
void addMessageToConsoleImpl(InstrumentingAgents*, MessageSource, MessageType, MessageLevel, const String& message, const String& scriptId, unsigned lineNumber, ScriptState*, unsigned long requestIdentifier);
// FIXME: Remove once we no longer generate stacks outside of Inspector.
void addMessageToConsoleImpl(InstrumentingAgents*, MessageSource, MessageType, MessageLevel, const String& message, PassRefPtr<ScriptCallStack>, unsigned long requestIdentifier);
void consoleCountImpl(InstrumentingAgents*, ScriptState*, PassRefPtr<ScriptArguments>);
void startConsoleTimingImpl(InstrumentingAgents*, Frame*, const String& title);
void stopConsoleTimingImpl(InstrumentingAgents*, Frame*, const String& title, PassRefPtr<ScriptCallStack>);
void consoleTimeStampImpl(InstrumentingAgents*, Frame*, PassRefPtr<ScriptArguments>);
void addStartProfilingMessageToConsoleImpl(InstrumentingAgents*, const String& title, unsigned lineNumber, const String& sourceURL);
void addProfileImpl(InstrumentingAgents*, RefPtr<ScriptProfile>, PassRefPtr<ScriptCallStack>);

inline void addMessageToConsole(Page* page, MessageSource source, MessageType type, MessageLevel level, const String& message, PassRefPtr<ScriptCallStack> callStack, unsigned long requestIdentifier = 0)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        addMessageToConsoleImpl(instrumentingAgents, source, type, level, message, callStack, requestIdentifier);
}

inline void addMessageToConsole(Page* page, MessageSource source, MessageType type, MessageLevel level, const String& message, ScriptState* state, PassRefPtr<ScriptArguments> arguments, unsigned long requestIdentifier = 0)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        addMessageToConsoleImpl(instrumentingAgents, source, type, level, message, state, arguments, requestIdentifier);
}

inline void addMessageToConsole(Page* page, MessageSource source, MessageType type, MessageLevel level, const String& message, const String& scriptId, unsigned lineNumber, ScriptState* state = 0, unsigned long requestIdentifier = 0)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        addMessageToConsoleImpl(instrumentingAgents, source, type, level, message, scriptId, lineNumber, state, requestIdentifier);
}

// FIXME: Convert to ScriptArguments to match non-worker context.
inline void addMessageToConsole(WorkerContext* workerContext, MessageSource source, MessageType type, MessageLevel level, const String& message, PassRefPtr<ScriptCallStack> callStack, unsigned long requestIdentifier = 0)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForWorkerContext(workerContext))
        addMessageToConsoleImpl(instrumentingAgents, source, type, level, message, callStack, requestIdentifier);
}

inline void addMessageToConsole(WorkerContext* workerContext, MessageSource source, MessageType type, MessageLevel level, const String& message, const String& scriptId, unsigned lineNumber, ScriptState* state, unsigned long requestIdentifier = 0)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForWorkerContext(workerContext))
        addMessageToConsoleImpl(instrumentingAgents, source, type, level, message, scriptId, lineNumber, state, requestIdentifier);
}

inline void consoleCount(Page* page, ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        consoleCountImpl(instrumentingAgents, state, arguments);
}

inline void startConsoleTiming(Frame* frame, const String& title)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        startConsoleTimingImpl(instrumentingAgents, frame, title);
}

inline void stopConsoleTiming(Frame* frame, const String& title, PassRefPtr<ScriptCallStack> stack)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        stopConsoleTimingImpl(instrumentingAgents, frame, title, stack);
}

inline void consoleTimeStamp(Frame* frame, PassRefPtr<ScriptArguments> arguments)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        consoleTimeStampImpl(instrumentingAgents, frame, arguments);
}

inline void addStartProfilingMessageToConsole(Page* page, const String& title, unsigned lineNumber, const String& sourceURL)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        addStartProfilingMessageToConsoleImpl(instrumentingAgents, title, lineNumber, sourceURL);
}

inline void addProfile(Page* page, RefPtr<ScriptProfile> profile, PassRefPtr<ScriptCallStack> callStack)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        addProfileImpl(instrumentingAgents, profile, callStack);
}

} // namespace InspectorInstrumentation

} // namespace WebCore

#endif // !defined(InspectorConsoleInstrumentation_h)
