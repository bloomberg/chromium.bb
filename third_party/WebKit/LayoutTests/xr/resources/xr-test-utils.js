// func: A function that takes a session and optionally a test object and
// performs tests. If func returns a promise, test will only pass if the promise
// resolves.
function xr_session_promise_test(
    func, device, sessionOptions, name, properties) {
  if (document.getElementById('webgl-canvas') ||
      document.getElementById('webgl2-canvas')) {
    webglCanvasSetup();
  }

  addFakeDevice(device);

  promise_test((t) => {
    return navigator.xr.requestDevice().then(
        (device) => new Promise((resolve, reject) => {
          // Perform the session request in a user gesture.
          runWithUserGesture(() => {
            resolve(device.requestSession(sessionOptions)
                        .then((session) => func(session, t)));
          });
        }));
  }, name, properties);
}

let webglCanvas, gl;
function webglCanvasSetup() {
  let webgl2 = false;
  webglCanvas = document.getElementById('webgl-canvas');
  if (!webglCanvas) {
    webglCanvas = document.getElementById('webgl2-canvas');
    webgl2 = true;
  }
  let glAttributes = {
    alpha: false,
    antialias: false,
  };
  gl = webglCanvas.getContext(webgl2 ? 'webgl2' : 'webgl', glAttributes);
}

// TODO(offenwanger): eventSender cannot be used with WPTs. Find another way to
// fake use gestures.
// https://chromium.googlesource.com/chromium/src/+/lkgr/docs/testing/web_platform_tests.md#tests-that-require-testing-apis
function runWithUserGesture(fn) {
  function thunk() {
    document.removeEventListener('keypress', thunk, false);
    fn()
  }
  document.addEventListener('keypress', thunk, false);
  eventSender.keyDown(' ', []);
}