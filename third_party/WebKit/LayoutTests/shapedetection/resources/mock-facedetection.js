"use strict";

let mockFaceDetectionProviderReady = define(
  'mockFaceDetectionProvider',
  ['services/shape_detection/public/interfaces/facedetection.mojom',
   'services/shape_detection/public/interfaces/facedetection_provider.mojom',
   'mojo/public/js/bindings',
   'mojo/public/js/core',
   'content/public/renderer/interfaces',
  ], (faceDetection, faceDetectionProvider, bindings, mojo, interfaces) => {

  class MockFaceDetectionProvider {
    constructor() {
      this.bindingSet_ = new bindings.BindingSet(
          faceDetectionProvider.FaceDetectionProvider);

      interfaces.addInterfaceOverrideForTesting(
          faceDetectionProvider.FaceDetectionProvider.name,
          handle => this.bindingSet_.addBinding(this, handle));
    }

    createFaceDetection(request, options) {
      this.mock_service_ = new MockFaceDetection(request, options);
    }

    getFrameData() {
      return this.mock_service_.buffer_data_;
    }

    getMaxDetectedFaces() {
     return this.mock_service_.maxDetectedFaces_;
    }

    getFastMode () {
      return this.mock_service_.fastMode_;
    }
  }

  class MockFaceDetection {
    constructor(request, options) {
      this.maxDetectedFaces_ = options.max_detected_faces;
      this.fastMode_ = options.fast_mode;
      this.binding_ = new bindings.Binding(faceDetection.FaceDetection, this,
                                           request);
    }

    detect(frame_data, width, height) {
      let receivedStruct = mojo.mapBuffer(frame_data, 0, width*height*4, 0);
      this.buffer_data_ = new Uint32Array(receivedStruct.buffer);
      return Promise.resolve({
        result: {
          bounding_boxes: [
            { x : 1.0, y: 1.0, width: 100.0, height: 100.0 },
            { x : 2.0, y: 2.0, width: 200.0, height: 200.0 },
            { x : 3.0, y: 3.0, width: 300.0, height: 300.0 },
          ]
        }
      });
      mojo.unmapBuffer(receivedStruct.buffer);
    }
  }
  return new MockFaceDetectionProvider();
});
