/**
 * Assert AudioWorklet is enabled.
 *
 * The content_shell driven by run-webkit-tests.py is supposed to enable all the
 * experimental web platform features.
 *
 * We also want to run the test on the browser. So we check both cases for
 * the content shell and the browser.
 */
function assertAudioWorklet() {
  if ((Boolean(window.internals) &&
       Boolean(window.internals.runtimeFlags.audioWorkletEnabled)) ||
      (Boolean(window.Worklet) && Boolean(window.audioWorklet))) {
    return;
  }

  throw new Error('AudioWorklet is not enabled.');
}
