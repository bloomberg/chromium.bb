// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_CONNECTOR_REGISTRY_H_
#define MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_CONNECTOR_REGISTRY_H_

#include <map>
#include <string>

#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

class ApplicationConnection;
class ServiceConnector;

namespace internal {

// ServiceConnectorRegistry maintains a default ServiceConnector as well as at
// most one ServiceConnector per interface name. When ConnectToService() is
// invoked the ServiceConnector registered by name is given the request. If
// a ServiceConnector has not been registered by name than the default
// ServiceConnector is given the request.
class ServiceConnectorRegistry {
 public:
  ServiceConnectorRegistry();
  ~ServiceConnectorRegistry();

  // Sets the default ServiceConnector. ServiceConnectorRegistry does *not*
  // take ownership of |service_connector|.
  void set_service_connector(ServiceConnector* service_connector) {
    service_connector_ = service_connector;
  }

  // Returns true if non ServiceConnectors have been registered by name.
  bool empty() const { return name_to_service_connector_.empty(); }

  // Sets a ServiceConnector by name. This deletes the existing ServiceConnector
  // and takes ownership of |service_connector|.
  void SetServiceConnectorForName(ServiceConnector* service_connector,
                                  const std::string& interface_name);
  void RemoveServiceConnectorForName(const std::string& interface_name);

  void ConnectToService(ApplicationConnection* application_connection,
                        const std::string& interface_name,
                        ScopedMessagePipeHandle client_handle);

 private:
  using NameToServiceConnectorMap = std::map<std::string, ServiceConnector*>;

  ServiceConnector* service_connector_;

  NameToServiceConnectorMap name_to_service_connector_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ServiceConnectorRegistry);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_CONNECTOR_REGISTRY_H_
