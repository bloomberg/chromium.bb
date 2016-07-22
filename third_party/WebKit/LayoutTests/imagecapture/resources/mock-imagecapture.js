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

      this.capabilities_ = { capabilities : {
          iso : { min : 100, max : 12000, current : 400 },
          height : { min : 240, max : 2448, current : 240 },
          width : { min : 320, max : 3264, current : 320 },
          width : { min : 320, max : 3264, current : 320 },
          zoom : { min : 0, max : 10, current : 5 },
          focusMode : "unavailable",
      }};
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
