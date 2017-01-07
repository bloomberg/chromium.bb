// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceConnector_h
#define ServiceConnector_h

#include "mojo/public/cpp/bindings/binding.h"
#include "public/platform/Platform.h"
#include "services/service_manager/public/interfaces/connector.mojom-blink.h"
#include "wtf/StdLibExtras.h"

namespace blink {

// ServiceConnector supports connecting to Mojo services by name.
class ServiceConnector {
 public:
  static ServiceConnector& instance() {
    DEFINE_STATIC_LOCAL(ServiceConnector, instance, ());
    return instance;
  }

  template <typename Interface>
  void connectToInterface(const char* serviceName,
                          mojo::InterfaceRequest<Interface> request) {
    if (!m_connector.is_bound()) {
      Platform::current()->bindServiceConnector(
          mojo::MakeRequest(&m_connector).PassMessagePipe());
    }

    if (m_connector.encountered_error())
      return;

    service_manager::mojom::blink::IdentityPtr remoteIdentity(
        service_manager::mojom::blink::Identity::New());
    remoteIdentity->name = serviceName;
    remoteIdentity->user_id = service_manager::mojom::blink::kInheritUserID;
    remoteIdentity->instance = "";

    service_manager::mojom::blink::InterfaceProviderPtr remoteInterfaces;
    m_connector->Connect(std::move(remoteIdentity),
                         MakeRequest(&remoteInterfaces),
                         base::Bind(&ServiceConnector::onConnectionCompleted));

    remoteInterfaces->GetInterface(Interface::Name_, request.PassMessagePipe());
  }

 private:
  ServiceConnector() {}

  static void onConnectionCompleted(
      service_manager::mojom::ConnectResult result,
      const WTF::String& targetUserID) {}

  service_manager::mojom::blink::ConnectorPtr m_connector;
};

}  // namespace blink

#endif  // ServiceConnector_h
