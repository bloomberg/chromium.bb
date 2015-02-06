// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of application;

typedef core.Listener ListenerFactory(core.MojoMessagePipeEndpoint endpoint);

class ServiceProvider extends service_provider.ServiceProvider {
  ListenerFactory factory;

  service_provider.ServiceProviderProxy _proxy;

  ServiceProvider(
      service_provider.ServiceProviderStub services,
      [service_provider.ServiceProviderProxy exposedServices = null])
      : _proxy = exposedServices,
        super.fromStub(services) {
    delegate = this;
  }

  connectToService(String interfaceName, core.MojoMessagePipeEndpoint pipe) =>
      factory(pipe).listen();

  requestService(String name, bindings.Proxy clientImpl) {
    assert(_proxy != null);
    assert(!clientImpl.isBound);
    var pipe = new core.MojoMessagePipe();
    clientImpl.bind(pipe.endpoints[0]);
    _proxy.connectToService(name, pipe.endpoints[1]);
  }

  close({bool nodefer : false}) {
    if (_proxy != null) {
      _proxy.close();
      _proxy = null;
    }
    super.close(nodefer: nodefer);
  }
}
