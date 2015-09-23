if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
    testRunner.waitUntilDone();
}

window.addEventListener('message', function() {
    runTest();
    if (window.testRunner)
        testRunner.notifyDone();
});

setFrameLocation = function(url) {
    var frame = document.getElementById('aFrame');
    var jsErrorMessage = 'Failed to set the \'location\' property on \'HTMLFrameElement\': Blocked a frame with origin "http://127.0.0.1:8000" from accessing a cross-origin frame.';
    try {
        setter(frame, url);
    } catch (e) {
        console.log("Caught exception while setting frame's location to '" + url + "'. '" + e + "'.");
        if (e.message == jsErrorMessage)
            console.log("PASS: Exception was '" + e.message + "'.");
        else
            console.log("FAIL: Exception should have been '" + jsErrorMessage + "', was '" + e.message + "'.");
    }
}

function runTest() {
    setFrameLocation('javascript:"FAIL: this should not have been loaded."');
    setFrameLocation(' javascript:"FAIL: this should not have been loaded."');
    setFrameLocation('java\0script:"FAIL: this should not have been loaded."');
    setFrameLocation('javascript\t:"FAIL: this should not have been loaded."');
    setFrameLocation('javascript\1:"FAIL: this should not have been loaded."');
}
