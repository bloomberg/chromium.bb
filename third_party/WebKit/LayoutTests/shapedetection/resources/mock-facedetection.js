"use strict";

let mockFaceDetectionProviderReady = define(
  'mockFaceDetectionProvider',
  ['services/shape_detection/public/interfaces/facedetection.mojom',
   'services/shape_detection/public/interfaces/facedetection_provider.mojom',
   'mojo/public/js/bindings',
   'mojo/public/js/core',
   'content/public/renderer/frame_interfaces',
   'content/public/renderer/interfaces',
  ], (faceDetection, faceDetectionProvider, bindings, mojo, frameInterfaces,
      processInterfaces) => {

  class MockFaceDetectionProvider {
    constructor() {
      this.bindingSet_ = new bindings.BindingSet(
          faceDetectionProvider.FaceDetectionProvider);

      frameInterfaces.addInterfaceOverrideForTesting(
          faceDetectionProvider.FaceDetectionProvider.name,
          handle => this.bindingSet_.addBinding(this, handle));
      processInterfaces.addInterfaceOverrideForTesting(
          faceDetectionProvider.FaceDetectionProvider.name,
          handle => this.bindingSet_.addBinding(this, handle));
    }

    createFaceDetection(request, options) {
      this.mockService_ = new MockFaceDetection(request, options);
    }

    getFrameData() {
      return this.mockService_.bufferData_;
    }

    getMaxDetectedFaces() {
     return this.mockService_.maxDetectedFaces_;
    }

    getFastMode () {
      return this.mockService_.fastMode_;
    }
  }

  class MockFaceDetection {
    constructor(request, options) {
      this.maxDetectedFaces_ = options.max_detected_faces;
      this.fastMode_ = options.fast_mode;
      this.binding_ = new bindings.Binding(faceDetection.FaceDetection, this,
                                           request);
    }

    detect(bitmap_data) {
      let receivedStruct = new Uint8Array(bitmap_data.pixel_data);
      this.bufferData_ = new Uint32Array(receivedStruct.buffer);
      return Promise.resolve({
        results: [
          {
            bounding_box: {x: 1.0, y: 1.0, width: 100.0, height: 100.0},
            landmarks: [{
              type: faceDetection.LandmarkType.EYE,
              location: {x: 4.0, y: 5.0}
            }]
          },
          {
            bounding_box: {x: 2.0, y: 2.0, width: 200.0, height: 200.0},
            landmarks: []
          },
          {
            bounding_box: {x: 3.0, y: 3.0, width: 300.0, height: 300.0},
            landmarks: []
          },
        ]
      });
    }
  }
  return new MockFaceDetectionProvider();
});
