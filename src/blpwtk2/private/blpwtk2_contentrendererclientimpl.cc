/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_contentrendererclientimpl.h>
#include <blpwtk2_inprocessresourceloaderbridge.h>
#include <blpwtk2_renderviewobserverimpl.h>
#include <blpwtk2_resourceloader.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_stringref.h>



// patch section: jswidget plugin


// patch section: renderer ui



#include <base/strings/utf_string_conversions.h>
#include <chrome/common/constants.mojom.h>
#include <content/child/font_warmup_win.h>
#include <content/public/renderer/render_thread.h>
#include <net/base/net_errors.h>
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include <skia/ext/fontmgr_default.h>
#include <third_party/skia/include/core/SkFontMgr.h>
#include <third_party/blink/public/platform/web_url_error.h>
#include <third_party/blink/public/platform/web_url_request.h>
#include <third_party/blink/public/web/web_plugin_params.h>
#include <third_party/skia/include/ports/SkTypeface_win.h>
#include <ui/gfx/win/direct_write.h>

#include <sstream>

namespace blpwtk2 {

                        // -------------------------------
                        // class ContentRendererClientImpl
                        // -------------------------------

ContentRendererClientImpl::ContentRendererClientImpl()
{
    skia::OverrideDefaultSkFontMgr(SkFontMgr_New_DirectWrite());
}

ContentRendererClientImpl::~ContentRendererClientImpl()
{
}

void ContentRendererClientImpl::RenderViewCreated(
    content::RenderView* render_view)
{
    // Create an instance of RenderViewObserverImpl.  This is an observer that
    // is registered with the RenderView.  The RenderViewImpl's destructor
    // will call OnDestruct() on all observers, which will delete this
    // instance of RenderViewObserverImpl.
    new RenderViewObserverImpl(render_view);
}

void ContentRendererClientImpl::RenderFrameCreated(
    content::RenderFrame *render_frame)
{



    // patch section: spellcheck


    // patch section: printing



}

void ContentRendererClientImpl::PrepareErrorPage(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    bool ignoring_cache,
    std::string* error_html)
{
    GURL gurl = (GURL) error.url();

    std::string description;
    description = net::ErrorToString(error.reason());

    std::string errorCode;
    {
        char tmp[128];
        sprintf_s(tmp, sizeof(tmp), "%d", error.reason());
        errorCode = tmp;
    }

    std::string localdesc{};

    if (error_html) {
        *error_html = "<h2>Navigation Error</h2>";
        *error_html += "<p>Failed to load '<b>" + gurl.spec() + "</b>'</p>";
        if (description.length()) {
            *error_html += "<p>" + description + "</p>";
        }
        *error_html += "<p>Error Reason: " + errorCode + "</p>";
        if (localdesc.length()) {
            *error_html += "<p>" + localdesc + "</p>";
        }
    }
}

std::unique_ptr<content::ResourceLoaderBridge>
ContentRendererClientImpl::OverrideResourceLoaderBridge(
    const content::ResourceRequestInfoProvider& request_info) {
  StringRef url = request_info.url().spec();

  if (!Statics::inProcessResourceLoader ||
      !Statics::inProcessResourceLoader->canHandleURL(url)) {
    return nullptr;
  }
  return std::make_unique<InProcessResourceLoaderBridge>(request_info);
}

bool ContentRendererClientImpl::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin)
{
    return false;
}

void ContentRendererClientImpl::OnBindInterface(
        const service_manager::BindSourceInfo& source,
        const std::string& name,
        mojo::ScopedMessagePipeHandle handle)
{
    // needed for chrome services
    d_registry.TryBindInterface(name, &handle);
}

void ContentRendererClientImpl::OnStart()
{
    // needed for chrome services
}

void ContentRendererClientImpl::GetInterface(
        const std::string& interface_name, mojo::ScopedMessagePipeHandle interface_pipe)
{
    // needed for chrome services
    GetConnector()->BindInterface(
        service_manager::ServiceFilter::ByName(chrome::mojom::kServiceName),
        interface_name, std::move(interface_pipe));
}

void ContentRendererClientImpl::CreateRendererService(
    service_manager::mojom::ServiceRequest service_request)
{
    d_forward_service = std::make_unique<blpwtk2::ForwardingService>(this);
    d_service_binding = std::make_unique<service_manager::ServiceBinding>(d_forward_service.get(),
        std::move(service_request));
}

service_manager::Connector* ContentRendererClientImpl::GetConnector()
{
    // needed for chrome services
    if (!d_connector) {
        d_connector = service_manager::Connector::Create(&d_connector_request);
    }
    return d_connector.get();
}

ForwardingService::ForwardingService(Service* target) : target_(target) {}

ForwardingService::~ForwardingService() {}

void ForwardingService::OnStart() {
  target_->OnStart();
}

void ForwardingService::OnBindInterface(
    const service_manager::BindSourceInfo& source,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  target_->OnBindInterface(source, interface_name, std::move(interface_pipe));
}

bool ForwardingService::OnServiceManagerConnectionLost() {
  return target_->OnServiceManagerConnectionLost();
}

}  // close namespace blpwtk2

// vim: ts=4 et

