function generateTest(pixelFormat, pixelType, prologue) {
    var wtu = WebGLTestUtils;
    var gl = null;
    var textureLoc = null;
    var successfullyParsed = false;
    var imageDataBefore = null;

    var init = function()
    {
        if (window.initNonKhronosFramework) {
            window.initNonKhronosFramework(true);
        }

        description('Verify texImage2D and texSubImage2D code paths taking canvas elements (' + pixelFormat + '/' + pixelType + ')');

        gl = wtu.create3DContext("example");

        if (!prologue(gl)) {
            finishTest();
            return;
        }

        var program = wtu.setupTexturedQuad(gl);

        gl.clearColor(0,0,0,1);
        gl.clearDepth(1);

        var testCanvas = document.createElement('canvas');
        testCanvas.width = 1;
        testCanvas.height = 2;
        var ctx = testCanvas.getContext("2d");
        ctx.fillStyle = "#ff0000";
        ctx.fillRect(0,0,1,1);
        ctx.fillStyle = "#00ff00";
        ctx.fillRect(0,1,1,1);
        imageDataBefore = ctx.getImageData(0, 0, testCanvas.width, testCanvas.height);
        runTest(testCanvas);
    }

    function runOneIteration(image, useTexSubImage2D, flipY, topColor, bottomColor)
    {
        debug('Testing ' + (useTexSubImage2D ? 'texSubImage2D' : 'texImage2D') +
              ' with flipY=' + flipY);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        // Disable any writes to the alpha channel
        gl.colorMask(1, 1, 1, 0);
        var texture = gl.createTexture();
        // Bind the texture to texture unit 0
        gl.bindTexture(gl.TEXTURE_2D, texture);
        // Set up texture parameters
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
        // Set up pixel store parameters
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, flipY);
        gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
        gl.pixelStorei(gl.UNPACK_COLORSPACE_CONVERSION_WEBGL, gl.NONE);
        // Upload the image into the texture
        if (useTexSubImage2D) {
            // Initialize the texture to black first
            gl.texImage2D(gl.TEXTURE_2D, 0, gl[pixelFormat], image.width, image.height, 0,
                          gl[pixelFormat], gl[pixelType], null);
            gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, gl[pixelFormat], gl[pixelType], image);
        } else {
            gl.texImage2D(gl.TEXTURE_2D, 0, gl[pixelFormat], gl[pixelFormat], gl[pixelType], image);
        }

        // Point the uniform sampler to texture unit 0
        gl.uniform1i(textureLoc, 0);
        // Draw the triangles
        wtu.drawQuad(gl, [0, 0, 0, 255]);
        // Check a few pixels near the top and bottom and make sure they have
        // the right color.
        debug("Checking lower left corner");
        wtu.checkCanvasRect(gl, 4, 4, 2, 2, bottomColor,
                            "shouldBe " + bottomColor);
        debug("Checking upper left corner");
        wtu.checkCanvasRect(gl, 4, gl.canvas.height - 8, 2, 2, topColor,
                            "shouldBe " + topColor);

        debug("Checking if pixel values in source canvas change after canvas used as webgl texture");
        checkSourceCanvasImageData(image.getContext("2d").getImageData(0, 0, image.width, image.height));
    }

    function checkSourceCanvasImageData(imageDataAfter)
    {
        if (imageDataBefore.length != imageDataAfter.length)
        {
            testFailed("The size of image data in source canvas become different after it is used in webgl texture.");
            return;
        }
        for (var i = 0; i < imageDataAfter.length; i++)
        {
            if (imageDataBefore[i] != imageDataAfter[i])
            {
                testFailed("Pixel values in source canvas have changed after canvas used in webgl texture.");
                return;
            }
        }
        testPassed("Pixel values in source canvas remain unchanged after canvas used in webgl texture.");
    }

    function runTest(image)
    {
        var red = [255, 0, 0];
        var green = [0, 255, 0];
        runOneIteration(image, false, true, red, green);
        runOneIteration(image, false, false, green, red);
        runOneIteration(image, true, true, red, green);
        runOneIteration(image, true, false, green, red);

        glErrorShouldBe(gl, gl.NO_ERROR, "should be no errors");
        finishTest();
    }

    return init;
}
