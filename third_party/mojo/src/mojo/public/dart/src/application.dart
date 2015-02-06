// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of application;

class _ApplicationImpl extends application_mojom.Application {
  shell_mojom.ShellProxy shell;
  Application _application;

  _ApplicationImpl(
      Application application, core.MojoMessagePipeEndpoint endpoint)
      : _application = application, super(endpoint) {
    super.delegate = this;
  }

  _ApplicationImpl.fromHandle(Application application, core.MojoHandle handle)
      : _application = application, super.fromHandle(handle) {
    super.delegate = this;
  }

  void initialize(shell_mojom.ShellProxy shellProxy, List<String> args) {
    assert(shell == null);
    shell = shellProxy;
    _application.initialize(args);
  }

  void acceptConnection(
      String requestorUrl,
      service_provider.ServiceProviderStub services,
      service_provider.ServiceProviderProxy exposedServices) =>
      _application._acceptConnection(requestorUrl, services, exposedServices);

  void requestQuit() => _application._requestQuitAndClose();

  void close({bool nodefer: false}) => shell.close();
}

// TODO(zra): Better documentation and examples.
// To implement, do the following:
// - Optionally override acceptConnection() if services are to be provided.
//   The override should assign a factory function to the passed in
//   ServiceProvider's |factory| field, and then call listen on the
//   ServiceProvider. The factory function should take a MojoMessagePipeEndpoint
//   and return an object that implements the requested interface.
// - Optionally override initialize() where needed.
// - Optionally override requestClose() to clean up state specific to your
//   application.
// To use an Application:
// - Call listen() on a newly created Application to begin providing services.
// - Call connectToService() to request services from the Shell.
// - Call close() to close connections to any requested ServiceProviders and the
//   Shell.
abstract class Application {
  _ApplicationImpl _applicationImpl;
  List<service_provider.ServiceProviderProxy> _proxies;
  List<ServiceProvider> _serviceProviders;

  Application(core.MojoMessagePipeEndpoint endpoint) {
    _proxies = [];
    _serviceProviders = [];
    _applicationImpl = new _ApplicationImpl(this, endpoint);
  }

  Application.fromHandle(core.MojoHandle appHandle) {
    _proxies = [];
    _serviceProviders = [];
    _applicationImpl = new _ApplicationImpl.fromHandle(this, appHandle);
  }

  void initialize(List<String> args) {}

  void connectToService(String url, bindings.Proxy proxy) {
    assert(!proxy.isBound);
    var endpoint = _connectToServiceHelper(url, proxy.name);
    proxy.bind(endpoint);
  }

  void requestQuit() {}

  listen() => _applicationImpl.listen();

  void _requestQuitAndClose() {
    requestQuit();
    close();
  }

  void close() {
    assert(_applicationImpl != null);
    _proxies.forEach((c) => c.close());
    _proxies.clear();
    _serviceProviders.forEach((sp) => sp.close());
    _serviceProviders.clear();
    _applicationImpl.close();
  }

  void _acceptConnection(
      String requestorUrl,
      service_provider.ServiceProviderStub services,
      service_provider.ServiceProviderProxy exposedServices) {
    var serviceProvider = new ServiceProvider(services, exposedServices);
    _serviceProviders.add(serviceProvider);
    acceptConnection(requestorUrl, serviceProvider);
  }

  void acceptConnection(String requestorUrl, ServiceProvider serviceProvider) {}

  core.MojoMessagePipeEndpoint _connectToServiceHelper(
      String url, String service) {
    var applicationPipe = new core.MojoMessagePipe();
    var proxyEndpoint = applicationPipe.endpoints[0];
    var applicationEndpoint = applicationPipe.endpoints[1];
    var serviceProviderProxy =
        new service_provider.ServiceProviderProxy.unbound();
    _applicationImpl.shell.connectToApplication(
        url, serviceProviderProxy, null);
    serviceProviderProxy.connectToService(service, applicationEndpoint);
    _proxies.add(serviceProviderProxy);
    return proxyEndpoint;
  }
}
