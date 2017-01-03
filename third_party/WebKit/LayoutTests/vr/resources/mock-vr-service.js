'use strict';

let mockVRService = loadMojoModules(
    'mockVRService',
    ['mojo/public/js/bindings',
     'mojo/public/js/connection',
     'mojo/public/js/router',
     'device/vr/vr_service.mojom',
    ]).then(mojo => {
  let [bindings, connection, router, vr_service] = mojo.modules;

  class MockVRDisplay extends vr_service.VRDisplay.stubClass {
    constructor(interfaceProvider) {
      super();
      interfaceProvider.addInterfaceOverrideForTesting(
          vr_service.VRDisplay.name,
          handle => this.connect_(handle));
    }

    connect_(handle) {
      this.router_ = new router.Router(handle);
      this.router_.setIncomingReceiver(this);
    }
  }

  class MockVRService extends vr_service.VRService.stubClass {
    constructor(interfaceProvider) {
      super();
      interfaceProvider.addInterfaceOverrideForTesting(
          vr_service.VRService.name,
          handle => this.connect_(handle));
      this.vrDisplays_ = null;
    }

    connect_(handle) {
      this.router_ = new router.Router(handle);
      this.router_.setIncomingReceiver(this);
    }

    setVRDisplays(displays) {
      this.vrDisplays_ = displays;
    }

    setClient(client) {
      if (this.vrDisplays_ != null) {
        this.vrDisplays_.forEach(display => {
          var displayPtr = new vr_service.VRDisplayPtr();
          var request = bindings.makeRequest(displayPtr);
          var binding = new bindings.Binding(
              vr_service.VRDisplay,
              new MockVRDisplay(mojo.frameInterfaces), request);
          var client_handle = new bindings.InterfaceRequest(
            connection.bindProxy(proxy => {}, vr_service.VRDisplayClient));
          client.onDisplayConnected(displayPtr, client_handle, display);
        });
        return Promise.resolve(
            {numberOfConnectedDevices: this.vrDisplays_.length});
      }
      return Promise.resolve({numberOfConnectedDevices: 0});
    }
  }

  return new MockVRService(mojo.frameInterfaces);
});

function vr_test(func, vrDisplays, name, properties) {
  promise_test(t => mockVRService.then((service) => {
    service.setVRDisplays(vrDisplays);
    return func();
  }), name, properties);
}


