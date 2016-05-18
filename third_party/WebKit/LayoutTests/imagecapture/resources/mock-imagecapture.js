"use strict";

let mockImageCaptureReady = define(
  'mockImageCapture',
  ['third_party/WebKit/public/platform/modules/imagecapture/image_capture.mojom',
   'mojo/public/js/bindings',
   'mojo/public/js/connection',
   'content/public/renderer/service_registry',
  ], (imageCapture, bindings, connection, serviceRegistry) => {

  class MockImageCapture {
    constructor() {
      serviceRegistry.addServiceOverrideForTesting(
          imageCapture.ImageCapture.name,
          pipe => this.bindToPipe(pipe));
      this.getCapabilitiesCallback_  = null;
    }

    bindToPipe(pipe) {
      this.stub_ = connection.bindHandleToStub(
          pipe, imageCapture.ImageCapture);
      bindings.StubBindings(this.stub_).delegate = this;
    }

    getCapabilities(source_id) {
      const response =
          { capabilities : { zoom : { min : 0, max : 10, initial : 5 } } };
      if (this.getCapabilitiesCallback_) {
        // Give the time needed for the Mojo response to ripple back to Blink.
        setTimeout(this.getCapabilitiesCallback_, 0, response.capabilities);
        //this.getCapabilitiesCallback_(response.capabilities)
      }
      return Promise.resolve(response);
    }

    takePhoto(source_id) {
      return Promise.resolve({ mime_type : 'image/cat',
                               data : "(,,,)=(^.^)=(,,,)" });
    }

    waitForGetCapabilities() {
      return new Promise((resolve,reject) => {
         this.getCapabilitiesCallback_ = resolve;
      });
    }

  }
  return new MockImageCapture();
});
