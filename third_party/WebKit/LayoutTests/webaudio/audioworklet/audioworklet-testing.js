/**
 * Check if |window.internals| and |window.internals.runtimeFlags.AudioWorklet|
 * are available.
 *
 * The content_shell driven by run-webkit-tests.py is supposed to enable all the
 * experimental web platform features. The flags are exposed via
 * |internals.runtimeFlag|.
 *
 * See: https://www.chromium.org/blink/runtime-enabled-features
 *
 * @return {Boolean}
 */
function isAudioWorkletEnabled() {
  return {
    onContentShell: Boolean(window.internals) &&
                    Boolean(window.internals.runtimeFlags.audioWorkletEnabled),
    onBrowser: Boolean(window.Worklet) && Boolean(window.audioWorklet)
  };
}
