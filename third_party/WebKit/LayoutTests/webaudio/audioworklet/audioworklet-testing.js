/* @global internals, Should */

// Check if |internals| and its |runtimeFlags.AudioWorklet| are available.
//
// The content_shell driven by run-webkit-tests.py is supposed to enable
// all the experimental web platform features. The flags are exposed via
// |internals.runtimeFlag|.
//
// See: https://www.chromium.org/blink/runtime-enabled-features
function checkInternalsAndAudioWorkletRuntimeFlag(taskDone) {

  var isInternals = Should('window.internals', window.internals).exist();

  if (!isInternals) {
    taskDone();
    return false;
  }

  var isFlag = Should('window.internals.runtimeFlags.audioWorkletEnabled',
    window.internals.runtimeFlags.audioWorkletEnabled).beEqualTo(true);

  if (!isFlag) {
    taskDone();
  }

  return isFlag;
}
