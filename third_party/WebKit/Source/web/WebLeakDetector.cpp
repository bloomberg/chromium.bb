/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "public/web/WebLeakDetector.h"

#include "bindings/v8/V8Binding.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/DOMWindow.h"
#include "core/inspector/InspectorCounters.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrame.h"

#include <v8.h>

using namespace WebCore;

namespace {

void cleanUpDOMObjects(blink::WebFrame* frame)
{
    v8::HandleScope handleScope(v8::Isolate::GetCurrent());
    v8::Local<v8::Context> context(frame->mainWorldScriptContext());
    v8::Context::Scope contextScope(context);

    // FIXME: HTML5 Notification should be closed because notification affects the result of number of DOM objects.

    ResourceFetcher* fetcher = currentDOMWindow(context->GetIsolate())->document()->fetcher();
    if (fetcher)
        fetcher->garbageCollectDocumentResources();

    memoryCache()->evictResources();

    v8::V8::LowMemoryNotification();
}

void numberOfDOMObjects(blink::WebFrame *frame, unsigned* numberOfLiveDocuments, unsigned* numberOfLiveNodes)
{
    v8::HandleScope handleScope(v8::Isolate::GetCurrent());
    v8::Local<v8::Context> context(frame->mainWorldScriptContext());
    v8::Context::Scope contextScope(context);

    *numberOfLiveDocuments = InspectorCounters::counterValue(InspectorCounters::DocumentCounter);
    *numberOfLiveNodes = InspectorCounters::counterValue(InspectorCounters::NodeCounter);
}

} // namespace

namespace blink {

void WebLeakDetector::collectGarbargeAndGetDOMCounts(WebLocalFrame* frame, unsigned* numberOfLiveDocuments, unsigned* numberOfLiveNodes)
{
    // FIXME: Count other DOM objects using WTF::dumpRefCountedInstanceCounts.
    cleanUpDOMObjects(frame);
    numberOfDOMObjects(frame, numberOfLiveDocuments, numberOfLiveNodes);
}

} // namespace blink
