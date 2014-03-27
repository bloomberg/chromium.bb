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

#include "FrameTestHelpers.h"
#include "URLTestHelpers.h"
#include "V8Event.h"
#include "WebFrame.h"
#include "WebFrameImpl.h"
#include "WebView.h"
#include "WebViewImpl.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "bindings/v8/V8AbstractEventListener.h"
#include "public/platform/Platform.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebDOMCustomEvent.h"
#include "v8.h"

#include <gtest/gtest.h>

using namespace WebCore;
using namespace blink;

namespace {

class TestListener : public V8AbstractEventListener {
public:
    virtual bool operator==(const EventListener& listener)
    {
        return true;
    }

    virtual void handleEvent(ExecutionContext* context, Event* event)
    {
        EXPECT_EQ(event->type(), "blah");

        v8::Local<v8::Context> v8Context = WebCore::toV8Context(context, DOMWrapperWorld::mainWorld());
        v8::Isolate* isolate = v8Context->GetIsolate();
        v8::Context::Scope scope(v8Context);
        v8::Handle<v8::Value> jsEvent = toV8(event, v8::Handle<v8::Object>(), isolate);

        EXPECT_EQ(jsEvent->ToObject()->Get(v8::String::NewFromUtf8(isolate, "detail")), v8::Boolean::New(isolate, true));
    }

    static PassRefPtr<TestListener> create(v8::Isolate* isolate)
    {
        return adoptRef(new TestListener(isolate));
    }

private:
    TestListener(v8::Isolate* isolate)
        : V8AbstractEventListener(false, 0, isolate)
    {
    }

    virtual v8::Local<v8::Value> callListenerFunction(ExecutionContext*, v8::Handle<v8::Value> jsevent, Event*)
    {
        ASSERT_NOT_REACHED();
        return v8::Local<v8::Value>();
    }
};

// Tests that a CustomEvent can be initialized with a 'detail' value that is a
// SerializedScriptValue, and that this value can be retrieved and read in an
// event listener. Initialization of a CustomEvent with a SerializedScriptValue
// is an internal API, so it cannot be tested with a LayoutTest.
//
// Triggers crash in GC. http://crbug.com/317669
TEST(CustomEventTest, InitWithSerializedScriptValue)
{
    const std::string baseURL = "http://www.test.com";
    const std::string path = "visible_iframe.html";

    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(baseURL.c_str()), WebString::fromUTF8(path.c_str()));
    FrameTestHelpers::WebViewHelper webViewHelper;
    WebFrameImpl* frame = toWebFrameImpl(webViewHelper.initializeAndLoad(baseURL + path)->mainFrame());

    // FIXME: oilpan: Remove PassRefPtr<Event>() once we support PassRefPtrWillBeRawPtr in WebDOMEvent.
    WebDOMEvent event = PassRefPtr<Event>(frame->frame()->document()->createEvent("CustomEvent", IGNORE_EXCEPTION));
    WebDOMCustomEvent customEvent = event.to<WebDOMCustomEvent>();

    v8::Isolate* isolate = toIsolate(frame->frame());
    v8::HandleScope handleScope(isolate);
    customEvent.initCustomEvent("blah", false, false, WebSerializedScriptValue::serialize(v8::Boolean::New(isolate, true)));
    RefPtr<EventListener> listener = TestListener::create(isolate);
    frame->frame()->document()->addEventListener("blah", listener, false);
    frame->frame()->document()->dispatchEvent(event);

    Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
}

}
