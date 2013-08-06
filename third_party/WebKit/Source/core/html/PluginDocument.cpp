/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/PluginDocument.h"

#include "HTMLNames.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/RawDataDocumentParser.h"
#include "core/html/HTMLEmbedElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/plugins/PluginView.h"
#include "core/rendering/RenderEmbeddedObject.h"

namespace WebCore {

using namespace HTMLNames;

// FIXME: Share more code with MediaDocumentParser.
class PluginDocumentParser : public RawDataDocumentParser {
public:
    static PassRefPtr<PluginDocumentParser> create(PluginDocument* document)
    {
        return adoptRef(new PluginDocumentParser(document));
    }

private:
    PluginDocumentParser(Document* document)
        : RawDataDocumentParser(document)
        , m_embedElement(0)
    {
    }

    virtual size_t appendBytes(const char*, size_t) OVERRIDE;

    virtual void finish() OVERRIDE;

    void createDocumentStructure();

    PluginView* pluginView() const;

    HTMLEmbedElement* m_embedElement;
};

void PluginDocumentParser::createDocumentStructure()
{
    RefPtr<Element> rootElement = document()->createElement(htmlTag, false);
    document()->appendChild(rootElement, IGNORE_EXCEPTION);
    toHTMLHtmlElement(rootElement.get())->insertedByParser();

    if (document()->frame() && document()->frame()->loader())
        document()->frame()->loader()->dispatchDocumentElementAvailable();

    RefPtr<Element> body = document()->createElement(bodyTag, false);
    body->setAttribute(marginwidthAttr, "0");
    body->setAttribute(marginheightAttr, "0");
    body->setAttribute(styleAttr, "background-color: rgb(38,38,38)");

    rootElement->appendChild(body, IGNORE_EXCEPTION);

    RefPtr<Element> embedElement = document()->createElement(embedTag, false);

    m_embedElement = static_cast<HTMLEmbedElement*>(embedElement.get());
    m_embedElement->setAttribute(widthAttr, "100%");
    m_embedElement->setAttribute(heightAttr, "100%");

    m_embedElement->setAttribute(nameAttr, "plugin");
    m_embedElement->setAttribute(srcAttr, document()->url().string());

    DocumentLoader* loader = document()->loader();
    ASSERT(loader);
    if (loader)
        m_embedElement->setAttribute(typeAttr, loader->mimeType());

    toPluginDocument(document())->setPluginNode(m_embedElement);

    body->appendChild(embedElement, IGNORE_EXCEPTION);

    Frame* frame = document()->frame();
    if (!frame)
        return;
    Settings* settings = frame->settings();
    if (!settings || !frame->loader()->allowPlugins(NotAboutToInstantiatePlugin))
        return;

    document()->updateLayout();

    // Below we assume that renderer->widget() to have been created by
    // document()->updateLayout(). However, in some cases, updateLayout() will
    // recurse too many times and delay its post-layout tasks (such as creating
    // the widget). Here we kick off the pending post-layout tasks so that we
    // can synchronously redirect data to the plugin.
    frame->view()->flushAnyPendingPostLayoutTasks();

    if (PluginView* view = pluginView())
        view->didReceiveResponse(document()->loader()->response());
}

size_t PluginDocumentParser::appendBytes(const char* data, size_t length)
{
    if (!m_embedElement)
        createDocumentStructure();

    if (!length)
        return 0;
    if (PluginView* view = pluginView())
        view->didReceiveData(data, length);

    return 0;
}

void PluginDocumentParser::finish()
{
    if (PluginView* view = pluginView()) {
        const ResourceError& error = document()->loader()->mainDocumentError();
        if (error.isNull())
            view->didFinishLoading();
        else
            view->didFailLoading(error);
    }
    RawDataDocumentParser::finish();
}

PluginView* PluginDocumentParser::pluginView() const
{
    if (Widget* widget = static_cast<PluginDocument*>(document())->pluginWidget()) {
        ASSERT_WITH_SECURITY_IMPLICATION(widget->isPluginContainer());
        return static_cast<PluginView*>(widget);
    }
    return 0;
}

PluginDocument::PluginDocument(const DocumentInit& initializer)
    : HTMLDocument(initializer, PluginDocumentClass)
    , m_shouldLoadPluginManually(true)
{
    setCompatibilityMode(QuirksMode);
    lockCompatibilityMode();
}

PassRefPtr<DocumentParser> PluginDocument::createParser()
{
    return PluginDocumentParser::create(this);
}

Widget* PluginDocument::pluginWidget()
{
    if (m_pluginNode && m_pluginNode->renderer()) {
        ASSERT(m_pluginNode->renderer()->isEmbeddedObject());
        return toRenderEmbeddedObject(m_pluginNode->renderer())->widget();
    }
    return 0;
}

Node* PluginDocument::pluginNode()
{
    return m_pluginNode.get();
}

void PluginDocument::detach(const AttachContext& context)
{
    // Release the plugin node so that we don't have a circular reference.
    m_pluginNode = 0;
    HTMLDocument::detach(context);
}

void PluginDocument::cancelManualPluginLoad()
{
    // PluginDocument::cancelManualPluginLoad should only be called once, but there are issues
    // with how many times we call beforeload on object elements. <rdar://problem/8441094>.
    if (!shouldLoadPluginManually())
        return;

    DocumentLoader* documentLoader = frame()->loader()->activeDocumentLoader();
    documentLoader->cancelMainResourceLoad(ResourceError::cancelledError(documentLoader->request().url()));
    setShouldLoadPluginManually(false);
}

}
