// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_BRIDGE_FACTORY_IMPL_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_BRIDGE_FACTORY_IMPL_H_

#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "components/remote_cocoa/common/alert.mojom.h"
#include "components/remote_cocoa/common/bridge_factory.mojom.h"
#include "components/remote_cocoa/common/bridged_native_widget.mojom.h"
#include "components/remote_cocoa/common/bridged_native_widget_host.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

// TODO(ccameron): This file is to be moved to /components/remote_cocoa/app_shim
// when possible. For now, put it in the namespace of that path.
namespace remote_cocoa {

// The factory that creates BridgedNativeWidget instances. This object is to
// be instantiated in app shim processes.
class REMOTE_COCOA_APP_SHIM_EXPORT BridgeFactoryImpl
    : public mojom::BridgeFactory {
 public:
  static BridgeFactoryImpl* Get();
  void BindRequest(mojom::BridgeFactoryAssociatedRequest request);

  // Set callbacks to create content types (content types cannot be created
  // in remote_cocoa).
  // TODO(https://crbug.com/888290): Move these types from content to
  // remote_cocoa.
  using RenderWidgetHostNSViewCreateCallback = base::RepeatingCallback<void(
      mojo::ScopedInterfaceEndpointHandle host_handle,
      mojo::ScopedInterfaceEndpointHandle view_request_handle)>;
  using WebContentsNSViewCreateCallback = base::RepeatingCallback<void(
      uint64_t view_id,
      mojo::ScopedInterfaceEndpointHandle host_handle,
      mojo::ScopedInterfaceEndpointHandle view_request_handle)>;
  void SetContentNSViewCreateCallbacks(
      RenderWidgetHostNSViewCreateCallback render_widget_host_create_callback,
      WebContentsNSViewCreateCallback web_conents_create_callback);

  // mojom::BridgeFactory:
  void CreateAlert(mojom::AlertBridgeRequest bridge_request) override;
  void CreateBridgedNativeWidget(
      uint64_t bridge_id,
      mojom::BridgedNativeWidgetAssociatedRequest bridge_request,
      mojom::BridgedNativeWidgetHostAssociatedPtrInfo host,
      mojom::TextInputHostAssociatedPtrInfo text_input_host) override;
  void CreateRenderWidgetHostNSView(
      mojom::StubInterfaceAssociatedPtrInfo host,
      mojom::StubInterfaceAssociatedRequest view_request) override;
  void CreateWebContentsNSView(
      uint64_t view_id,
      mojom::StubInterfaceAssociatedPtrInfo host,
      mojom::StubInterfaceAssociatedRequest view_request) override;

 private:
  friend class base::NoDestructor<BridgeFactoryImpl>;
  BridgeFactoryImpl();
  ~BridgeFactoryImpl() override;

  RenderWidgetHostNSViewCreateCallback render_widget_host_create_callback_;
  WebContentsNSViewCreateCallback web_conents_create_callback_;

  mojo::AssociatedBinding<mojom::BridgeFactory> binding_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_BRIDGE_FACTORY_IMPL_H_
