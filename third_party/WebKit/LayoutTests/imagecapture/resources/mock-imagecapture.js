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
          iso : { min : 100.0, max : 12000.0, current : 400.0, step : 1.0 },
          height : { min : 240.0, max : 2448.0, current : 240.0, step : 2.0 },
          width : { min : 320.0, max : 3264.0, current : 320.0, step : 3.0 },
          zoom : { min : 0.0, max : 10.0, current : 5.0, step : 5.0 },
          focus_mode : imageCapture.MeteringMode.MANUAL,
          exposure_mode : imageCapture.MeteringMode.SINGLE_SHOT,
          exposure_compensation :
              { min : -200.0, max : 200.0, current : 33.0, step : 33.0},
          white_balance_mode : imageCapture.MeteringMode.CONTINUOUS,
          fill_light_mode : imageCapture.FillLightMode.AUTO,
          red_eye_reduction : true,
          color_temperature :
              { min : 2500.0, max : 6500.0, current : 6000.0, step : 1000.0 },
          brightness : { min : 1.0, max : 10.0, current : 5.0, step : 1.0 },
          contrast : { min : 2.0, max : 9.0, current : 5.0, step : 1.0 },
          saturation : { min : 3.0, max : 8.0, current : 6.0, step : 1.0 },
          sharpness : { min : 4.0, max : 7.0, current : 7.0, step : 1.0 },
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
