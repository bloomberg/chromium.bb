/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "public/web/WebPageSerializer.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/Frame.h"
#include "core/html/HTMLAllCollection.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTableElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"
#include "core/page/PageSerializer.h"
#include "platform/SerializedResource.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebCString.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"
#include "public/web/WebLocalFrame.h"
#include "public/web/WebPageSerializerClient.h"
#include "web/WebViewImpl.h"
#include "wtf/Vector.h"
#include "wtf/text/StringConcatenate.h"

namespace blink {

namespace {

class MHTMLPageSerializerDelegate final : public PageSerializer::Delegate {
public:
    ~MHTMLPageSerializerDelegate() override;
    bool shouldIgnoreAttribute(const Attribute&) override;
};

MHTMLPageSerializerDelegate::~MHTMLPageSerializerDelegate()
{
}

bool MHTMLPageSerializerDelegate::shouldIgnoreAttribute(const Attribute& attribute)
{
    // TODO(fgorski): Presence of srcset attribute causes MHTML to not display images, as only the value of src
    // is pulled into the archive. Discarding srcset prevents the problem. Long term we should make sure to MHTML
    // plays nicely with srcset.
    return attribute.localName() == HTMLNames::srcsetAttr;
}

} // namespace

void WebPageSerializer::serialize(WebView* view, WebVector<WebPageSerializer::Resource>* resourcesParam)
{
    Vector<SerializedResource> resources;
    PageSerializer serializer(&resources, PassOwnPtr<PageSerializer::Delegate>(nullptr));
    serializer.serialize(toWebViewImpl(view)->page());

    Vector<Resource> result;
    for (Vector<SerializedResource>::const_iterator iter = resources.begin(); iter != resources.end(); ++iter) {
        Resource resource;
        resource.url = iter->url;
        resource.mimeType = iter->mimeType.ascii();
        // FIXME: we are copying all the resource data here. Idealy we would have a WebSharedData().
        resource.data = WebCString(iter->data->data(), iter->data->size());
        result.append(resource);
    }

    *resourcesParam = result;
}

static PassRefPtr<SharedBuffer> serializePageToMHTML(Page* page, MHTMLArchive::EncodingPolicy encodingPolicy)
{
    Vector<SerializedResource> resources;
    PageSerializer serializer(&resources, adoptPtr(new MHTMLPageSerializerDelegate));
    serializer.serialize(page);
    Document* document = page->deprecatedLocalMainFrame()->document();
    return MHTMLArchive::generateMHTMLData(resources, encodingPolicy, document->title(), document->suggestedMIMEType());
}

WebCString WebPageSerializer::serializeToMHTML(WebView* view)
{
    RefPtr<SharedBuffer> mhtml = serializePageToMHTML(toWebViewImpl(view)->page(), MHTMLArchive::UseDefaultEncoding);
    // FIXME: we are copying all the data here. Idealy we would have a WebSharedData().
    return WebCString(mhtml->data(), mhtml->size());
}

WebCString WebPageSerializer::serializeToMHTMLUsingBinaryEncoding(WebView* view)
{
    RefPtr<SharedBuffer> mhtml = serializePageToMHTML(toWebViewImpl(view)->page(), MHTMLArchive::UseBinaryEncoding);
    // FIXME: we are copying all the data here. Idealy we would have a WebSharedData().
    return WebCString(mhtml->data(), mhtml->size());
}

bool WebPageSerializer::serialize(WebLocalFrame* frame, bool recursive, WebPageSerializerClient* client,
    const WebVector<WebURL>& links, const WebVector<WebString>& localPaths, const WebString& localDirectoryName)
{
    ASSERT(frame);
    ASSERT(client);
    ASSERT(links.size() == localPaths.size());

    Vector<SerializedResource> resources;
    PageSerializer serializer(&resources, nullptr);

    serializer.setRewriteURLFolder(localDirectoryName);
    for (size_t i = 0; i < links.size(); i++) {
        KURL url = links[i];
        serializer.registerRewriteURL(url.string(), localPaths[i]);
    }

    serializer.serialize(toWebViewImpl(frame->view())->page());

    for (SerializedResource& resource : resources) {
        client->didSerializeDataForFrame(resource.url, WebCString(resource.data->data(), resource.data->size()), WebPageSerializerClient::CurrentFrameIsFinished);
    }
    client->didSerializeDataForFrame(KURL(), WebCString("", 0), WebPageSerializerClient::AllFramesAreFinished);
    return true;
}

WebString WebPageSerializer::generateMetaCharsetDeclaration(const WebString& charset)
{
    String charsetString = "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=" + static_cast<const String&>(charset) + "\">";
    return charsetString;
}

WebString WebPageSerializer::generateMarkOfTheWebDeclaration(const WebURL& url)
{
    return String::format("\n<!-- saved from url=(%04d)%s -->\n",
        static_cast<int>(url.spec().length()), url.spec().data());
}

WebString WebPageSerializer::generateBaseTagDeclaration(const WebString& baseTarget)
{
    if (baseTarget.isEmpty())
        return String("<base href=\".\">");
    String baseString = "<base href=\".\" target=\"" + static_cast<const String&>(baseTarget) + "\">";
    return baseString;
}

} // namespace blink
