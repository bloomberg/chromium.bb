/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "core/page/PageConsole.h"

#include <stdio.h>
#include "DOMWindow.h"
#include "bindings/v8/ScriptCallStackFactory.h"
#include "bindings/v8/ScriptValue.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/inspector/InspectorConsoleInstrumentation.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/ScriptArguments.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/ConsoleTypes.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#include <wtf/UnusedParam.h>

#include "core/platform/chromium/TraceEvent.h"

namespace WebCore {

namespace {

int muteCount = 0;

}

PageConsole::PageConsole(Page* page)
    : m_page(page)
{
}

PageConsole::~PageConsole() { }

void PageConsole::addMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier, Document* document)
{
    String url;
    if (document)
        url = document->url().string();
    unsigned line = 0;
    if (document && document->parsing() && !document->isInDocumentWrite() && document->scriptableDocumentParser()) {
        ScriptableDocumentParser* parser = document->scriptableDocumentParser();
        if (!parser->isWaitingForScripts() && !parser->isExecutingScript())
            line = parser->lineNumber().oneBasedInt();
    }
    addMessage(source, level, message, url, line, 0, 0, requestIdentifier);
}

void PageConsole::addMessage(MessageSource source, MessageLevel level, const String& message, PassRefPtr<ScriptCallStack> callStack)
{
    addMessage(source, level, message, String(), 0, callStack, 0);
}

void PageConsole::addMessage(MessageSource source, MessageLevel level, const String& message, const String& url, unsigned lineNumber, PassRefPtr<ScriptCallStack> callStack, ScriptState* state, unsigned long requestIdentifier)
{
    if (muteCount && source != ConsoleAPIMessageSource)
        return;

    Page* page = this->page();
    if (!page)
        return;

    if (callStack)
        InspectorInstrumentation::addMessageToConsole(page, source, LogMessageType, level, message, callStack, requestIdentifier);
    else
        InspectorInstrumentation::addMessageToConsole(page, source, LogMessageType, level, message, url, lineNumber, state, requestIdentifier);

    if (source == CSSMessageSource)
        return;

    page->chrome()->client()->addMessageToConsole(source, level, message, lineNumber, url);
}

// static
void PageConsole::mute()
{
    muteCount++;
}

// static
void PageConsole::unmute()
{
    ASSERT(muteCount > 0);
    muteCount--;
}

} // namespace WebCore
