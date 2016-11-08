// Runs a series of tests related to console output on a worklet.
//
// Usage:
// runConsoleTests(workletType);
function runConsoleTests(worklet, opt_path) {
    const path = opt_path || '';

    promise_test(function() {
        return worklet.import(path + 'resources/console-worklet-script.js').then(function(undefined_arg) {
            // Results should be verified by a test expectation file.
        }).catch(function(error) {
            assert_unreached('unexpected rejection: ' + error);
        });

    }, 'Console output on a Worklet.');
}
