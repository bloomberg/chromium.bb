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

#include "config.h"
#include "FrameTestHelpers.h"

#include "URLTestHelpers.h"
#include "wtf/StdLibExtras.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebSettings.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "public/platform/Platform.h"
#include "public/platform/WebString.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebUnitTestSupport.h"

namespace WebKit {
namespace FrameTestHelpers {

namespace {

class QuitTask : public WebThread::Task {
public:
    virtual void run()
    {
        Platform::current()->currentThread()->exitRunLoop();
    }
};

WebFrameClient* defaultWebFrameClient()
{
    DEFINE_STATIC_LOCAL(WebFrameClient, client, ());
    return &client;
}

WebViewClient* defaultWebViewClient()
{
    DEFINE_STATIC_LOCAL(WebViewClient,  client, ());
    return &client;
}

} // namespace

void loadFrame(WebFrame* frame, const std::string& url)
{
    WebURLRequest urlRequest;
    urlRequest.initialize();
    urlRequest.setURL(URLTestHelpers::toKURL(url));
    frame->loadRequest(urlRequest);
}

void runPendingTasks()
{
    Platform::current()->currentThread()->postTask(new QuitTask);
    Platform::current()->currentThread()->enterRunLoop();
}

WebViewHelper::WebViewHelper()
    : m_mainFrame(0)
    , m_webView(0)
{
}

WebViewHelper::~WebViewHelper()
{
    reset();
}

WebViewImpl* WebViewHelper::initialize(bool enableJavascript, WebFrameClient* webFrameClient, WebViewClient* webViewClient, void (*updateSettingsFunc)(WebSettings*))
{
    reset();

    if (!webFrameClient)
        webFrameClient = defaultWebFrameClient();
    if (!webViewClient)
        webViewClient = defaultWebViewClient();
    m_webView = WebViewImpl::create(webViewClient);
    m_webView->settings()->setJavaScriptEnabled(enableJavascript);
    if (updateSettingsFunc) {
        updateSettingsFunc(m_webView->settings());
    } else {
        m_webView->settings()->setDeviceSupportsMouse(false);
        m_webView->settings()->setForceCompositingMode(true);
    }

    m_mainFrame = WebFrameImpl::create(webFrameClient);
    m_webView->setMainFrame(m_mainFrame);

    return m_webView;
}

WebViewImpl* WebViewHelper::initializeAndLoad(const std::string& url, bool enableJavascript, WebFrameClient* webFrameClient, WebViewClient* webViewClient, void (*updateSettingsFunc)(WebSettings*))
{
    initialize(enableJavascript, webFrameClient, webViewClient, updateSettingsFunc);

    loadFrame(webView()->mainFrame(), url);
    Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();

    return webViewImpl();
}

void WebViewHelper::reset()
{
    if (m_webView) {
        m_webView->close();
        m_webView = 0;
    }
    if (m_mainFrame) {
        m_mainFrame->close();
        m_mainFrame = 0;
    }
}

} // namespace FrameTestHelpers
} // namespace WebKit
