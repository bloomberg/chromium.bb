'use strict';

let mockVRService = loadMojoModules(
    'mockVRService',
    ['mojo/public/js/bindings',
     'device/vr/vr_service.mojom',
    ]).then(mojo => {
  let [bindings, vr_service] = mojo.modules;

  class MockVRDisplay {
    constructor(interfaceProvider, displayInfo, service) {
      this.bindingSet_ = new bindings.BindingSet(vr_service.VRDisplay);
      this.displayClient_ = new vr_service.VRDisplayClientPtr();
      this.displayInfo_ = displayInfo;
      this.service_ = service;

      interfaceProvider.addInterfaceOverrideForTesting(
          vr_service.VRDisplay.name,
          handle => this.bindingSet_.addBinding(this, handle));

      if (service.client_) {
        this.notifyClientOfDisplay();
      }
    }

    requestPresent(secureOrigin) {
      return Promise.resolve({success: true});
    }

    forceActivate(reason) {
      this.displayClient_.onActivate(reason);
    }

    notifyClientOfDisplay() {
      let displayPtr = new vr_service.VRDisplayPtr();
      let request = bindings.makeRequest(displayPtr);
      let binding = new bindings.Binding(
          vr_service.VRDisplay,
          this, request);
      let clientRequest = bindings.makeRequest(this.displayClient_);
      this.service_.client_.onDisplayConnected(displayPtr, clientRequest,
          this.displayInfo_);
    }
  }

  class MockVRService {
    constructor(interfaceProvider) {
      this.bindingSet_ = new bindings.BindingSet(vr_service.VRService);
      this.mockVRDisplays_ = [];

      interfaceProvider.addInterfaceOverrideForTesting(
          vr_service.VRService.name,
          handle => this.bindingSet_.addBinding(this, handle));
    }

    setVRDisplays(displays) {
      this.mockVRDisplays_ = [];
      for (let i = 0; i < displays.length; i++) {
        displays[i].index = i;
        this.mockVRDisplays_.push(new MockVRDisplay(mojo.frameInterfaces,
              displays[i], this));
      }
    }

    addVRDisplay(display) {
      if (this.mockVRDisplays_.length) {
        display.index =
            this.mockVRDisplays_[this.mockVRDisplays_.length - 1] + 1;
      } else {
        display.index = 0;
      }
      this.mockVRDisplays_.push(new MockVRDisplay(mojo.frameInterfaces,
            display, this));
    }

    setClient(client) {
      this.client_ = client;
      for (let i = 0; i < this.mockVRDisplays_.length; i++) {
        this.mockVRDisplays_[i].notifyClientOfDisplay();
      }

      let device_number = this.mockVRDisplays_.length;
      return Promise.resolve({numberOfConnectedDevices: device_number});
    }
  }

  return new MockVRService(mojo.frameInterfaces);
});

function vr_test(func, vrDisplays, name, properties) {
  mockVRService.then( (service) => {
    service.setVRDisplays(vrDisplays);
    let t = async_test(name, properties);
    func(t, service);
  });
}
