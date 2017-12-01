// Check if AudioWorklet is available before running a test.
function assertAudioWorklet() {
  let offlineContext = new OfflineAudioContext(1, 1, 44100);

  // We want to be able to run tests both on the browser and the content shell.
  // So check if AudioWorklet runtime flag is enabled, or check the context
  // has AudioWorklet object.
  if ((Boolean(window.internals) &&
       Boolean(window.internals.runtimeFlags.audioWorkletEnabled)) ||
      offlineContext.audioWorklet instanceof AudioWorklet) {
    return;
  }

  throw new Error('AudioWorklet is not enabled.');
}
