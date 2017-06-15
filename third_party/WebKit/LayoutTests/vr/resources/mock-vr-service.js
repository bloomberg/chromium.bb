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
      this.presentation_provider_ = new MockVRPresentationProvider();

      interfaceProvider.addInterfaceOverrideForTesting(
          vr_service.VRDisplay.name,
          handle => this.bindingSet_.addBinding(this, handle));

      if (service.client_) {
        this.notifyClientOfDisplay();
      }
    }

    requestPresent(secureOrigin, submitFrameClient, request) {
      this.presentation_provider_.bind(submitFrameClient, request);
      return Promise.resolve({success: true});
    }

    setPose(pose) {
      if (pose == null) {
        this.presentation_provider_.pose_ = null;
      } else {
        this.presentation_provider_.initPose();
        this.presentation_provider_.fillPose(pose);
      }
    }

    getNextMagicWindowPose() {
      return Promise.resolve({
        pose: this.presentation_provider_.pose_,
      });
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

  class MockVRPresentationProvider {
    constructor() {
      this.timeDelta_ = 0;
      this.binding_ = new bindings.Binding(vr_service.VRPresentationProvider,
          this);
      this.pose_ = null;
    }
    bind(client, request) {
      this.submitFrameClient_ = client;
      this.binding_.close();
      this.binding_.bind(request);
    }
    submitFrame(frameId, mailboxHolder) {
      // Trigger the submit completion callbacks here. WARNING: The
      // Javascript-based mojo mocks are *not* re-entrant.  In the current
      // default implementation, Javascript calls display.submitFrame, and the
      // corresponding C++ code uses a reentrant mojo call that waits for
      // onSubmitFrameTransferred to indicate completion. This never finishes
      // when using the mocks since the incoming calls are queued until the
      // current execution context finishes. As a workaround, use the alternate
      // "WebVRExperimentalRendering" mode which works without reentrant calls,
      // the code only checks for completion on the *next* frame, see the
      // corresponding option setting in RuntimeEnabledFeatures.json5.
      this.submitFrameClient_.onSubmitFrameTransferred();
      this.submitFrameClient_.onSubmitFrameRendered();
    }
    getVSync() {
      if (this.pose_) {
        this.pose_.poseIndex++;
      }

      let retval = Promise.resolve({
        pose: this.pose_,
        time: {
          microseconds: this.timeDelta_,
        },
        frame_id: 0,
        status: vr_service.VRPresentationProvider.VSyncStatus.SUCCESS,
      });

      this.timeDelta_ += 1000.0 / 60.0;
      return retval;
    }
    initPose() {
      this.pose_ = {
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
      return Promise.resolve({number_of_connected_devices: device_number});
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
