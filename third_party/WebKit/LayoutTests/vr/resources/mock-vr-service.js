'use strict';

let mockVRService = loadMojoModules(
    'mockVRService',
    ['mojo/public/js/bindings',
     'device/vr/vr_service.mojom',
    ]).then(mojo => {
  let [bindings, vr_service] = mojo.modules;

  class MockVRDisplay {
    constructor(interfaceProvider) {
      this.bindingSet_ = new bindings.BindingSet(vr_service.VRDisplay);

      interfaceProvider.addInterfaceOverrideForTesting(
          vr_service.VRDisplay.name,
          handle => this.bindingSet_.addBinding(this, handle));
    }

    requestPresent(secureOrigin) {
      return Promise.resolve({success: true});
    }
  }

  class MockVRService {
    constructor(interfaceProvider) {
      this.bindingSet_ = new bindings.BindingSet(vr_service.VRService);
      this.displayClient_ = new vr_service.VRDisplayClientPtr();
      this.vrDisplays_ = null;

      interfaceProvider.addInterfaceOverrideForTesting(
          vr_service.VRService.name,
          handle => this.bindingSet_.addBinding(this, handle));
    }

    setVRDisplays(displays) {
      for (let i = 0; i < displays.length; i++) {
        displays[i].index = i;
      }
      this.vrDisplays_ = displays;
    }

    notifyClientOfDisplays() {
      if (this.vrDisplays_ == null) {
        return;
      }
      for (let i = 0; i < this.vrDisplays_.length; i++) {
        let displayPtr = new vr_service.VRDisplayPtr();
        let request = bindings.makeRequest(displayPtr);
        let binding = new bindings.Binding(
            vr_service.VRDisplay,
            new MockVRDisplay(mojo.frameInterfaces), request);
        let clientRequest = bindings.makeRequest(this.displayClient_);
        this.client_.onDisplayConnected(displayPtr, clientRequest, this.vrDisplays_[i]);
      }
    }

    setClient(client) {
      this.client_ = client;
      this.notifyClientOfDisplays();

      var device_number = (this.vrDisplays_== null ? 0 : this.vrDisplays_.length);
      return Promise.resolve({numberOfConnectedDevices: device_number});
    }
  }

  return new MockVRService(mojo.frameInterfaces);
});

function vr_test(func, vrDisplays, name, properties) {
  mockVRService.then( (service) => {
    service.setVRDisplays(vrDisplays);
    let t = async_test(name, properties);
    func(t);
  });
}
