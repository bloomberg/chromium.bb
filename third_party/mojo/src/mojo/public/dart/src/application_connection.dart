// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of application;

typedef Object ServiceFactory(core.MojoMessagePipeEndpoint endpoint);
typedef void FallbackServiceFactory(String interfaceName,
    core.MojoMessagePipeEndpoint endpoint);


class LocalServiceProvider implements ServiceProvider {
  final ApplicationConnection connection;
  ServiceProviderStub _stub;

  LocalServiceProvider(this.connection, this._stub) {
    _stub.delegate = this;
  }

  listen() => _stub.listen();

  void close({bool nodefer : false}) => _stub.close(nodefer: nodefer);

  void connectToService(String interfaceName,
      core.MojoMessagePipeEndpoint pipe) {
    if (connection._nameToServiceFactory.containsKey(interfaceName)) {
      connection._nameToServiceFactory[interfaceName](pipe);
      return;
    }
    if (connection.fallbackServiceFactory != null) {
      connection.fallbackServiceFactory(interfaceName, pipe);
      return;
    }
    // The specified interface isn't provided. Close the pipe so the
    // remote endpoint sees that we don't support this interface.
    pipe.handle.close();
  }
}

// Encapsulates a pair of ServiceProviders that enable services (interface
// implementations) to be provided to a remote application as well as exposing
// services provided by a remote application. ApplicationConnection
// objects are returned by the Application ConnectToApplication() method
// and they're passed to the Application AcceptConnection() method.
//
// To request a service from the remote application:
//   var proxy = applicationConnection.requestService(ViewManagerClientName);
//
// To provide a service to the remote application, specify a function that
// returns a service. For example:
//   applicationConnection.provideService(ViewManagerClientName, (pipe) =>
//       new ViewManagerClientImpl(pipe));
//
// To handle requests for any interface, set fallbackServiceFactory to a
// function that takes ownership of the incoming message pipe endpoint. If the
// fallbackServiceFactory function doesn't bind the pipe, it should close it.
//
// The fallbackServiceFactory is only used if a service wasn't specified
// with provideService().

class ApplicationConnection {
  ServiceProviderProxy remoteServiceProvider;
  LocalServiceProvider _localServiceProvider;
  final _nameToServiceFactory = new Map<String, ServiceFactory>();
  FallbackServiceFactory _fallbackServiceFactory;

  ApplicationConnection(ServiceProviderStub stub, ServiceProviderProxy proxy)
      : remoteServiceProvider = proxy {
    if (stub != null) _localServiceProvider =
        new LocalServiceProvider(this, stub);
  }

  FallbackServiceFactory get fallbackServiceFactory => _fallbackServiceFactory;
                         set fallbackServiceFactory(FallbackServiceFactory f) {
    assert(_localServiceProvider != null);
    _fallbackServiceFactory = f;
  }

  bindings.ProxyBase requestService(bindings.ProxyBase proxy) {
    assert(!proxy.impl.isBound &&
        (remoteServiceProvider != null) &&
        remoteServiceProvider.impl.isBound);
    var pipe = new core.MojoMessagePipe();
    proxy.impl.bind(pipe.endpoints[0]);
    remoteServiceProvider.ptr.connectToService(
        proxy.name, pipe.endpoints[1]);
    return proxy;
  }

  void provideService(String interfaceName, ServiceFactory factory) {
    assert(_localServiceProvider != null);
    _nameToServiceFactory[interfaceName] = factory;
  }

  void listen() {
    assert(_localServiceProvider != null);
    _localServiceProvider.listen();
  }

  void close({bool nodefer: false}) {
    if (remoteServiceProvider != null) {
      remoteServiceProvider.close();
      remoteServiceProvider = null;
    }
    if (_localServiceProvider != null) {
      _localServiceProvider.close(nodefer: nodefer);
      _localServiceProvider = null;
    }

  }
}
