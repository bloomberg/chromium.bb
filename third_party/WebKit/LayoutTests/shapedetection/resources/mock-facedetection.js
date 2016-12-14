"use strict";

let mockFaceDetectionProviderReady = define(
  'mockFaceDetectionProvider',
  ['third_party/WebKit/public/platform/modules/shapedetection/facedetection.mojom',
   'third_party/WebKit/public/platform/modules/shapedetection/facedetection_provider.mojom',
   'mojo/public/js/bindings',
   'mojo/public/js/connection',
   'mojo/public/js/core',
   'content/public/renderer/frame_interfaces',
  ], (faceDetection, faceDetectionProvider, bindings, connection, mojo, interfaces) => {

  class MockFaceDetectionProvider {
    constructor() {
      interfaces.addInterfaceOverrideForTesting(
          faceDetectionProvider.FaceDetectionProvider.name,
          pipe => this.bindToPipe(pipe));
    }

    bindToPipe(pipe) {
      this.stub_ = connection.bindHandleToStub(
          pipe, faceDetectionProvider.FaceDetectionProvider);
      bindings.StubBindings(this.stub_).delegate = this;
    }

    createFaceDetection(request, options) {
      this.mock_service_ = new MockFaceDetection(options);
      this.mock_service_.stub_ = connection.bindHandleToStub(
          request.handle, faceDetection.FaceDetection);
      bindings.StubBindings(this.mock_service_.stub_).delegate =
          this.mock_service_;
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
    constructor(options) {
      this.maxDetectedFaces_ = options.max_detected_faces;
      this.fastMode_ = options.fast_mode;
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
