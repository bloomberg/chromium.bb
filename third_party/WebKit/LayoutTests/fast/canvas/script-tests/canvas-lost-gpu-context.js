description("Test the behavior of canvas recovery after a gpu context loss");

var recoveryLoopPeriod = 5;
var ctx;
var imageData;
var imgdata;

if (window.internals && window.testRunner) {
    testRunner.dumpAsText();
    ctx = document.createElement('canvas').getContext('2d');
    document.body.appendChild(ctx.canvas);
    ctx.fillStyle = '#f00';
    ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
    imageData = ctx.getImageData(0, 0, 1, 1);
    imgdata = imageData.data;
    shouldBe("imgdata[0]", "255");
    shouldBe("imgdata[1]", "0");
    shouldBe("imgdata[2]", "0");
    shouldBe("imgdata[3]", "255");

    window.internals.loseSharedGraphicsContext3D();
    // Verify whether canvas contents are lost with the graphics context.
    imageData = ctx.getImageData(0, 0, 1, 1);
    if (imageData.data[0] == 255) {
        debug('<span>Aborting test: Graphics context loss did not destroy canvas contents. This is expected if canvas is not accelerated.</span>');
    } else {
        // Redrawing immediately will fail because we are working with an
        // unrecovered context here. The context recovery is asynchronous
        // because it requires the context loss notification task to be
        // processed on the renderer main thread, which triggers the
        // re-creation of the SharedGC3D.
        ctx.fillStyle = '#0f0';
        ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
        imageData = ctx.getImageData(0, 0, 1, 1);
        imgdata = imageData.data;
        shouldBe("imgdata[0]", "0");
        shouldBe("imgdata[1]", "0");
        shouldBe("imgdata[2]", "0");
        shouldBe("imgdata[3]", "0");

        testRunner.waitUntilDone();
        setTimeout(recoveryLoop, recoveryLoopPeriod);
    }
} else {
    testFailed('This test requires window.internals and window.testRunner.');
}

// Graphics context recovery happens asynchronously.  To test for recovery, we keep
// retrying to use the canvas until it succeeds, which should hapen long before the test
// times-out.
function recoveryLoop() {
    ctx.fillStyle = '#00f';
    ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
    imageData = ctx.getImageData(0, 0, 1, 1);
    if (imageData.data[2] == 255) {
        testPassed('Graphics context recovered.');
        testRunner.notifyDone();
    } else {
        // Context not yet recovered.  Try again.
        setTimeout(recoveryLoop, recoveryLoopPeriod);
    }
}
