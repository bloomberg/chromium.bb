'use strict';

class MockVRDisplay {
  constructor(displayInfo, service) {
    this.bindingSet_ = new mojo.BindingSet(device.mojom.VRDisplay);
    this.displayClient_ = new device.mojom.VRDisplayClientPtr();
    this.displayInfo_ = displayInfo;
    this.service_ = service;
    this.presentation_provider_ = new MockVRPresentationProvider();

    if (service.client_) {
      this.notifyClientOfDisplay();
    }
  }

  requestPresent(submitFrameClient, request) {
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
    let displayPtr = new device.mojom.VRDisplayPtr();
    let request = mojo.makeRequest(displayPtr);
    let binding = new mojo.Binding(device.mojom.VRDisplay, this, request);
    let clientRequest = mojo.makeRequest(this.displayClient_);
    this.service_.client_.onDisplayConnected(displayPtr, clientRequest,
        this.displayInfo_);
  }
}

class MockVRPresentationProvider {
  constructor() {
    this.binding_ = new mojo.Binding(device.mojom.VRPresentationProvider, this);
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

    // Convert current document time to monotonic time.
    var now = window.performance.now() / 1000.0;
    var diff =
        now - window.internals.monotonicTimeToZeroBasedDocumentTime(now);
    now += diff;
    now *= 1000000;

    let retval = Promise.resolve({
      pose: this.pose_,
      time: {
        microseconds: now,
      },
      frameId: 0,
      status: device.mojom.VRPresentationProvider.VSyncStatus.SUCCESS,
    });

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
  constructor() {
    this.bindingSet_ = new mojo.BindingSet(device.mojom.VRService);
    this.mockVRDisplays_ = [];

    this.interceptor_ =
        new MojoInterfaceInterceptor(device.mojom.VRService.name);
    this.interceptor_.oninterfacerequest =
        e => this.bindingSet_.addBinding(this, e.handle);
    this.interceptor_.start();
  }

  setVRDisplays(displays) {
    this.mockVRDisplays_ = [];
    for (let i = 0; i < displays.length; i++) {
      displays[i].index = i;
      this.mockVRDisplays_.push(new MockVRDisplay(displays[i], this));
    }
  }

  addVRDisplay(display) {
    if (this.mockVRDisplays_.length) {
      display.index =
          this.mockVRDisplays_[this.mockVRDisplays_.length - 1] + 1;
    } else {
      display.index = 0;
    }
    this.mockVRDisplays_.push(new MockVRDisplay(display, this));
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

let mockVRService = new MockVRService(mojo.frameInterfaces);

function vr_test(func, vrDisplays, name, properties) {
  mockVRService.setVRDisplays(vrDisplays);
  let t = async_test(name, properties);
  func(t, mockVRService);
}
