// Test runner for the paint worklet.
//
// Calls a given function with a newly created element, and prints the expected
// geometry to the console.
//
// Runs each test sequentially after a layout and a paint.
//
// Usage:
// testRunnerGeometryLogging([{
//      func: function(el) {
//          el.style.width = '100px';
//          el.style.height = '100px';
//      },
//      expected: {width: 100, height: 100},
// ]);

function testRunnerGeometryLogging(tests, workletCode) {
    if (window.testRunner) {
        testRunner.waitUntilDone();
        testRunner.dumpAsText();
    }

    paintWorklet.import('resources/paint-logging-green.js').then(function() {
        tests.reduce(function(chain, obj) {
            return chain.then(function() {
                console.log('The worklet should log: \'width: ' + obj.expected.width + ', height: ' + obj.expected.height + '\'');
                var el = document.createElement('div');
                document.body.appendChild(el);
                obj.func(el);
                return new Promise(function(resolve) {
                    runAfterLayoutAndPaint(function() {
                        document.body.removeChild(el);
                        resolve();
                    });
                });
            });
        }, Promise.resolve()).then(function() {
            if (window.testRunner) {
                testRunner.notifyDone();
            }
        });
    });
}
