// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "FrameTestHelpers.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "web/WebLocalFrameImpl.h"
#include "wtf/Forward.h"
#include "wtf/text/Base64.h"
#include <gtest/gtest.h>
#include <v8.h>

using WebCore::ScriptController;
using WebCore::ScriptSourceCode;
using WebCore::V8DOMActivityLogger;
using WebCore::toCoreStringWithUndefinedOrNullCheck;
using blink::FrameTestHelpers::WebViewHelper;
using blink::FrameTestHelpers::runPendingTasks;

namespace {

class TestActivityLogger : public V8DOMActivityLogger {
public:
    virtual ~TestActivityLogger() { }

    void logGetter(const String& apiName) OVERRIDE
    {
        m_loggedActivities.append(apiName);
    }

    void logSetter(const String& apiName, const v8::Handle<v8::Value>& newValue) OVERRIDE
    {
        m_loggedActivities.append(apiName + " | " + toCoreStringWithUndefinedOrNullCheck(newValue));
    }

    void logSetter(const String& apiName, const v8::Handle<v8::Value>& newValue, const v8::Handle<v8::Value>& oldValue) OVERRIDE
    {
        m_loggedActivities.append(apiName + " | " + toCoreStringWithUndefinedOrNullCheck(oldValue) + " | " + toCoreStringWithUndefinedOrNullCheck(newValue));
    }

    void logMethod(const String& apiName, int argc, const v8::Handle<v8::Value>* argv) OVERRIDE
    {
        String activityString = apiName;
        for (int i = 0; i  < argc; i++)
            activityString = activityString + " | " + toCoreStringWithUndefinedOrNullCheck(argv[i]);
        m_loggedActivities.append(activityString);
    }

    void logEvent(const String& eventName, int argc, const String* argv) OVERRIDE
    {
        String activityString = eventName;
        for (int i = 0; i  < argc; i++) {
            activityString = activityString + " | " + argv[i];
        }
        m_loggedActivities.append(activityString);
    }

    void clear() { m_loggedActivities.clear(); }
    bool verifyActivities(const Vector<String>& activities) const { return m_loggedActivities == activities; }

private:
    Vector<String> m_loggedActivities;
};

class ActivityLoggerTest : public testing::Test {
protected:
    ActivityLoggerTest()
    {
        m_activityLogger = new TestActivityLogger();
        V8DOMActivityLogger::setActivityLogger(isolatedWorldId, String(), adoptPtr(m_activityLogger));
        m_webViewHelper.initialize(true);
        m_scriptController = &m_webViewHelper.webViewImpl()->mainFrameImpl()->frame()->script();
    }

    void executeScriptInMainWorld(const String& script) const
    {
        m_scriptController->executeScriptInMainWorld(script);
        runPendingTasks();
    }

    void executeScriptInIsolatedWorld(const String& script) const
    {
        Vector<ScriptSourceCode> sources;
        sources.append(ScriptSourceCode(script));
        Vector<v8::Local<v8::Value> > results;
        m_scriptController->executeScriptInIsolatedWorld(isolatedWorldId, sources, extensionGroup, 0);
        runPendingTasks();
    }

    bool verifyActivities(const String& activities)
    {
        Vector<String> activityVector;
        activities.split("\n", activityVector);
        return m_activityLogger->verifyActivities(activityVector);
    }

private:
    static const int isolatedWorldId = 1;
    static const int extensionGroup = 0;

    WebViewHelper m_webViewHelper;
    ScriptController* m_scriptController;
    // TestActivityLogger is owned by a static table within V8DOMActivityLogger
    // and should be alive as long as not overwritten.
    TestActivityLogger* m_activityLogger;
};

TEST_F(ActivityLoggerTest, EventHandler)
{
    v8::HandleScope scope(v8::Isolate::GetCurrent());
    const char* code =
        "document.body.innerHTML = '<a onclick=\\\'do()\\\'>test</a>';"
        "document.body.onchange = function(){};"
        "document.body.setAttribute('onfocus', 'fnc()');"
        "document.body.addEventListener('onload', function(){});";
    const char* expectedActivities =
        "blinkAddEventListener | A | click\n"
        "blinkAddElement | a | \n"
        "blinkAddEventListener | BODY | change\n"
        "blinkAddEventListener | LocalDOMWindow | focus\n"
        "blinkAddEventListener | BODY | onload";
    executeScriptInMainWorld(code);
    ASSERT_TRUE(verifyActivities(""));
    executeScriptInIsolatedWorld(code);
    ASSERT_TRUE(verifyActivities(expectedActivities));
}

TEST_F(ActivityLoggerTest, ScriptElement)
{
    v8::HandleScope scope(v8::Isolate::GetCurrent());
    const char* code =
        "document.body.innerHTML = '<script src=\\\'data:text/html;charset=utf-8,\\\'></script>';"
        "document.body.innerHTML = '<script>console.log(\\\'test\\\')</script>';"
        "var script = document.createElement('script');"
        "document.body.appendChild(script);"
        "script = document.createElement('script');"
        "script.src = 'data:text/html;charset=utf-8,';"
        "document.body.appendChild(script);"
        "document.write('<body><script src=\\\'data:text/html;charset=utf-8,\\\'></script></body>');";
    const char* expectedActivities =
        "blinkAddElement | script | data:text/html;charset=utf-8,\n"
        "blinkAddElement | script | \n"
        "blinkAddElement | script | \n"
        "HTMLScriptElement.src | data:text/html;charset=utf-8,\n"
        "blinkAddElement | script | data:text/html;charset=utf-8,\n"
        "blinkAddElement | script | data:text/html;charset=utf-8,";
    executeScriptInMainWorld(code);
    ASSERT_TRUE(verifyActivities(""));
    executeScriptInIsolatedWorld(code);
    ASSERT_TRUE(verifyActivities(expectedActivities));
}

TEST_F(ActivityLoggerTest, IFrameElement)
{
    v8::HandleScope scope(v8::Isolate::GetCurrent());
    const char* code =
        "document.body.innerHTML = '<iframe src=\\\'data:text/html;charset=utf-8,\\\'></iframe>';"
        "document.body.innerHTML = '<iframe></iframe>';"
        "var iframe = document.createElement('iframe');"
        "document.body.appendChild(iframe);"
        "iframe = document.createElement('iframe');"
        "iframe.src = 'data:text/html;charset=utf-8,';"
        "document.body.appendChild(iframe);"
        "document.write('<body><iframe src=\\\'data:text/html;charset=utf-8,\\\'></iframe></body>');";
    const char* expectedActivities =
        "blinkAddElement | iframe | data:text/html;charset=utf-8,\n"
        "blinkAddElement | iframe | \n"
        "blinkAddElement | iframe | \n"
        "HTMLIFrameElement.src |  | data:text/html;charset=utf-8,\n"
        "blinkAddElement | iframe | data:text/html;charset=utf-8,\n"
        "blinkAddElement | iframe | data:text/html;charset=utf-8,";
    executeScriptInMainWorld(code);
    ASSERT_TRUE(verifyActivities(""));
    executeScriptInIsolatedWorld(code);
    ASSERT_TRUE(verifyActivities(expectedActivities));
}

TEST_F(ActivityLoggerTest, AnchorElement)
{
    v8::HandleScope scope(v8::Isolate::GetCurrent());
    const char* code =
        "document.body.innerHTML = '<a href=\\\'data:text/css;charset=utf-8,\\\'></a>';"
        "document.body.innerHTML = '<a></a>';"
        "var a = document.createElement('a');"
        "document.body.appendChild(a);"
        "a = document.createElement('a');"
        "a.href = 'data:text/css;charset=utf-8,';"
        "document.body.appendChild(a);"
        "document.write('<body><a href=\\\'data:text/css;charset=utf-8,\\\'></a></body>');";
    const char* expectedActivities =
        "blinkAddElement | a | data:text/css;charset=utf-8,\n"
        "blinkAddElement | a | \n"
        "blinkAddElement | a | \n"
        "HTMLAnchorElement.href |  | data:text/css;charset=utf-8,\n"
        "blinkAddElement | a | data:text/css;charset=utf-8,\n"
        "blinkAddElement | a | data:text/css;charset=utf-8,";
    executeScriptInMainWorld(code);
    ASSERT_TRUE(verifyActivities(""));
    executeScriptInIsolatedWorld(code);
    ASSERT_TRUE(verifyActivities(expectedActivities));
}

TEST_F(ActivityLoggerTest, LinkElement)
{
    v8::HandleScope scope(v8::Isolate::GetCurrent());
    const char* code =
        "document.body.innerHTML = '<link rel=\\\'stylesheet\\\' href=\\\'data:text/css;charset=utf-8,\\\'></link>';"
        "document.body.innerHTML = '<link></link>';"
        "var link = document.createElement('link');"
        "document.body.appendChild(link);"
        "link = document.createElement('link');"
        "link.rel = 'stylesheet';"
        "link.href = 'data:text/css;charset=utf-8,';"
        "document.body.appendChild(link);"
        "document.write('<body><link rel=\\\'stylesheet\\\' href=\\\'data:text/css;charset=utf-8,\\\'></link></body>');";
    const char* expectedActivities =
        "blinkAddElement | link | stylesheet | data:text/css;charset=utf-8,\n"
        "blinkAddElement | link |  | \n"
        "blinkAddElement | link |  | \n"
        "HTMLLinkElement.href | data:text/css;charset=utf-8,\n"
        "blinkAddElement | link | stylesheet | data:text/css;charset=utf-8,\n"
        "blinkAddElement | link | stylesheet | data:text/css;charset=utf-8,";
    executeScriptInMainWorld(code);
    ASSERT_TRUE(verifyActivities(""));
    executeScriptInIsolatedWorld(code);
    ASSERT_TRUE(verifyActivities(expectedActivities));
}

TEST_F(ActivityLoggerTest, InputElement)
{
    v8::HandleScope scope(v8::Isolate::GetCurrent());
    const char* code =
        "document.body.innerHTML = '<input type=\\\'submit\\\' formaction=\\\'data:text/html;charset=utf-8,\\\'></input>';"
        "document.body.innerHTML = '<input></input>';"
        "var input = document.createElement('input');"
        "document.body.appendChild(input);"
        "input = document.createElement('input');"
        "input.type = 'submit';"
        "input.formAction = 'data:text/html;charset=utf-8,';"
        "document.body.appendChild(input);"
        "document.write('<body><input type=\\\'submit\\\' formaction=\\\'data:text/html;charset=utf-8,\\\'></input></body>');";
    const char* expectedActivities =
        "blinkAddElement | input | submit | data:text/html;charset=utf-8,\n"
        "blinkAddElement | input |  | \n"
        "blinkAddElement | input |  | \n"
        "HTMLInputElement.formAction | data:text/html;charset=utf-8,\n"
        "blinkAddElement | input | submit | data:text/html;charset=utf-8,\n"
        "blinkAddElement | input | submit | data:text/html;charset=utf-8,";
    executeScriptInMainWorld(code);
    ASSERT_TRUE(verifyActivities(""));
    executeScriptInIsolatedWorld(code);
    ASSERT_TRUE(verifyActivities(expectedActivities));
}

TEST_F(ActivityLoggerTest, ButtonElement)
{
    v8::HandleScope scope(v8::Isolate::GetCurrent());
    const char* code =
        "document.body.innerHTML = '<button type=\\\'submit\\\' formmethod=\\\'post\\\' formaction=\\\'data:text/html;charset=utf-8,\\\'></input>';"
        "document.body.innerHTML = '<button></button>';"
        "var button = document.createElement('button');"
        "document.body.appendChild(button);"
        "button = document.createElement('button');"
        "button.type = 'submit';"
        "button.formMethod = 'post';"
        "button.formAction = 'data:text/html;charset=utf-8,';"
        "document.body.appendChild(button);"
        "document.write('<body><button type=\\\'submit\\\' formmethod=\\\'post\\\' formaction=\\\'data:text/html;charset=utf-8,\\\'></button></body>');";
    const char* expectedActivities =
        "blinkAddElement | button | submit | post | data:text/html;charset=utf-8,\n"
        "blinkAddElement | button |  |  | \n"
        "blinkAddElement | button |  |  | \n"
        "HTMLButtonElement.formAction | data:text/html;charset=utf-8,\n"
        "blinkAddElement | button | submit | post | data:text/html;charset=utf-8,\n"
        "blinkAddElement | button | submit | post | data:text/html;charset=utf-8,";
    executeScriptInMainWorld(code);
    ASSERT_TRUE(verifyActivities(""));
    executeScriptInIsolatedWorld(code);
    ASSERT_TRUE(verifyActivities(expectedActivities));
}

TEST_F(ActivityLoggerTest, FormElement)
{
    v8::HandleScope scope(v8::Isolate::GetCurrent());
    const char* code =
        "document.body.innerHTML = '<form method=\\\'post\\\' action=\\\'data:text/html;charset=utf-8,\\\'></form>';"
        "document.body.innerHTML = '<form></form>';"
        "var form = document.createElement('form');"
        "document.body.appendChild(form);"
        "form = document.createElement('form');"
        "form.method = 'post';"
        "form.action = 'data:text/html;charset=utf-8,';"
        "document.body.appendChild(form);"
        "document.write('<body><form method=\\\'post\\\' action=\\\'data:text/html;charset=utf-8,\\\'></form></body>');";
    const char* expectedActivities =
        "blinkAddElement | form | post | data:text/html;charset=utf-8,\n"
        "blinkAddElement | form |  | \n"
        "blinkAddElement | form |  | \n"
        "HTMLFormElement.action | data:text/html;charset=utf-8,\n"
        "blinkAddElement | form | post | data:text/html;charset=utf-8,\n"
        "blinkAddElement | form | post | data:text/html;charset=utf-8,";
    executeScriptInMainWorld(code);
    ASSERT_TRUE(verifyActivities(""));
    executeScriptInIsolatedWorld(code);
    ASSERT_TRUE(verifyActivities(expectedActivities));
}

} // namespace
