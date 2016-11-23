"use strict";

let mockBarcodeDetectionReady = define(
  'mockBarcodeDetection',
  ['third_party/WebKit/public/platform/modules/shapedetection/barcodedetection.mojom',
   'mojo/public/js/bindings',
   'mojo/public/js/connection',
   'mojo/public/js/core',
   'content/public/renderer/frame_interfaces',
  ], (barcodeDetection, bindings, connection, mojo, interfaces) => {

  class MockBarcodeDetection {
    constructor() {
      interfaces.addInterfaceOverrideForTesting(
          barcodeDetection.BarcodeDetection.name,
          pipe => this.bindToPipe(pipe));
    }

    bindToPipe(pipe) {
      this.stub_ = connection.bindHandleToStub(pipe,
                                               barcodeDetection.BarcodeDetection);
      bindings.StubBindings(this.stub_).delegate = this;
    }

    detect(frame_data, width, height) {
      let receivedStruct = mojo.mapBuffer(frame_data, 0, width*height*4, 0);
      this.buffer_data_ = new Uint32Array(receivedStruct.buffer);
      return Promise.resolve({
        results: [
          {
            raw_value : "cats",
            bounding_box: { x : 1.0, y: 1.0, width: 100.0, height: 100.0 },
          },
          {
            raw_value : "dogs",
            bounding_box: { x : 2.0, y: 2.0, width: 50.0, height: 50.0 },
          },
        ],
      });
      mojo.unmapBuffer(receivedStruct.buffer);
    }

    getFrameData() {
      return this.buffer_data_;
    }
  }
  return new MockBarcodeDetection();
});
