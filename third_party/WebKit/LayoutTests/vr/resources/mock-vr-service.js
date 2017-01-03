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

    requestPresent(secureOrigin) {
      return Promise.resolve({success: true});
    }
  }

  class MockVRService extends vr_service.VRService.stubClass {
    constructor(interfaceProvider) {
      super();
      interfaceProvider.addInterfaceOverrideForTesting(
          vr_service.VRService.name,
          handle => this.connect_(handle));
      this.vr_displays_ = null;
    }

    connect_(handle) {
      this.router_ = new router.Router(handle);
      this.router_.setIncomingReceiver(this);
    }

    setVRDisplays(displays) {
      for (let i = 0; i < displays.length; i++) {
        displays[i].index = i;
      }
      this.vr_displays_ = displays;
    }

    notifyClientOfDisplays() {
      if (this.vr_displays_ == null) {
        return;
      }
      for (let i = 0; i < this.vr_displays_.length; i++) {
        let displayPtr = new vr_service.VRDisplayPtr();
        let request = bindings.makeRequest(displayPtr);
        let binding = new bindings.Binding(
            vr_service.VRDisplay,
            new MockVRDisplay(mojo.frameInterfaces), request);
        let client_handle = new bindings.InterfaceRequest(
          connection.bindProxy(proxy => {
            this.displayClient_ = proxy;
        }, vr_service.VRDisplayClient));
        this.client_.onDisplayConnected(displayPtr, client_handle, this.vr_displays_[i]);
      }
    }

    setClient(client) {
      this.client_ = client;
      this.notifyClientOfDisplays();

      var device_number = (this.vr_displays_== null ? 0 : this.vr_displays_.length);
      return Promise.resolve({numberOfConnectedDevices: device_number});
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
