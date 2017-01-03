'use strict';

function fakeVRDisplays(){
  return [{
  index : 0,
  displayName : "FakeVRDevice",
  capabilities : {
    hasOrientation : true,
    hasPosition : false,
    hasExternalDisplay : false,
    canPresent : false,
  },
  stageParameters : null,
  leftEye : {
    fieldOfView : {
      upDegrees : 45,
      downDegrees : 45,
      leftDegrees : 45,
      rightDegrees : 45,
    },
    offset : [
      -0.03, 0, 0
    ],
    renderWidth : 1024,
    renderHeight : 1024,
  },
  rightEye : {
    fieldOfView : {
      upDegrees : 45,
      downDegrees : 45,
      leftDegrees : 45,
      rightDegrees : 45,
    },
    offset : [
      0.03, 0, 0
    ],
    renderWidth : 1024,
    renderHeigth : 1024,
  }
}];
}
