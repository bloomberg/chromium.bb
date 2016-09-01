"use strict";

let mockImageCaptureReady = define(
  'mockImageCapture',
  ['media/mojo/interfaces/image_capture.mojom',
   'mojo/public/js/bindings',
   'mojo/public/js/connection',
   'content/public/renderer/interfaces',
  ], (imageCapture, bindings, connection, interfaces) => {

  class MockImageCapture {
    constructor() {
      interfaces.addInterfaceOverrideForTesting(
          imageCapture.ImageCapture.name,
          pipe => this.bindToPipe(pipe));

      this.capabilities_ = { capabilities : {
          iso : { min : 100, max : 12000, current : 400 },
          height : { min : 240, max : 2448, current : 240 },
          width : { min : 320, max : 3264, current : 320 },
          width : { min : 320, max : 3264, current : 320 },
          zoom : { min : 0, max : 10, current : 5 },
          focus_mode : imageCapture.MeteringMode.MANUAL,
          exposure_mode : imageCapture.MeteringMode.SINGLE_SHOT,
          exposure_compensation : { min : -200, max : 200, current : 33},
          white_balance_mode : imageCapture.MeteringMode.CONTINUOUS,
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
      return Promise.resolve({ blob : { mime_type : 'image/cat',
                                        data : new Array(2) } });
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
