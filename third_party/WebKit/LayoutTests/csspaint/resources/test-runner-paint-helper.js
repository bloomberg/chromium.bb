// Given a set of 'tests', runs them in the worklet, then comparing the
// expectedError or the expectedMessage.
//
// Usage:
//   runner([{/* test1 */}, { /* test2 */}]);
function runner(tests) {
    if (window.testRunner) {
        testRunner.waitUntilDone();
        testRunner.dumpAsText();
    }

    tests.reduce(function(chain, obj) {
        return chain.then(function() {
            if (obj.expectedError) {
                console.log('The worklet should throw an error with: "' + obj.expectedError + '"');
            } else if (obj.expectedMessage) {
                console.log('The worklet should log a warning with: "' + obj.expectedMessage + '"');
            } else {
                console.log('The worklet should not throw an error.');
            }
            var blob = new Blob([obj.script], {type: 'text/javascript'});
            return paintWorklet.import(URL.createObjectURL(blob));
        });
    }, Promise.resolve()).then(function() {
        if (window.testRunner) {
            testRunner.notifyDone();
        }
    });
}