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
#include <blpwtk2_rendercompositor.h>
#include <blpwtk2_rendermessagedelegate.h>
#include <blpwtk2_renderviewobserverimpl.h>
#include <blpwtk2_resourceloader.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_stringref.h>



// patch section: jswidget plugin
#include <blpwtk2_jswidget.h>


// patch section: renderer ui



#include <base/strings/utf_string_conversions.h>
#include <chrome/common/constants.mojom.h>
#include <components/spellcheck/renderer/spellcheck.h>
#include <components/spellcheck/renderer/spellcheck_provider.h>
#include <content/child/font_warmup_win.h>
#include <content/public/renderer/render_frame.h>
#include <content/public/renderer/render_thread.h>
#include <content/public/renderer/render_view.h>
#include <net/base/net_errors.h>
#include <services/service_manager/public/cpp/service_context.h>
#include "services/service_manager/public/cpp/bind_source_info.h"
#include <skia/ext/fontmgr_default_win.h>
#include <third_party/skia/include/core/SkFontMgr.h>
#include <third_party/blink/public/platform/web_url_error.h>
#include <third_party/blink/public/platform/web_url_request.h>
#include <third_party/blink/public/web/web_plugin_params.h>
#include <third_party/skia/include/ports/SkTypeface_win.h>
#include <ui/gfx/win/direct_write.h>

#include <components/printing/renderer/print_render_frame_helper.h>

#include <sstream>

namespace blpwtk2 {

                        // -------------------------------
                        // class ContentRendererClientImpl
                        // -------------------------------

ContentRendererClientImpl::ContentRendererClientImpl()
{
    SetDefaultSkiaFactory(SkFontMgr_New_DirectWrite());
}

ContentRendererClientImpl::~ContentRendererClientImpl()
{
    RenderCompositorFactory::Terminate();
}

void ContentRendererClientImpl::RenderThreadStarted()
{
    if (!d_spellcheck) {
        d_spellcheck.reset(new SpellCheck(&d_registry, this));
    }
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

    // Create an instance of SpellCheckProvider.
    new SpellCheckProvider(render_frame, d_spellcheck.get(), this);


    // patch section: printing

    // Create an instance of PrintWebViewHelper.  This is an observer that is
    // registered with the RenderFrame.  The RenderFrameImpl's destructor
    // will call OnDestruct() on all observers, which will delete this
    // instance of PrintWebViewHelper.
    new printing::PrintRenderFrameHelper(
            render_frame,
            std::unique_ptr<printing::PrintRenderFrameHelper::Delegate>(
                printing::PrintRenderFrameHelper::CreateEmptyDelegate()));



}

void ContentRendererClientImpl::PrepareErrorPage(
    content::RenderFrame* render_frame,
    const blink::WebURLRequest& failed_request,
    const blink::WebURLError& error,
    std::string* error_html)
{
    GURL gurl = failed_request.Url();

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

content::ResourceLoaderBridge*
ContentRendererClientImpl::OverrideResourceLoaderBridge(
    const network::ResourceRequest *request)
{
    StringRef url = request->url.spec();

    if (!Statics::inProcessResourceLoader ||
        !Statics::inProcessResourceLoader->canHandleURL(url))
    {
        return nullptr;
    }
    return new InProcessResourceLoaderBridge(request);
}

bool ContentRendererClientImpl::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin)
{
    if (params.mime_type.Ascii() != "application/x-bloomberg-jswidget") {
        return false;
    }

    *plugin = new JsWidget(render_frame->GetWebFrame());
    return true;
}

bool ContentRendererClientImpl::Dispatch(IPC::Message *msg)
{
    if (Statics::rendererUIEnabled &&
        RenderMessageDelegate::GetInstance()->OnMessageReceived(*msg)) {
        delete msg;
        return true;
    }

    return false;
}

bool ContentRendererClientImpl::BindFrameSinkProvider(
    content::mojom::FrameSinkProviderRequest request)
{
    if (Statics::rendererUIEnabled) {
        RenderCompositorFactory::GetInstance()->Bind(std::move(request));
        return true;
    }

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
    // needed for chrome services
    d_service_context = std::make_unique<service_manager::ServiceContext>(
        std::make_unique<blpwtk2::ForwardingService>(this),
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

void ForwardingService::SetContext(service_manager::ServiceContext* context) {
  target_->SetContext(context);
}

}  // close namespace blpwtk2

// vim: ts=4 et
