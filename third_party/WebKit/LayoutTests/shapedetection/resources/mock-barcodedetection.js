"use strict";

let mockBarcodeDetectionReady = define(
  'mockBarcodeDetection',
  ['services/shape_detection/public/interfaces/barcodedetection.mojom',
   'mojo/public/js/bindings',
   'mojo/public/js/core',
   'content/public/renderer/frame_interfaces',
   'content/public/renderer/interfaces',
  ], (barcodeDetection, bindings, mojo, frameInterfaces, processInterfaces) => {

  class MockBarcodeDetection {
    constructor() {
      this.bindingSet_ = new bindings.BindingSet(
          barcodeDetection.BarcodeDetection);

      frameInterfaces.addInterfaceOverrideForTesting(
          barcodeDetection.BarcodeDetection.name,
          handle => this.bindingSet_.addBinding(this, handle));
      processInterfaces.addInterfaceOverrideForTesting(
          barcodeDetection.BarcodeDetection.name,
          handle => this.bindingSet_.addBinding(this, handle));
    }

    detect(bitmap_data) {
      let receivedStruct = new Uint8Array(bitmap_data.pixel_data);
      this.buffer_data_ = new Uint32Array(receivedStruct.buffer);
      return Promise.resolve({
        results: [
          {
            raw_value : "cats",
            bounding_box: { x: 1.0, y: 1.0, width: 100.0, height: 100.0 },
            corner_points: [
              { x: 1.0, y: 1.0 },
              { x: 101.0, y: 1.0 },
              { x: 101.0, y: 101.0 },
              { x: 1.0, y: 101.0 }
            ],
          },
          {
            raw_value : "dogs",
            bounding_box: { x: 2.0, y: 2.0, width: 50.0, height: 50.0 },
            corner_points: [
              { x: 2.0, y: 2.0 },
              { x: 52.0, y: 2.0 },
              { x: 52.0, y: 52.0 },
              { x: 2.0, y: 52.0 }
            ],
          },
        ],
      });
    }

    getFrameData() {
      return this.buffer_data_;
    }
  }
  return new MockBarcodeDetection();
});
