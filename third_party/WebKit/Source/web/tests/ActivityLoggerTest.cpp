// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "FrameTestHelpers.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8DOMActivityLogger.h"
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
    }

    void executeScriptInIsolatedWorld(const String& script) const
    {
        Vector<ScriptSourceCode> sources;
        sources.append(ScriptSourceCode(script));
        Vector<v8::Local<v8::Value> > results;
        m_scriptController->executeScriptInIsolatedWorld(isolatedWorldId, sources, extensionGroup, 0);
    }

    bool verifyActivities(const String& activities)
    {
        Vector<String> activityVector;
        activities.split(";", activityVector);
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
        "Element.innerHTML | <a onclick='do()'>test</a>;"
        "blinkAddEventListener | A | click;"
        "blinkAddEventListener | BODY | change;"
        "blinkAddEventListener | LocalDOMWindow | focus;"
        "blinkAddEventListener | BODY | onload";
    executeScriptInMainWorld(code);
    ASSERT_TRUE(verifyActivities(""));
    executeScriptInIsolatedWorld(code);
    ASSERT_TRUE(verifyActivities(expectedActivities));
}

} // namespace
