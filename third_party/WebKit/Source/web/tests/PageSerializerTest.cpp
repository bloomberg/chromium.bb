/*
 * Copyright (c) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/page/Page.h"
#include "core/page/PageSerializer.h"
#include "core/testing/URLTestHelpers.h"
#include "platform/SerializedResource.h"
#include "public/platform/Platform.h"
#include "public/platform/WebString.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebSettings.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "wtf/Vector.h"
#include <gtest/gtest.h>

using namespace blink;
using blink::URLTestHelpers::toKURL;
using blink::URLTestHelpers::registerMockedURLLoad;

namespace {

class PageSerializerTest : public testing::Test {
public:
    PageSerializerTest()
        : m_folder(WebString::fromUTF8("pageserializer/"))
        , m_baseUrl(toKURL("http://www.test.com"))
    {
    }

protected:
    virtual void SetUp()
    {
        // We want the images to load and JavaScript to be on.
        m_helper.initialize(true, 0, 0, &configureSettings);
    }

    virtual void TearDown()
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

    void setBaseUrl(const char* url)
    {
        m_baseUrl = toKURL(url);
    }

    void setBaseFolder(const char* folder)
    {
        m_folder = WebString::fromUTF8(folder);
    }

    void registerURL(const char* file, const char* mimeType)
    {
        registerMockedURLLoad(KURL(m_baseUrl, file), WebString::fromUTF8(file), m_folder, WebString::fromUTF8(mimeType));
    }

    void registerErrorURL(const char* file, int statusCode)
    {
        WebURLError error;
        error.reason = 0xdead + statusCode;
        error.domain = "PageSerializerTest";

        WebURLResponse response;
        response.initialize();
        response.setMIMEType("text/html");
        response.setHTTPStatusCode(statusCode);

        Platform::current()->unitTestSupport()->registerMockedErrorURL(KURL(m_baseUrl, file), response, error);
    }

    void serialize(const char* url)
    {
        FrameTestHelpers::loadFrame(m_helper.webView()->mainFrame(), KURL(m_baseUrl, url).string().utf8().data());
        PageSerializer serializer(&m_resources);
        serializer.serialize(m_helper.webViewImpl()->mainFrameImpl()->frame()->page());
    }

    Vector<SerializedResource>& getResources()
    {
        return m_resources;
    }


    const SerializedResource* getResource(const char* url, const char* mimeType)
    {
        KURL kURL = KURL(m_baseUrl, url);
        String mime(mimeType);
        for (size_t i = 0; i < m_resources.size(); ++i) {
            const SerializedResource& resource = m_resources[i];
            if (resource.url == kURL && !resource.data->isEmpty()
                && (mime.isNull() || equalIgnoringCase(resource.mimeType, mime)))
                return &resource;
        }
        return 0;
    }

    bool isSerialized(const char* url, const char* mimeType = 0)
    {
        return getResource(url, mimeType);
    }

    String getSerializedData(const char* url, const char* mimeType = 0)
    {
        const SerializedResource* resource = getResource(url, mimeType);
        if (resource)
            return String(resource->data->data(), resource->data->size());
        return String();
    }

private:
    static void configureSettings(WebSettings* settings)
    {
        settings->setImagesEnabled(true);
        settings->setLoadsImagesAutomatically(true);
        settings->setJavaScriptEnabled(true);
    }

    FrameTestHelpers::WebViewHelper m_helper;
    WebString m_folder;
    KURL m_baseUrl;
    Vector<SerializedResource> m_resources;
};


TEST_F(PageSerializerTest, InputImage)
{
    setBaseFolder("pageserializer/input-image/");

    registerURL("input-image.html", "text/html");
    registerURL("button.png", "image/png");
    registerErrorURL("non-existing-button.png", 404);

    serialize("input-image.html");

    EXPECT_TRUE(isSerialized("button.png", "image/png"));
    EXPECT_FALSE(isSerialized("non-existing-button.png", "image/png"));
}

TEST_F(PageSerializerTest, XMLDeclaration)
{
    V8TestingScope scope(v8::Isolate::GetCurrent());
    setBaseFolder("pageserializer/xmldecl/");

    registerURL("xmldecl.xml", "text/xml");
    serialize("xmldecl.xml");

    String expectedStart("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    EXPECT_TRUE(getSerializedData("xmldecl.xml").startsWith(expectedStart));
}

TEST_F(PageSerializerTest, DTD)
{
    setBaseFolder("pageserializer/dtd/");

    registerURL("dtd.html", "text/html");
    serialize("dtd.html");

    String expectedStart("<!DOCTYPE html>");
    EXPECT_TRUE(getSerializedData("dtd.html").startsWith(expectedStart));
}

TEST_F(PageSerializerTest, Font)
{
    setBaseFolder("pageserializer/font/");

    registerURL("font.html", "text/html");
    registerURL("font.ttf", "application/octet-stream");

    serialize("font.html");

    EXPECT_TRUE(isSerialized("font.ttf", "application/octet-stream"));
}

}
