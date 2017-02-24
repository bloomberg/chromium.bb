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
      this.vsync_provider_ = new MockVRVSyncProvider();

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

    setPose(pose) {
      if (pose == null) {
        this.vsync_provider_.pose_ = null;
      } else {
        this.vsync_provider_.initPose();
        this.vsync_provider_.fillPose(pose);
      }
    }

    getVRVSyncProvider(request) { this.vsync_provider_.bind(request); }

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

  class MockVRVSyncProvider {
    constructor() {
      this.timeDelta_ = 0;
      this.binding_ = new bindings.Binding(vr_service.VRVSyncProvider, this);
      this.pose_ = null;
    }
    bind(request) {
      this.binding_.close();
      this.binding_.bind(request);
    }
    getVSync() {
      if (this.pose_) {
        this.pose_.timestamp = this.timeDelta_;
        this.pose_.poseIndex++;
      }

      let retval = Promise.resolve({
        pose: this.pose_,
        time: {
          microseconds: this.timeDelta_,
        },
        frameId: 0,
        error: vr_service.VRVSyncProvider.Status.SUCCESS,
      });

      this.timeDelta_ += 1000.0 / 60.0;
      return retval;
    }
    initPose() {
      this.pose_ = {
        timestamp: 0,
        orientation: null,
        position: null,
        angularVelocity: null,
        linearVelocity: null,
        angularAcceleration: null,
        linearAcceleration: null,
        poseIndex: 0
      };
    }
    fillPose(pose) {
      for (var field in pose) {
        if (this.pose_.hasOwnProperty(field)) {
          this.pose_[field] = pose[field];
        }
      }
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
