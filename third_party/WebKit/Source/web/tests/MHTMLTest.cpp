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

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Location.h"
#include "core/page/Page.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrame.h"
#include "public/web/WebView.h"
#include "web/tests/FrameTestHelpers.h"
#include "web/tests/URLTestHelpers.h"

#include <gtest/gtest.h>

using namespace blink;
using blink::Document;
using blink::LocalFrame;
using blink::Page;
using blink::KURL;
using blink::URLTestHelpers::toKURL;

namespace {

class MHTMLTest : public testing::Test {
public:
    MHTMLTest()
    {
    }

protected:
    virtual void SetUp()
    {
        m_helper.initialize();
    }

    virtual void TearDown()
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

    void registerMockedURLLoad(const std::string& url, const WebString& fileName)
    {
        URLTestHelpers::registerMockedURLLoad(toKURL(url), fileName, WebString::fromUTF8("mhtml/"), WebString::fromUTF8("text/html"));
    }

    void loadURLInTopFrame(const WebURL& url)
    {
        FrameTestHelpers::loadFrame(m_helper.webView()->mainFrame(), url.string().utf8().data());
    }

    Page* page() const { return m_helper.webViewImpl()->page(); }

private:
    FrameTestHelpers::WebViewHelper m_helper;
};

// Checks that the domain is set to the actual MHTML file, not the URL it was
// generated from.
TEST_F(MHTMLTest, CheckDomain)
{
    const char kFileURL[] = "file:///simple_test.mht";

    // Register the mocked frame and load it.
    WebURL url = toKURL(kFileURL);
    registerMockedURLLoad(kFileURL, WebString::fromUTF8("simple_test.mht"));
    loadURLInTopFrame(url);
    ASSERT_TRUE(page());
    LocalFrame* frame = toLocalFrame(page()->mainFrame());
    ASSERT_TRUE(frame);
    Document* document = frame->document();
    ASSERT_TRUE(document);

    EXPECT_STREQ(kFileURL, frame->domWindow()->location().href().ascii().data());

    blink::SecurityOrigin* origin = document->securityOrigin();
    EXPECT_STRNE("localhost", origin->domain().ascii().data());
}

}
