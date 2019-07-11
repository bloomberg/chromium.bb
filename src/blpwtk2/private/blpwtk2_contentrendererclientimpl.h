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

#ifndef INCLUDED_BLPWTK2_CONTENTRENDERERCLIENTIMPL_H
#define INCLUDED_BLPWTK2_CONTENTRENDERERCLIENTIMPL_H

#include <blpwtk2_config.h>

#include <content/public/renderer/content_renderer_client.h>
#include <content/public/renderer/render_thread_observer.h>
#include <services/service_manager/public/cpp/binder_registry.h>
#include <services/service_manager/public/cpp/connector.h>
#include <services/service_manager/public/cpp/service.h>
#include <services/service_manager/public/cpp/local_interface_provider.h>

namespace service_manager {
class ServiceBinding;
struct BindSourceInfo;
}

namespace blpwtk2 {
class ForwardingService;

                        // ===============================
                        // class ContentRendererClientImpl
                        // ===============================

// This interface allows us to add hooks to the "renderer" portion of the
// content module.  This is created during the startup process.
class ContentRendererClientImpl : public content::ContentRendererClient,
                                  public service_manager::Service,
                                  public service_manager::LocalInterfaceProvider
{
  public:
    ContentRendererClientImpl();
    ~ContentRendererClientImpl() final;

    void RenderViewCreated(content::RenderView *render_view) override;
        // Notifies that a new RenderView has been created.

    void RenderFrameCreated(content::RenderFrame *render_frame) override;

    void PrepareErrorPage(content::RenderFrame* render_frame,
                          const blink::WebURLError& error,
                          const std::string& http_method,
                          bool ignoring_cache,
                          std::string* error_html) override;
    // Returns the information to display when a navigation error occurs.
    // If |error_html| is not null then it may be set to a HTML page
    // containing the details of the error and maybe links to more info.
    // If |error_description| is not null it may be set to contain a brief
    // message describing the error that has occurred.
    // Either of the out parameters may be not written to in certain cases
    // (lack of information on the error code) so the caller should take
    // care to initialize the string values with safe defaults before the
    // call.

    std::unique_ptr<content::ResourceLoaderBridge> OverrideResourceLoaderBridge(
        const content::ResourceRequestInfoProvider& request_info) override;
    // Allows the embedder to override the ResourceLoaderBridge used.
    // If it returns NULL, the content layer will use the default loader.

    bool OverrideCreatePlugin(
        content::RenderFrame           *render_frame,
        const blink::WebPluginParams&   params,
        blink::WebPlugin              **plugin) override;
        // Allows the embedder to override creating a plugin. If it returns
        // true, then |plugin| will contain the created plugin, although it
        // could be NULL. If it returns false, the content layer will create
        // the plugin.
  private:
    // service_manager::Service:
    void OnBindInterface(const service_manager::BindSourceInfo& source,
                         const std::string& interface_name,
                         mojo::ScopedMessagePipeHandle interface_pipe) override;
    void OnStart() override;

    // service_manager::LocalInterfaceProvider:
    void GetInterface(const std::string& name,
                      mojo::ScopedMessagePipeHandle request_handle) override;

    // content::ContentRendererClient:
    void CreateRendererService(
        service_manager::mojom::ServiceRequest service_request) override;

    service_manager::Connector* GetConnector();

    service_manager::BinderRegistry d_registry;
    std::unique_ptr<service_manager::Connector> d_connector;
    service_manager::mojom::ConnectorRequest d_connector_request;
    std::unique_ptr<blpwtk2::ForwardingService> d_forward_service;
    std::unique_ptr<service_manager::ServiceBinding> d_service_binding;

    DISALLOW_COPY_AND_ASSIGN(ContentRendererClientImpl);
};

class ForwardingService : public service_manager::Service {
 public:
  // |target| must outlive this object.
  explicit ForwardingService(service_manager::Service* target);
  ~ForwardingService() override;

  // Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;
  bool OnServiceManagerConnectionLost() override;

 private:
  service_manager::Service* const target_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ForwardingService);
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_CONTENTRENDERERCLIENTIMPL_H

// vim: ts=4 et

