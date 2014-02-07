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

#include "config.h"
#include "WebHelperPluginImpl.h"

#include "PageWidgetDelegate.h"
#include "WebDocument.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebPlugin.h"
#include "WebPluginContainerImpl.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "WebWidgetClient.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/FocusController.h"
#include "core/frame/FrameView.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"

using namespace WebCore;

namespace blink {

static const char documentStartLiteral[] = "<!DOCTYPE html><head><meta charset='UTF-8'></head><body>\n<object type=\"";
static const char documentEndLiteral[] = "\"></object></body>\n";

static void writeDocument(const WebString& pluginType, const WebURL& url, WebCore::FrameLoader& loader)
{
    RefPtr<SharedBuffer> data = SharedBuffer::create();
    data->append(documentStartLiteral, sizeof(documentStartLiteral) - 1);
    data->append(pluginType.utf8().data(), pluginType.utf8().length());
    data->append(documentEndLiteral, sizeof(documentEndLiteral) - 1);

    loader.load(FrameLoadRequest(0, ResourceRequest(url.isValid() ? KURL(url) : blankURL()), SubstituteData(data, "text/html", "UTF-8", KURL(), ForceSynchronousLoad)));
}

class HelperPluginChromeClient : public EmptyChromeClient {
    WTF_MAKE_NONCOPYABLE(HelperPluginChromeClient);
    WTF_MAKE_FAST_ALLOCATED;

public:
    explicit HelperPluginChromeClient(WebHelperPluginImpl* widget)
        : m_widget(widget)
    {
        ASSERT(m_widget->m_widgetClient);
    }

private:
    virtual void closeWindowSoon() OVERRIDE
    {
        // This should never be called since the only way to close the
        // invisible page is via closeAndDelete().
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual void* webView() const OVERRIDE
    {
        return m_widget->m_webView;
    }

    WebHelperPluginImpl* m_widget;
};

// HelperPluginFrameClient acts as a filter to only forward messages onto the
// main render frame that WebHelperPlugin actually needs. This prevents
// having the WebHelperPlugin's frame accidentally signaling events on the
// client that are meant only for WebFrames which are part of the main DOM.
class HelperPluginFrameClient : public WebFrameClient {
public:
    HelperPluginFrameClient(WebFrameClient* hostWebFrameClient)
        : m_hostWebFrameClient(hostWebFrameClient)
    {
    }

    virtual ~HelperPluginFrameClient()
    {
    }

    virtual WebPlugin* createPlugin(blink::WebFrame* frame, const WebPluginParams& params)
    {
        return m_hostWebFrameClient->createPlugin(frame, params);
    }

private:
    WebFrameClient* m_hostWebFrameClient;
};


// WebHelperPluginImpl ----------------------------------------------------------------

WebHelperPluginImpl::WebHelperPluginImpl(WebWidgetClient* client)
    : m_widgetClient(client)
    , m_webView(0)
    , m_mainFrame(0)
{
    ASSERT(client);
}

WebHelperPluginImpl::~WebHelperPluginImpl()
{
    ASSERT(!m_page);
    ASSERT(!m_widgetClient); // Should only be called via close().
}

bool WebHelperPluginImpl::initialize(const String& pluginType, const WebDocument& hostDocument, WebViewImpl* webView)
{
    ASSERT(webView);
    m_webView = webView;

    return initializePage(pluginType, hostDocument);
}

void WebHelperPluginImpl::closeAndDelete()
{
    if (m_page) {
        m_page->clearPageGroup();
        m_page->mainFrame()->loader().stopAllLoaders();
    }

    // We must destroy the page now in case the host page is being destroyed, in
    // which case some of the objects the page depends on may have been
    // destroyed by the time this->close() is called asynchronously.
    destroyPage();

    // closeWidgetSoon() will call this->close() later.
    m_widgetClient->closeWidgetSoon();

    m_mainFrame->close();
    m_mainFrame = 0;
}

void WebHelperPluginImpl::closeAndDeleteSoon()
{
    m_webView->closeAndDeleteHelperPluginSoon(this);
}

void WebHelperPluginImpl::initializeFrame(WebFrameClient* client)
{
    ASSERT(m_page);
    ASSERT(!m_frameClient);
    m_frameClient = adoptPtr(new HelperPluginFrameClient(client));
    m_mainFrame = WebFrameImpl::create(m_frameClient.get());
    m_mainFrame->initializeAsMainFrame(m_page.get());
}

// Returns a pointer to the WebPlugin by finding the single <object> tag in the page.
WebPlugin* WebHelperPluginImpl::getPlugin()
{
    ASSERT(m_page);

    RefPtr<HTMLCollection> objectElements = m_page->mainFrame()->document()->getElementsByTagName(WebCore::HTMLNames::objectTag.localName());
    ASSERT(objectElements && objectElements->length() == 1);
    if (!objectElements || objectElements->length() < 1)
        return 0;
    Element* element = objectElements->item(0);
    ASSERT(element->hasTagName(WebCore::HTMLNames::objectTag));
    WebCore::Widget* widget = toHTMLPlugInElement(element)->pluginWidget();
    if (!widget)
        return 0;
    WebPlugin* plugin = toWebPluginContainerImpl(widget)->plugin();
    ASSERT(plugin);
    // If the plugin is a placeholder, it is not useful to the caller, and it
    // could be replaced at any time. Therefore, do not return it.
    if (plugin->isPlaceholder())
        return 0;

    // The plugin was instantiated and will outlive this object.
    return plugin;
}

bool WebHelperPluginImpl::initializePage(const WebString& pluginType, const WebDocument& hostDocument)
{
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    m_chromeClient = adoptPtr(new HelperPluginChromeClient(this));
    pageClients.chromeClient = m_chromeClient.get();

    m_page = adoptPtr(new Page(pageClients));
    ASSERT(!m_page->settings().scriptEnabled());
    m_page->settings().setPluginsEnabled(true);

    m_webView->client()->initializeHelperPluginWebFrame(this);

    // The page's main frame was set in initializeFrame() as a result of the above call.
    Frame* frame = m_page->mainFrame();
    ASSERT(frame);
    frame->loader().forceSandboxFlags(SandboxAll & ~SandboxPlugins);
    frame->setView(FrameView::create(frame));
    // No need to set a size or make it not transparent.

    writeDocument(pluginType, hostDocument.url(), frame->loader());

    return true;
}

void WebHelperPluginImpl::destroyPage()
{
    if (!m_page)
        return;

    if (m_page->mainFrame())
        m_page->mainFrame()->loader().frameDetached();

    m_page.clear();
}

void WebHelperPluginImpl::layout()
{
    PageWidgetDelegate::layout(m_page.get());
}

void WebHelperPluginImpl::setFocus(bool)
{
    ASSERT_NOT_REACHED();
}

void WebHelperPluginImpl::close()
{
    RELEASE_ASSERT(!m_page); // Should only be called via closeAndDelete().
    RELEASE_ASSERT(!m_mainFrame); // This function must be called asynchronously, after closeAndDelete() completes.
    m_widgetClient = 0;
    delete this;
}

// WebHelperPlugin ----------------------------------------------------------------

WebHelperPlugin* WebHelperPlugin::create(WebWidgetClient* client)
{
    RELEASE_ASSERT(client);

    // The returned object is owned by the caller, which must destroy it by
    // calling closeAndDelete(). The WebWidgetClient must not call close()
    // other than as a result of closeAndDelete().
    return new WebHelperPluginImpl(client);
}

} // namespace blink
