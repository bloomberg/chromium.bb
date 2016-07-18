// Run a callback after layout and paint of all pending document changes.
//
// It has two modes:
// - traditional mode, for existing tests, and tests needing customized notifyDone timing:
//   Usage:
//     if (window.testRunner)
//       testRunner.waitUntilDone();
//     runAfterLayoutAndPaint(function() {
//       ... // some code which modifies style/layout
//       if (window.testRunner)
//         testRunner.notifyDone();
//       // Or to ensure the next paint is executed before the test finishes:
//       // if (window.testRunner)
//       //   runAfterAfterLayoutAndPaint(function() { testRunner.notifyDone() });
//       // Or notifyDone any time later if needed.
//     });
//
// - autoNotifyDone mode, for new tests which just need to change style/layout and finish:
//   Usage:
//     runAfterLayoutAndPaint(function() {
//       ... // some code which modifies style/layout
//     }, true);

if (window.internals) {
    // TODO(wangxianzhu): Some spv2 tests crash with under-invalidation-checking
    // because the extra display items between Subsequence/EndSubsequence for
    // under-invalidation checking breaks paint chunks. Should fix this when fixing
    // crbug.com/596983.
    if (!internals.runtimeFlags.slimmingPaintV2Enabled)
        internals.runtimeFlags.slimmingPaintUnderInvalidationCheckingEnabled = true;
}

function runAfterLayoutAndPaint(callback, autoNotifyDone) {
    if (!window.testRunner) {
        // For manual test. Delay 500ms to allow us to see the visual change
        // caused by the callback.
        setTimeout(callback, 500);
        return;
    }

    if (autoNotifyDone)
        testRunner.waitUntilDone();

    testRunner.layoutAndPaintAsyncThen(function() {
        callback();
        if (autoNotifyDone)
            testRunner.notifyDone();
    });
}
