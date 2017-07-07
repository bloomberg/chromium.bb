"use strict";

class MockBarcodeDetection {
  constructor() {
    this.bindingSet_ = new mojo.BindingSet(
        shapeDetection.mojom.BarcodeDetection);

    this.interceptor_ = new MojoInterfaceInterceptor(
        shapeDetection.mojom.BarcodeDetection.name);
    this.interceptor_.oninterfacerequest =
        e => this.bindingSet_.addBinding(this, e.handle);
    this.interceptor_.start();
  }

  detect(bitmap_data) {
    let receivedStruct = new Uint8Array(bitmap_data.pixel_data);
    this.buffer_data_ = new Uint32Array(receivedStruct.buffer);
    return Promise.resolve({
      results: [
        {
          rawValue : "cats",
          boundingBox: { x: 1.0, y: 1.0, width: 100.0, height: 100.0 },
          cornerPoints: [
            { x: 1.0, y: 1.0 },
            { x: 101.0, y: 1.0 },
            { x: 101.0, y: 101.0 },
            { x: 1.0, y: 101.0 }
          ],
        },
        {
          rawValue : "dogs",
          boundingBox: { x: 2.0, y: 2.0, width: 50.0, height: 50.0 },
          cornerPoints: [
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

let mockBarcodeDetection = new MockBarcodeDetection();
