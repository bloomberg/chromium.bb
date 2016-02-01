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

#include "core/frame/FrameSerializer.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "platform/SerializedResource.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebString.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebSettings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "wtf/Assertions.h"
#include "wtf/Vector.h"

using blink::URLTestHelpers::toKURL;
using blink::URLTestHelpers::registerMockedURLLoad;

namespace blink {

class FrameSerializerTest
    : public testing::Test
    , public FrameSerializer::Delegate {
public:
    FrameSerializerTest()
        : m_folder(WebString::fromUTF8("frameserializer/"))
        , m_baseUrl(toKURL("http://www.test.com"))
    {
    }

protected:
    void SetUp() override
    {
        // We want the images to load and JavaScript to be on.
        m_helper.initialize(true, 0, 0, &configureSettings);
    }

    void TearDown() override
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

    void setBaseFolder(const char* folder)
    {
        m_folder = WebString::fromUTF8(folder);
    }

    void setRewriteURLFolder(const char* folder)
    {
        m_rewriteFolder = folder;
    }

    void registerURL(const char* url, const char* file, const char* mimeType)
    {
        registerMockedURLLoad(KURL(m_baseUrl, url), WebString::fromUTF8(file), m_folder, WebString::fromUTF8(mimeType));
    }

    void registerURL(const char* file, const char* mimeType)
    {
        registerURL(file, file, mimeType);
    }

    void registerErrorURL(const char* file, int statusCode)
    {
        WebURLError error;
        error.reason = 0xdead + statusCode;
        error.domain = "FrameSerializerTest";

        WebURLResponse response;
        response.initialize();
        response.setMIMEType("text/html");
        response.setHTTPStatusCode(statusCode);

        Platform::current()->unitTestSupport()->registerMockedErrorURL(KURL(m_baseUrl, file), response, error);
    }

    void registerRewriteURL(const char* fromURL, const char* toURL)
    {
        m_rewriteURLs.add(fromURL, toURL);
    }

    void serialize(const char* url)
    {
        FrameTestHelpers::loadFrame(m_helper.webView()->mainFrame(), KURL(m_baseUrl, url).string().utf8().data());
        FrameSerializer serializer(m_resources, *this);
        Frame* frame = m_helper.webViewImpl()->mainFrameImpl()->frame();
        for (; frame; frame = frame->tree().traverseNext()) {
            // This is safe, because tests do not do cross-site navigation
            // (and therefore don't have remote frames).
            serializer.serializeFrame(*toLocalFrame(frame));
        }
    }

    Vector<SerializedResource>& getResources()
    {
        return m_resources;
    }

    const SerializedResource* getResource(const char* urlString, const char* mimeType)
    {
        const KURL url(m_baseUrl, urlString);
        String mime(mimeType);
        for (size_t i = 0; i < m_resources.size(); ++i) {
            const SerializedResource& resource = m_resources[i];
            if (resource.url == url && !resource.data->isEmpty()
                && (mime.isNull() || equalIgnoringCase(resource.mimeType, mime)))
                return &resource;
        }
        return nullptr;
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

    // FrameSerializer::Delegate implementation.
    bool rewriteLink(const Element& element, String& rewrittenLink)
    {
        String completeURL;
        for (const auto& attribute : element.attributes()) {
            if (element.hasLegalLinkAttribute(attribute.name())) {
                completeURL = element.document().completeURL(attribute.value());
                break;
            }
        }

        if (completeURL.isNull() || !m_rewriteURLs.contains(completeURL))
            return false;

        StringBuilder uriBuilder;
        uriBuilder.append(m_rewriteFolder);
        uriBuilder.appendLiteral("/");
        uriBuilder.append(m_rewriteURLs.get(completeURL));
        rewrittenLink = uriBuilder.toString();
        return true;
    }

    FrameTestHelpers::WebViewHelper m_helper;
    WebString m_folder;
    KURL m_baseUrl;
    Vector<SerializedResource> m_resources;
    HashMap<String, String> m_rewriteURLs;
    String m_rewriteFolder;
};

TEST_F(FrameSerializerTest, HTMLElements)
{
    setBaseFolder("frameserializer/elements/");

    registerURL("elements.html", "text/html");
    registerURL("style.css", "style.css", "text/css");
    registerURL("copyright.html", "empty.txt", "text/html");
    registerURL("script.js", "empty.txt", "text/javascript");

    registerURL("bodyBackground.png", "image.png", "image/png");

    registerURL("imageSrc.png", "image.png", "image/png");

    registerURL("inputImage.png", "image.png", "image/png");

    registerURL("tableBackground.png", "image.png", "image/png");
    registerURL("trBackground.png", "image.png", "image/png");
    registerURL("tdBackground.png", "image.png", "image/png");

    registerURL("blockquoteCite.html", "empty.txt", "text/html");
    registerURL("qCite.html", "empty.txt", "text/html");
    registerURL("delCite.html", "empty.txt", "text/html");
    registerURL("insCite.html", "empty.txt", "text/html");

    registerErrorURL("nonExisting.png", 404);

    serialize("elements.html");

    EXPECT_EQ(8U, getResources().size());

    EXPECT_TRUE(isSerialized("elements.html", "text/html"));
    EXPECT_TRUE(isSerialized("style.css", "text/css"));
    EXPECT_TRUE(isSerialized("bodyBackground.png", "image/png"));
    EXPECT_TRUE(isSerialized("imageSrc.png", "image/png"));
    EXPECT_TRUE(isSerialized("inputImage.png", "image/png"));
    EXPECT_TRUE(isSerialized("tableBackground.png", "image/png"));
    EXPECT_TRUE(isSerialized("trBackground.png", "image/png"));
    EXPECT_TRUE(isSerialized("tdBackground.png", "image/png"));
    EXPECT_FALSE(isSerialized("nonExisting.png", "image/png"));
}

TEST_F(FrameSerializerTest, Frames)
{
    setBaseFolder("frameserializer/frames/");

    registerURL("simple_frames.html", "text/html");
    registerURL("simple_frames_top.html", "text/html");
    registerURL("simple_frames_1.html", "text/html");
    registerURL("simple_frames_3.html", "text/html");

    registerURL("frame_1.png", "image.png", "image/png");
    registerURL("frame_2.png", "image.png", "image/png");
    registerURL("frame_3.png", "image.png", "image/png");
    registerURL("frame_4.png", "image.png", "image/png");

    serialize("simple_frames.html");

    EXPECT_EQ(8U, getResources().size());

    EXPECT_TRUE(isSerialized("simple_frames.html", "text/html"));
    EXPECT_TRUE(isSerialized("simple_frames_top.html", "text/html"));
    EXPECT_TRUE(isSerialized("simple_frames_1.html", "text/html"));
    EXPECT_TRUE(isSerialized("simple_frames_3.html", "text/html"));

    EXPECT_TRUE(isSerialized("frame_1.png", "image/png"));
    EXPECT_TRUE(isSerialized("frame_2.png", "image/png"));
    EXPECT_TRUE(isSerialized("frame_3.png", "image/png"));
    EXPECT_TRUE(isSerialized("frame_4.png", "image/png"));
}

TEST_F(FrameSerializerTest, IFrames)
{
    setBaseFolder("frameserializer/frames/");

    registerURL("top_frame.html", "text/html");
    registerURL("simple_iframe.html", "text/html");
    registerURL("object_iframe.html", "text/html");
    registerURL("embed_iframe.html", "text/html");
    registerURL("encoded_iframe.html", "text/html");

    registerURL("top.png", "image.png", "image/png");
    registerURL("simple.png", "image.png", "image/png");
    registerURL("object.png", "image.png", "image/png");
    registerURL("embed.png", "image.png", "image/png");

    serialize("top_frame.html");

    EXPECT_EQ(10U, getResources().size());

    EXPECT_TRUE(isSerialized("top_frame.html", "text/html"));
    EXPECT_TRUE(isSerialized("simple_iframe.html", "text/html")); // Twice.
    EXPECT_TRUE(isSerialized("object_iframe.html", "text/html"));
    EXPECT_TRUE(isSerialized("embed_iframe.html", "text/html"));
    EXPECT_TRUE(isSerialized("encoded_iframe.html", "text/html"));

    EXPECT_TRUE(isSerialized("top.png", "image/png"));
    EXPECT_TRUE(isSerialized("simple.png", "image/png"));
    EXPECT_TRUE(isSerialized("object.png", "image/png"));
    EXPECT_TRUE(isSerialized("embed.png", "image/png"));

    // Ensure that frame contents are not NFC-normalized before encoding.
    String expectedMetaCharset = "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=EUC-KR\">";
    EXPECT_TRUE(getSerializedData("encoded_iframe.html", "text/html").contains(expectedMetaCharset));
    EXPECT_TRUE(getSerializedData("encoded_iframe.html", "text/html").contains("\xE4\xC5\xD1\xE2"));
    EXPECT_FALSE(getSerializedData("encoded_iframe.html", "text/html").contains("\xE4\xC5\xE4\xC5"));
}

// Tests that when serializing a page with blank frames these are reported with their resources.
TEST_F(FrameSerializerTest, BlankFrames)
{
    setBaseFolder("frameserializer/frames/");

    registerURL("blank_frames.html", "text/html");
    registerURL("red_background.png", "image.png", "image/png");
    registerURL("orange_background.png", "image.png", "image/png");
    registerURL("blue_background.png", "image.png", "image/png");

    serialize("blank_frames.html");

    EXPECT_EQ(7U, getResources().size());

    EXPECT_TRUE(isSerialized("http://www.test.com/red_background.png", "image/png"));
    EXPECT_TRUE(isSerialized("http://www.test.com/orange_background.png", "image/png"));
    EXPECT_TRUE(isSerialized("http://www.test.com/blue_background.png", "image/png"));

    // The blank frames no longer get magic URL (i.e. wyciwyg://frame/0), so we
    // can't really assert their presence via URL.  We also can't use content-id
    // in assertions (since it is not deterministic).  Therefore we need to rely
    // on getResources().size() assertion above and on browser-level tests
    // (i.e. SavePageMultiFrameBrowserTest.AboutBlank).
}

TEST_F(FrameSerializerTest, CSS)
{
    setBaseFolder("frameserializer/css/");

    registerURL("css_test_page.html", "text/html");
    registerURL("link_styles.css", "text/css");
    registerURL("encoding.css", "text/css");
    registerURL("import_style_from_link.css", "text/css");
    registerURL("import_styles.css", "text/css");
    registerURL("do_not_serialize.png", "image.png", "image/png");
    registerURL("red_background.png", "image.png", "image/png");
    registerURL("orange_background.png", "image.png", "image/png");
    registerURL("yellow_background.png", "image.png", "image/png");
    registerURL("green_background.png", "image.png", "image/png");
    registerURL("blue_background.png", "image.png", "image/png");
    registerURL("purple_background.png", "image.png", "image/png");
    registerURL("pink_background.png", "image.png", "image/png");
    registerURL("brown_background.png", "image.png", "image/png");
    registerURL("ul-dot.png", "image.png", "image/png");
    registerURL("ol-dot.png", "image.png", "image/png");

    serialize("css_test_page.html");

    EXPECT_EQ(15U, getResources().size());

    EXPECT_FALSE(isSerialized("do_not_serialize.png", "image/png"));

    EXPECT_TRUE(isSerialized("css_test_page.html", "text/html"));
    EXPECT_TRUE(isSerialized("link_styles.css", "text/css"));
    EXPECT_TRUE(isSerialized("encoding.css", "text/css"));
    EXPECT_TRUE(isSerialized("import_styles.css", "text/css"));
    EXPECT_TRUE(isSerialized("import_style_from_link.css", "text/css"));
    EXPECT_TRUE(isSerialized("red_background.png", "image/png"));
    EXPECT_TRUE(isSerialized("orange_background.png", "image/png"));
    EXPECT_TRUE(isSerialized("yellow_background.png", "image/png"));
    EXPECT_TRUE(isSerialized("green_background.png", "image/png"));
    EXPECT_TRUE(isSerialized("blue_background.png", "image/png"));
    EXPECT_TRUE(isSerialized("purple_background.png", "image/png"));
    EXPECT_TRUE(isSerialized("pink_background.png", "image/png"));
    EXPECT_TRUE(isSerialized("brown_background.png", "image/png"));
    EXPECT_TRUE(isSerialized("ul-dot.png", "image/png"));
    EXPECT_TRUE(isSerialized("ol-dot.png", "image/png"));

    // Ensure encodings are specified.
    EXPECT_TRUE(getSerializedData("link_styles.css", "text/css").startsWith("@charset"));
    EXPECT_TRUE(getSerializedData("import_styles.css", "text/css").startsWith("@charset"));
    EXPECT_TRUE(getSerializedData("import_style_from_link.css", "text/css").startsWith("@charset"));
    EXPECT_TRUE(getSerializedData("encoding.css", "text/css").startsWith("@charset \"euc-kr\";"));

    // Ensure that stylesheet contents are not NFC-normalized before encoding.
    EXPECT_TRUE(getSerializedData("encoding.css", "text/css").contains("\xE4\xC5\xD1\xE2"));
    EXPECT_FALSE(getSerializedData("encoding.css", "text/css").contains("\xE4\xC5\xE4\xC5"));
}

TEST_F(FrameSerializerTest, CSSImport)
{
    setBaseFolder("frameserializer/css/");

    registerURL("import.html", "text/html");
    registerURL("import/base.css", "text/css");
    registerURL("import/relative/red-background.css", "text/css");
    registerURL("import/absolute/green-header.css", "text/css");

    serialize("import.html");

    EXPECT_TRUE(isSerialized("import.html", "text/html"));
    EXPECT_TRUE(isSerialized("import/base.css", "text/css"));
    EXPECT_TRUE(isSerialized("import/relative/red-background.css", "text/css"));
    EXPECT_TRUE(isSerialized("import/absolute/green-header.css", "text/css"));
}

TEST_F(FrameSerializerTest, XMLDeclaration)
{
    V8TestingScope scope(v8::Isolate::GetCurrent());
    setBaseFolder("frameserializer/xml/");

    registerURL("xmldecl.xml", "text/xml");
    serialize("xmldecl.xml");

    String expectedStart("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    EXPECT_TRUE(getSerializedData("xmldecl.xml").startsWith(expectedStart));
}

TEST_F(FrameSerializerTest, DTD)
{
    setBaseFolder("frameserializer/dtd/");

    registerURL("html5.html", "text/html");
    serialize("html5.html");

    String expectedStart("<!DOCTYPE html>");
    EXPECT_TRUE(getSerializedData("html5.html").startsWith(expectedStart));
}

TEST_F(FrameSerializerTest, Font)
{
    setBaseFolder("frameserializer/font/");

    registerURL("font.html", "text/html");
    registerURL("font.ttf", "application/octet-stream");

    serialize("font.html");

    EXPECT_TRUE(isSerialized("font.ttf", "application/octet-stream"));
}

TEST_F(FrameSerializerTest, DataURI)
{
    setBaseFolder("frameserializer/datauri/");

    registerURL("page_with_data.html", "text/html");

    serialize("page_with_data.html");

    EXPECT_EQ(1U, getResources().size());
    EXPECT_TRUE(isSerialized("page_with_data.html", "text/html"));
}

TEST_F(FrameSerializerTest, DataURIMorphing)
{
    setBaseFolder("frameserializer/datauri/");

    registerURL("page_with_morphing_data.html", "text/html");

    serialize("page_with_morphing_data.html");

    EXPECT_EQ(2U, getResources().size());
    EXPECT_TRUE(isSerialized("page_with_morphing_data.html", "text/html"));
}

TEST_F(FrameSerializerTest, RewriteLinksSimple)
{
    setBaseFolder("frameserializer/rewritelinks/");
    setRewriteURLFolder("folder");

    registerURL("rewritelinks_simple.html", "text/html");
    registerURL("absolute.png", "image.png", "image/png");
    registerURL("relative.png", "image.png", "image/png");
    registerRewriteURL("http://www.test.com/absolute.png", "a.png");
    registerRewriteURL("http://www.test.com/relative.png", "b.png");

    serialize("rewritelinks_simple.html");

    EXPECT_EQ(3U, getResources().size());
    EXPECT_NE(getSerializedData("rewritelinks_simple.html", "text/html").find("\"folder/a.png\""), kNotFound);
    EXPECT_NE(getSerializedData("rewritelinks_simple.html", "text/html").find("\"folder/b.png\""), kNotFound);
}

// Test that we don't regress https://bugs.webkit.org/show_bug.cgi?id=99105
TEST_F(FrameSerializerTest, SVGImageDontCrash)
{
    setBaseFolder("frameserializer/svg/");

    registerURL("page_with_svg_image.html", "text/html");
    registerURL("green_rectangle.svg", "image/svg+xml");

    serialize("page_with_svg_image.html");

    EXPECT_EQ(2U, getResources().size());

    EXPECT_TRUE(isSerialized("green_rectangle.svg", "image/svg+xml"));
    EXPECT_GT(getSerializedData("green_rectangle.svg", "image/svg+xml").length(), 250U);
}

TEST_F(FrameSerializerTest, DontIncludeErrorImage)
{
    setBaseFolder("frameserializer/image/");

    registerURL("page_with_img_error.html", "text/html");
    registerURL("error_image.png", "image/png");

    serialize("page_with_img_error.html");

    EXPECT_EQ(1U, getResources().size());
    EXPECT_TRUE(isSerialized("page_with_img_error.html", "text/html"));
    EXPECT_FALSE(isSerialized("error_image.png", "image/png"));
}

TEST_F(FrameSerializerTest, NamespaceElementsDontCrash)
{
    setBaseFolder("frameserializer/namespace/");

    registerURL("namespace_element.html", "text/html");

    serialize("namespace_element.html");

    EXPECT_EQ(1U, getResources().size());
    EXPECT_TRUE(isSerialized("namespace_element.html", "text/html"));
    EXPECT_GT(getSerializedData("namespace_element.html", "text/html").length(), 0U);
}

TEST_F(FrameSerializerTest, markOfTheWebDeclaration)
{
    EXPECT_EQ("saved from url=(0015)http://foo.com/", FrameSerializer::markOfTheWebDeclaration(KURL(ParsedURLString, "http://foo.com")));
    EXPECT_EQ("saved from url=(0015)http://f-o.com/", FrameSerializer::markOfTheWebDeclaration(KURL(ParsedURLString, "http://f-o.com")));
    EXPECT_EQ("saved from url=(0019)http://foo.com-%2D/", FrameSerializer::markOfTheWebDeclaration(KURL(ParsedURLString, "http://foo.com--")));
    EXPECT_EQ("saved from url=(0024)http://f-%2D.com-%2D%3E/", FrameSerializer::markOfTheWebDeclaration(KURL(ParsedURLString, "http://f--.com-->")));
    EXPECT_EQ("saved from url=(0020)http://foo.com/?-%2D", FrameSerializer::markOfTheWebDeclaration(KURL(ParsedURLString, "http://foo.com?--")));
    EXPECT_EQ("saved from url=(0020)http://foo.com/#-%2D", FrameSerializer::markOfTheWebDeclaration(KURL(ParsedURLString, "http://foo.com#--")));
    EXPECT_EQ("saved from url=(0026)http://foo.com/#bar-%2Dbaz", FrameSerializer::markOfTheWebDeclaration(KURL(ParsedURLString, "http://foo.com#bar--baz")));
}

} // namespace blink
