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
    }

    bindToPipe(pipe) {
      this.stub_ = connection.bindHandleToStub(
          pipe, imageCapture.ImageCapture);
      bindings.StubBindings(this.stub_).delegate = this;
    }

    takePhoto(sourceid) {
      return Promise.resolve({ mime_type : 'image/cat',
                               data : "(,,,)=(^.^)=(,,,)" });
    }
  }
  return new MockImageCapture();
});
