"use strict";

let mockImageCaptureReady = define(
  'mockImageCapture',
  ['media/capture/mojo/image_capture.mojom',
   'mojo/public/js/bindings',
   'content/public/renderer/interfaces',
  ], (imageCapture, bindings, interfaces) => {

  class MockImageCapture {
    constructor() {
      interfaces.addInterfaceOverrideForTesting(
          imageCapture.ImageCapture.name,
          handle => this.bindingSet_.addBinding(this, handle));

      this.state_ = { capabilities : {
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
          points_of_interest : [],
      }};
      this.settings_ = null;
      this.bindingSet_ = new bindings.BindingSet(imageCapture.ImageCapture);
    }

    getCapabilities(source_id) {
      return Promise.resolve(this.state_);
    }

    setOptions(source_id, settings) {
      this.settings_ = settings;
      if (settings.has_iso)
        this.state_.capabilities.iso.current = settings.iso;
      if (settings.has_height)
        this.state_.capabilities.height.current = settings.height;
      if (settings.has_width)
        this.state_.capabilities.width.current = settings.width;
      if (settings.has_zoom)
        this.state_.capabilities.zoom.current = settings.zoom;
      if (settings.has_focus_mode)
        this.state_.capabilities.focus_mode = settings.focus_mode;

      if (settings.points_of_interest.length > 0) {
        this.state_.capabilities.points_of_interest =
            settings.points_of_interest;
      }

      if (settings.has_exposure_mode)
        this.state_.capabilities.exposure_mode = settings.exposure_mode;

      if (settings.has_exposure_compensation) {
        this.state_.capabilities.exposure_compensation.current =
            settings.exposure_compensation;
      }
      if (settings.has_white_balance_mode) {
        this.state_.capabilities.white_balance_mode =
            settings.white_balance_mode;
      }
      if (settings.has_fill_light_mode)
        this.state_.capabilities.fill_light_mode = settings.fill_light_mode;
      if (settings.has_red_eye_reduction)
        this.state_.capabilities.red_eye_reduction = settings.red_eye_reduction;
      if (settings.has_color_temperature) {
        this.state_.capabilities.color_temperature.current =
            settings.color_temperature;
      }
      if (settings.has_brightness)
        this.state_.capabilities.brightness.current = settings.brightness;
      if (settings.has_contrast)
        this.state_.capabilities.contrast.current = settings.contrast;
      if (settings.has_saturation)
        this.state_.capabilities.saturation.current = settings.saturation;
      if (settings.has_sharpness)
        this.state_.capabilities.sharpness.current = settings.sharpness;

      return Promise.resolve({ success : true });
    }

    takePhoto(source_id) {
      return Promise.resolve({ blob : { mime_type : 'image/cat',
                                        data : new Array(2) } });
    }

    capabilities() {
      return this.state_.capabilities;
    }

    options() {
      return this.settings_;
    }

  }
  return new MockImageCapture();
});
