"use strict";

let mockImageCaptureReady = define(
  'mockImageCapture',
  ['media/mojo/interfaces/image_capture.mojom',
   'mojo/public/js/bindings',
   'mojo/public/js/connection',
   'content/public/renderer/service_registry',
  ], (imageCapture, bindings, connection, serviceRegistry) => {

  class MockImageCapture {
    constructor() {
      serviceRegistry.addServiceOverrideForTesting(
          imageCapture.ImageCapture.name,
          pipe => this.bindToPipe(pipe));

      this.capabilities_ = { capabilities: { zoom : { min : 0, max : 10, current : 5 } } };
      this.settings_ = null;
    }

    bindToPipe(pipe) {
      this.stub_ = connection.bindHandleToStub(pipe, imageCapture.ImageCapture);
      bindings.StubBindings(this.stub_).delegate = this;
    }

    getCapabilities(source_id) {
      return Promise.resolve(this.capabilities_);
    }

    setOptions(source_id, settings) {
      this.settings_ = settings;
      return Promise.resolve({ success : true });
    }

    takePhoto(source_id) {
      return Promise.resolve({ mime_type : 'image/cat',
                               data : "(,,,)=(^.^)=(,,,)" });
    }

    capabilities() {
      return this.capabilities_.capabilities;
    }

    options() {
      return this.settings_;
    }

  }
  return new MockImageCapture();
});
