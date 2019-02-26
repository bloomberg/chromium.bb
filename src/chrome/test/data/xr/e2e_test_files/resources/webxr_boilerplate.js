// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Add additional setup steps to the object from webvr_e2e.js if it exists.
if (typeof initializationSteps !== 'undefined') {
  initializationSteps['getXRDevice'] = false;
  initializationSteps['magicWindowStarted'] = false;
} else {
  // Create here if it doesn't exist so we can access it later without checking
  // if it's defined.
  var initializationSteps = {};
}

var webglCanvas = document.getElementById('webgl-canvas');
var glAttribs = {
  alpha: false,
};
var gl = null;
var xrDevice = null;
var onMagicWindowXRFrameCallback = null;
var onImmersiveXRFrameCallback = null;
var shouldSubmitFrame = true;
var hasPresentedFrame = false;
var arSessionRequestWouldTriggerPermissionPrompt = null;

var sessionTypes = Object.freeze({
  IMMERSIVE: 1,
  AR: 2,
  MAGIC_WINDOW: 3
});
var sessionTypeToRequest = sessionTypes.IMMERSIVE;

class SessionInfo {
  constructor() {
    this.session = null;
    this.frameOfRef = null;
  }

  get currentSession() {
    return this.session;
  }

  get currentFrameOfRef() {
    return this.frameOfRef;
  }

  set currentSession(session) {
    this.session = session;
  }

  set currentFrameOfRef(frameOfRef) {
    this.frameOfRef = frameOfRef;
  }

  clearSession() {
    this.session = null;
    this.frameOfRef = null;
  }
}

var sessionInfos = {}
sessionInfos[sessionTypes.IMMERSIVE] = new SessionInfo();
sessionInfos[sessionTypes.AR] = new SessionInfo();
sessionInfos[sessionTypes.MAGIC_WINDOW] = new SessionInfo();

function getSessionType(session) {
  if (session.immersive) {
    return sessionTypes.IMMERSIVE;
  } else if (sessionInfos[sessionTypes.AR].currentSession == session) {
    // TODO(bsheedy): Replace this check if there's ever something like
    // session.ar for checking if the session is AR-capable.
    return sessionTypes.AR;
  } else {
    return sessionTypes.MAGIC_WINDOW;
  }
}

function onRequestSession() {
  switch (sessionTypeToRequest) {
    case sessionTypes.IMMERSIVE:
      xrDevice.requestSession({immersive: true}).then( (session) => {
        sessionInfos[sessionTypes.IMMERSIVE].currentSession = session;
        onSessionStarted(session);
      });
      break;
    case sessionTypes.AR:
      let sessionOptions = {
        environmentIntegration: true,
        outputContext: webglCanvas.getContext('xrpresent'),
      };
      xrDevice.requestSession(sessionOptions).then((session) => {
        sessionInfos[sessionTypes.AR].currentSession = session;
        onSessionStarted(session);
      });
      break;
    default:
      throw 'Given unsupported WebXR session type enum ' + sessionTypeToRequest;
  }
}

function onSessionStarted(session) {
  session.addEventListener('end', onSessionEnded);
  // Initialize the WebGL context for use with XR if it hasn't been already
  if (!gl) {
    glAttribs['compatibleXRDevice'] = session.device;

    // Create an offscreen canvas and get its context
    let offscreenCanvas = document.createElement('canvas');
    gl = offscreenCanvas.getContext('webgl', glAttribs);
    if (!gl) {
      throw 'Failed to get WebGL context';
    }
    gl.clearColor(0.0, 1.0, 0.0, 1.0);
    gl.enable(gl.DEPTH_TEST);
    gl.enable(gl.CULL_FACE);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  }

  session.baseLayer = new XRWebGLLayer(session, gl);
  session.requestFrameOfReference('eye-level').then( (frameOfRef) => {
    sessionInfos[getSessionType(session)].currentFrameOfRef = frameOfRef;
    session.requestAnimationFrame(onXRFrame);
  });
}

function onSessionEnded(event) {
  sessionInfos[getSessionType(event.session)].clearSession();
}

function onXRFrame(t, frame) {
  let session = frame.session;
  session.requestAnimationFrame(onXRFrame);

  let frameOfRef = null;
  frameOfRef = sessionInfos[getSessionType(session)].currentFrameOfRef;
  let pose = frame.getDevicePose(frameOfRef);

  // Exiting the rAF callback without dirtying the GL context is interpreted
  // as not submitting a frame
  if (!shouldSubmitFrame) {
    return;
  }

  gl.bindFramebuffer(gl.FRAMEBUFFER, session.baseLayer.framebuffer);

  // If in an immersive session, set canvas to blue.
  // If in an AR session, just draw the camera.
  // Otherwise, red.
  switch (getSessionType(session)) {
    case sessionTypes.IMMERSIVE:
      gl.clearColor(0.0, 0.0, 1.0, 1.0);
      if (onImmersiveXRFrameCallback) {
        onImmersiveXRFrameCallback(session, frame, gl);
      }
      gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
      break;
    case sessionTypes.AR:
      // Do nothing for now
      break;
    default:
      if (onMagicWindowXRFrameCallback) {
        onMagicWindowXRFrameCallback(session, frame);
      }
      gl.clearColor(1.0, 0.0, 0.0, 1.0);
      gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  }

  hasPresentedFrame = true;
}

// Try to get an XRDevice and set up a non-immersive session with it
if (navigator.xr) {
  navigator.xr.requestDevice().then( (device) => {
    xrDevice = device;
    if (!device)
      return;

    // Set up the device to have a non-immersive session (magic window) drawing
    // into the full screen canvas on the page
    let ctx = webglCanvas.getContext('xrpresent');
    // WebXR for VR tests want a non-immersive session to be automatically
    // created on page load to reduce the amount of boilerplate code necessary.
    // However, doing so during AR tests currently fails due to AR sessions
    // always requiring a user gesture. So, allow a page to set a variable
    // before loading this JavaScript file if they wish to skip the automatic
    // non-immersive session creation.
    if (typeof shouldAutoCreateNonImmersiveSession === 'undefined'
        || shouldAutoCreateNonImmersiveSession === true) {
      device.requestSession({outputContext: ctx}).then( (session) => {
        onSessionStarted(session);
      }).then( () => {
        initializationSteps['magicWindowStarted'] = true;
      });
    } else {
      initializationSteps['magicWindowStarted'] = true;
    }
  }).then( () => {
    initializationSteps['getXRDevice'] = true;
  });
} else {
  initializationSteps['getXRDevice'] = true;
  initializationSteps['magicWindowStarted'] = true;
}

webglCanvas.onclick = onRequestSession;
