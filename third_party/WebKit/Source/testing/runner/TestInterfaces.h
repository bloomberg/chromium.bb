/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef TestInterfaces_h
#define TestInterfaces_h

#include "public/platform/WebNonCopyable.h"
#include "public/testing/WebScopedPtr.h"

#include <vector>

#if defined(USE_DEFAULT_RENDER_THEME)
#include "WebTestThemeEngineMock.h"
#elif defined(WIN32)
#include "WebTestThemeEngineWin.h"
#elif defined(__APPLE__)
#include "WebTestThemeEngineMac.h"
#endif

namespace blink {
class WebFrame;
class WebThemeEngine;
class WebURL;
class WebView;
}

namespace WebTestRunner {

class AccessibilityController;
class EventSender;
class GamepadController;
class TestRunner;
class TextInputController;
class WebTestDelegate;
class WebTestProxyBase;

class TestInterfaces : public blink::WebNonCopyable {
public:
    TestInterfaces();
    ~TestInterfaces();

    void setWebView(blink::WebView*, WebTestProxyBase*);
    void setDelegate(WebTestDelegate*);
    void bindTo(blink::WebFrame*);
    void resetTestHelperControllers();
    void resetAll();
    void setTestIsRunning(bool);
    void configureForTestWithURL(const blink::WebURL&, bool generatePixels);

    void windowOpened(WebTestProxyBase*);
    void windowClosed(WebTestProxyBase*);

    AccessibilityController* accessibilityController();
    EventSender* eventSender();
    TestRunner* testRunner();
    WebTestDelegate* delegate();
    WebTestProxyBase* proxy();
    const std::vector<WebTestProxyBase*>& windowList();
    blink::WebThemeEngine* themeEngine();

private:
    WebScopedPtr<AccessibilityController> m_accessibilityController;
    WebScopedPtr<EventSender> m_eventSender;
    WebScopedPtr<GamepadController> m_gamepadController;
    WebScopedPtr<TextInputController> m_textInputController;
    WebScopedPtr<TestRunner> m_testRunner;
    WebTestDelegate* m_delegate;
    WebTestProxyBase* m_proxy;

    std::vector<WebTestProxyBase*> m_windowList;
#if defined(USE_DEFAULT_RENDER_THEME)
    WebScopedPtr<WebTestThemeEngineMock> m_themeEngine;
#elif defined(WIN32)
    WebScopedPtr<WebTestThemeEngineWin> m_themeEngine;
#elif defined(__APPLE__)
    WebScopedPtr<WebTestThemeEngineMac> m_themeEngine;
#endif
};

}

#endif // TestInterfaces_h
