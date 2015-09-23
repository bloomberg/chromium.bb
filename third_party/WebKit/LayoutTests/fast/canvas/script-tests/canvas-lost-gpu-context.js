description("Test the behavior of canvas recovery after a gpu context loss");

var ctx;
var lostEventHasFired = false;
var contextLostTest;

if (window.internals && window.testRunner) {
    testRunner.dumpAsText();
    var canvas = document.createElement('canvas');
    canvas.addEventListener('contextlost', contextLost);
    canvas.addEventListener('contextrestored', contextRestored);
    ctx = canvas.getContext('2d');
    document.body.appendChild(ctx.canvas);
    verifyContextLost(false);
    window.internals.loseSharedGraphicsContext3D();
    // for the canvas to realize it Graphics context was lost we must try to use the canvas
    ctx.fillRect(0, 0, 1, 1);
    if (!ctx.isContextLost()) {
        debug('<span>Aborting test: Graphics context loss did not destroy canvas contents. This is expected if canvas is not accelerated.</span>');
    } else {
        verifyContextLost(true);
        testRunner.waitUntilDone();
    }
} else {
    testFailed('This test requires window.internals and window.testRunner.');
}


function verifyContextLost(shouldBeLost) {
    // Verify context loss experimentally as well as isContextLost()
    ctx.fillStyle = '#0f0';
    ctx.fillRect(0, 0, 1, 1);
    contextLostTest = ctx.getImageData(0, 0, 1, 1).data[1] == 0;
    if (shouldBeLost) {
        shouldBeTrue('contextLostTest');
        shouldBeTrue('ctx.isContextLost()');
    } else {
        shouldBeFalse('contextLostTest');
        shouldBeFalse('ctx.isContextLost()');
    }
}

function contextLost() {
    if (lostEventHasFired) {
        testFailed('Context lost event was dispatched more than once.');
    } else {
        testPassed('Graphics context lost event dispatched.');
    }
    lostEventHasFired = true;
    verifyContextLost(true);
}

function contextRestored() {
    if (lostEventHasFired) {
        testPassed('Context restored event dispatched after context lost.');
    } else {
        testFailed('Context restored event was dispatched before a context lost event.');
    }
    verifyContextLost(false);
    testRunner.notifyDone();
}
