// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_APPLICATION_APPLICATION_CONNECTION_H_
#define MOJO_PUBLIC_APPLICATION_APPLICATION_CONNECTION_H_

#include <string>

#include "mojo/public/cpp/application/lib/service_connector.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"

namespace mojo {

// An instance of this class is passed to
// ApplicationDelegate's ConfigureIncomingConnection() method each time a
// connection is made to this app, and to ApplicationDelegate's
// ConfigureOutgoingConnection() method when the app connects to
// another.
//
// To use define a class that implements your specific service api, e.g. FooImpl
// to implement a service named Foo.
// That class must subclass an InterfaceImpl specialization.
//
// Then implement an InterfaceFactory<Foo> that binds instances of FooImpl to
// InterfaceRequest<Foo>s and register that on the connection.
//
// connection->AddService(&factory);
//
// Or if you have multiple factories implemented by the same type, explicitly
// specify the interface to register the factory for:
//
// connection->AddService<Foo>(&my_foo_and_bar_factory_);
// connection->AddService<Bar>(&my_foo_and_bar_factory_);
//
// The InterfaceFactory must outlive the ApplicationConnection.
class ApplicationConnection {
 public:
  virtual ~ApplicationConnection();

  template <typename Interface>
  void AddService(InterfaceFactory<Interface>* factory) {
    AddServiceConnector(
        new internal::InterfaceFactoryConnector<Interface>(factory));
  }

  // Connect to the service implementing |Interface|.
  template <typename Interface>
  void ConnectToService(InterfacePtr<Interface>* ptr) {
    if (ServiceProvider* sp = GetServiceProvider()) {
      MessagePipe pipe;
      ptr->Bind(pipe.handle0.Pass());
      sp->ConnectToService(Interface::Name_, pipe.handle1.Pass());
    }
  }

  // The url identifying the application on the other end of this connection.
  virtual const std::string& GetRemoteApplicationURL() = 0;

  // Raw ServiceProvider interface to remote application.
  virtual ServiceProvider* GetServiceProvider() = 0;

 private:
  virtual void AddServiceConnector(
      internal::ServiceConnectorBase* service_connector) = 0;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_APPLICATION_APPLICATION_CONNECTION_H_
