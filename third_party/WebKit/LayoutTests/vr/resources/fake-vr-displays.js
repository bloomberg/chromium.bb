'use strict';

function fakeVRDisplays(){
  let generic_fov = {
    upDegrees : 45,
    downDegrees : 45,
    leftDegrees : 45,
    rightDegrees : 45,
  };

  return {
    FakeMagicWindowOnly: {
      displayName : "FakeVRDisplay",
      capabilities : {
        hasOrientation : true,
        hasPosition : false,
        hasExternalDisplay : true,
        canPresent : false
      },
      stageParameters : null,
      leftEye : {
        fieldOfView: generic_fov,
        offset : [-0.03, 0, 0],
        renderWidth: 1024,
        renderHeight: 1024
      },
      rightEye : {
        fieldOfView : generic_fov,
        offset : [0.03, 0, 0],
        renderWidth: 1024,
        renderHeight: 1024
      }
    },
    Pixel: { // Pixel info as of Dec. 22 2016
      displayName : "Google, Inc. Daydream View",
      capabilities : {
        hasOrientation : true,
        hasPosition : false,
        hasExternalDisplay : false,
        canPresent : true,
      },
      stageParameters : null,
      leftEye : {
        fieldOfView : {
          upDegrees : 48.316,
          downDegrees : 50.099,
          leftDegrees : 35.197,
          rightDegrees : 50.899,
        },
        offset : [-0.032, 0, 0],
        renderWidth : 0,
        renderHeight : 0
      },
      rightEye : {
        fieldOfView : {
          upDegrees : 48.316,
          downDegrees : 50.099,
          leftDegrees: 50.899,
          rightDegrees: 35.197
        },
        offset : [0.032, 0, 0],
        renderWidth : 0,
        renderHeight : 0
      }
    }
    // TODO(bsheedy) add more displays like Rift/Vive
  };
}
