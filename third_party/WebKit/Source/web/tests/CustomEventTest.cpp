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
#include "public/platform/Platform.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebDOMCustomEvent.h"
#include "v8.h"

#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKit;

namespace {

class TestListener : public EventListener {
public:
    virtual bool operator==(const EventListener& listener)
    {
        return true;
    }

    virtual void handleEvent(ScriptExecutionContext* context, Event* event)
    {
        EXPECT_EQ(event->type(), "blah");

        v8::Context::Scope scope(m_v8Context);
        v8::Isolate* isolate = m_v8Context->GetIsolate();
        v8::Handle<v8::Value> jsEvent = toV8(event, v8::Handle<v8::Object>(), isolate);

        EXPECT_EQ(jsEvent->ToObject()->Get(v8::String::New("detail")), v8::Boolean::New(true));
    }

    static PassRefPtr<TestListener> create(v8::Local<v8::Context> v8Context)
    {
        return adoptRef(new TestListener(v8Context));
    }

private:
    TestListener()
        : EventListener(JSEventListenerType)
    {
    }

    TestListener(v8::Local<v8::Context> v8Context)
        : EventListener(JSEventListenerType)
        , m_v8Context(v8Context)
    {
    }

    v8::Local<v8::Context> m_v8Context;
};

// Tests that a CustomEvent can be initialized with a 'detail' value that is a
// SerializedScriptValue, and that this value can be retrieved and read in an
// event listener. Initialization of a CustomEvent with a SerializedScriptValue
// is an internal API, so it cannot be tested with a LayoutTest.
TEST(CustomEventTest, InitWithSerializedScriptValue)
{
    const std::string baseURL = "http://www.test.com";
    const std::string path = "visible_iframe.html";

    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(baseURL.c_str()), WebString::fromUTF8(path.c_str()));
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(baseURL + path);
    WebFrameImpl* frame = toWebFrameImpl(webView->mainFrame());

    WebDOMEvent event = frame->frame()->document()->createEvent("CustomEvent", IGNORE_EXCEPTION);
    WebDOMCustomEvent customEvent = event.to<WebDOMCustomEvent>();

    v8::HandleScope handleScope(isolateForFrame(frame->frame()));
    customEvent.initCustomEvent("blah", false, false, WebSerializedScriptValue::serialize(v8::Boolean::New(true)));
    RefPtr<EventListener> listener = TestListener::create(frame->mainWorldScriptContext());
    frame->frame()->document()->addEventListener("blah", listener, false);
    frame->frame()->document()->dispatchEvent(event);

    Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    webView->close();
}

}
