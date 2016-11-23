"use strict";

let mockFaceDetectionReady = define(
  'mockFaceDetection',
  ['third_party/WebKit/public/platform/modules/shapedetection/facedetection.mojom',
   'mojo/public/js/bindings',
   'mojo/public/js/connection',
   'mojo/public/js/core',
   'content/public/renderer/frame_interfaces',
  ], (faceDetection, bindings, connection, mojo, interfaces) => {

  class MockFaceDetection {
    constructor() {
      interfaces.addInterfaceOverrideForTesting(
          faceDetection.FaceDetection.name,
          pipe => this.bindToPipe(pipe));
    }

    bindToPipe(pipe) {
      this.stub_ = connection.bindHandleToStub(pipe,
                                               faceDetection.FaceDetection);
      bindings.StubBindings(this.stub_).delegate = this;
    }

    detect(frame_data, width, height, options) {
      let receivedStruct = mojo.mapBuffer(frame_data, 0, width*height*4, 0);
      this.buffer_data_ = new Uint32Array(receivedStruct.buffer);
      this.maxDetectedFaces_ = options.max_detected_faces;
      this.fastMode_ = options.fast_mode;
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

    getFrameData() {
      return this.buffer_data_;
    }

    getMaxDetectedFaces() {
     return this.maxDetectedFaces_;
    }

    getFastMode () {
      return this.fastMode_;
    }
  }
  return new MockFaceDetection();
});
