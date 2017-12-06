//------------- Reference Buffers for Pixelated Filter Quality -------------//
var refBufPixelatedPremul = [
    255,0,0,255,    255,0,0,255,    26,0,0,26,      26,0,0,26,
    255,0,0,255,    255,0,0,255,    26,0,0,26,      26,0,0,26,
    0,255,0,255,    0,255,0,255,    0,26,0,26,      0,26,0,26,
    0,255,0,255,    0,255,0,255,    0,26,0,26,      0,26,0,26];

var refBufPixelatedUnpremul = [
    255,0,0,255,    255,0,0,255,    255,0,0,26,     255,0,0,26,
    255,0,0,255,    255,0,0,255,    255,0,0,26,     255,0,0,26,
    0,255,0,255,    0,255,0,255,    0,255,0,26,     0,255,0,26,
    0,255,0,255,    0,255,0,255,    0,255,0,26,     0,255,0,26];

var refBufPixelatedPremulFlipY = [
    0,255,0,255,    0,255,0,255,    0,26,0,26,      0,26,0,26,
    0,255,0,255,    0,255,0,255,    0,26,0,26,      0,26,0,26,
    255,0,0,255,    255,0,0,255,    26,0,0,26,      26,0,0,26,
    255,0,0,255,    255,0,0,255,    26,0,0,26,      26,0,0,26];

var refBufPixelatedUnpremulFlipY = [
    0,255,0,255,    0,255,0,255,    0,255,0,26,     0,255,0,26,
    0,255,0,255,    0,255,0,255,    0,255,0,26,     0,255,0,26,
    255,0,0,255,    255,0,0,255,    255,0,0,26,     255,0,0,26,
    255,0,0,255,    255,0,0,255,    255,0,0,26,     255,0,0,26];

//---------------- Reference Buffers for Low Filter Quality ----------------//
var refBufLowPremul= [
    255,0,0,255,    197,0,0,197,    83,0,0,83,      26,0,0,26,
    191,63,0,255,   148,49,0,197,   62,20,0,83,     19,6,0,26,
    63,191,0,255,   49,148,0,197,   20,62,0,83,     6,19,0,26,
    0,255,0,255,    0,197,0,197,    0,83,0,83,      0,26,0,26];

var refBufLowUnpremul = [
    255,0,0,255,    255,0,0,197,    255,0,0,83,     255,0,0,26,
    191,63,0,255,   192,63,0,197,   190,61,0,83,    186,59,0,26,
    63,191,0,255,   63,192,0,197,   61,190,0,83,    59,186,0,26,
    0,255,0,255,    0,255,0,197,    0,255,0,83,     0,255,0,26];

var refBufLowPremulFlipY = [
    0,255,0,255,    0,197,0,197,    0,83,0,83,      0,26,0,26,
    63,191,0,255,   49,148,0,197,   20,62,0,83,     6,19,0,26,
    191,63,0,255,   148,49,0,197,   62,20,0,83,     19,6,0,26,
    255,0,0,255,    197,0,0,197,    83,0,0,83,      26,0,0,26];

var refBufLowUnpremulFlipY = [
    0,255,0,255,    0,255,0,197,    0,255,0,83,     0,255,0,26,
    63,191,0,255,   63,192,0,197,   61,190,0,83,    59,186,0,26,
    191,63,0,255,   192,63,0,197,   190,61,0,83,    186,59,0,26,
    255,0,0,255,    255,0,0,197,    255,0,0,83,     255,0,0,26];

//-------------- Reference Buffers for Medium Filter Quality ---------------//
var refBufMediumPremul = refBufLowPremul;
var refBufMediumUnpremul = refBufLowUnpremul;
var refBufMediumPremulFlipY = refBufLowPremulFlipY;
var refBufMediumUnpremulFlipY = refBufLowUnpremulFlipY;

//-------- Reference Buffers for High Filter Quality (Premul Source) -------//
var refBufHighPremulSourcePremul = [
    255,0,0,255,    200,0,0,200,    81,0,0,81,      21,0,0,21,
    198,63,0,255,   152,48,0,200,   62,20,0,81,     16,5,0,21,
    63,198,0,255,   48,152,0,200,   20,62,0,81,     5,16,0,21,
    0,255,0,255,    0,200,0,200,    0,81,0,81,      0,21,0,21];

var refBufHighUnpremulSourcePremul = [
    255,0,0,255,    255,0,0,200,    255,0,0,81,     255,0,0,21,
    198,63,0,255,   194,61,0,200,   195,63,0,81,    194,61,0,21,
    63,198,0,255,   61,194,0,200,   63,195,0,81,    61,194,0,21,
    0,255,0,255,    0,255,0,200,    0,255,0,81,     0,255,0,21];

var refBufHighPremulFlipYSourcePremul = [
    0,255,0,255,    0,200,0,200,    0,81,0,81,      0,21,0,21,
    63,198,0,255,   48,152,0,200,   20,62,0,81,     5,16,0,21,
    198,63,0,255,   152,48,0,200,   62,20,0,81,     16,5,0,21,
    255,0,0,255,    200,0,0,200,    81,0,0,81,      21,0,0,21];

var refBufHighUnpremulFlipYSourcePremul = [
    0,255,0,255,    0,255,0,200,    0,255,0,81,     0,255,0,21,
    63,198,0,255,   61,194,0,200,   63,195,0,81,    61,194,0,21,
    198,63,0,255,   194,61,0,200,   195,63,0,81,    194,61,0,21,
    255,0,0,255,    255,0,0,200,    255,0,0,81,     255,0,0,21];

//------- Reference Buffers for High Filter Quality (Unpremul Source) ------//
var refBufHighPremulSourceUnpremul = [
    255,0,0,255,    200,0,0,200,    81,0,0,81,      21,0,0,21,
    198,63,0,255,   152,48,0,200,   62,20,0,81,     16,5,0,21,
    63,198,0,255,   48,152,0,200,   20,62,0,81,     5,16,0,21,
    0,255,0,255,    0,200,0,200,    0,81,0,81,      0,21,0,21];

var refBufHighUnpremulSourceUnpremul = [
    255,0,0,255,    255,0,0,200,    255,0,0,81,     255,0,0,21,
    193,62,0,255,   193,62,0,200,   193,62,0,81,    193,62,0,21,
    62,193,0,255,   62,193,0,200,   62,193,0,81,    62,193,0,21,
    0,255,0,255,    0,255,0,200,    0,255,0,81,     0,255,0,21];

var refBufHighPremulFlipYSourceUnpremul = [
    0,255,0,255,    0,200,0,200,    0,81,0,81,      0,21,0,21,
    63,198,0,255,   48,152,0,200,   20,62,0,81,     5,16,0,21,
    198,63,0,255,   152,48,0,200,   62,20,0,81,     16,5,0,21,
    255,0,0,255,    200,0,0,200,    81,0,0,81,      21,0,0,21];

var refBufHighUnpremulFlipYSourceUnpremul = [
    0,255,0,255,    0,255,0,200,    0,255,0,81,     0,255,0,21,
    62,193,0,255,   62,193,0,200,   62,193,0,81,    62,193,0,21,
    193,62,0,255,   193,62,0,200,   193,62,0,81,    193,62,0,21,
    255,0,0,255,    255,0,0,200,    255,0,0,81,     255,0,0,21];


var wtu = WebGLTestUtils;
var tiu = TexImageUtils;
var gl = null;
var internalFormat = "RGBA";
var pixelFormat = "RGBA";
var pixelType = "UNSIGNED_BYTE";
var opaqueTolerance = 0;
var transparentTolerance = 0;
var transparentToleranceForBlob = 5;

function getRefBuffer(testOptions, flipY, premultiplyAlpha) {
    var refBufName = "refBuf" +
                     testOptions.resizeQuality.charAt(0).toUpperCase() +
                     testOptions.resizeQuality.slice(1);
    if (premultiplyAlpha)
        refBufName += "Premul";
    else
        refBufName += "Unpremul";
    if (flipY)
        refBufName += "FlipY";
    if (testOptions.resizeQuality == "high") {
        if (testOptions.sourceIsPremul)
            refBufName += "SourcePremul";
        else
            refBufName += "SourceUnpremul";
    }
    return eval(refBufName);
}

function checkPixels(buf, refBuf, tolerance, retVal)
{
    if (buf.length != refBuf.length) {
        retVal.testPassed = false;
        return;
    }
    for (var p = 0; p < buf.length; p++) {
        if (Math.abs(buf[p] - refBuf[p]) > tolerance) {
            retVal.testPassed = false;
            return;
        }
    }
}

function runOneIteration(useTexSubImage2D, bindingTarget, program, bitmap,
                         flipY, premultiplyAlpha, retVal, colorSpace,
                         testOptions)
{
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    // Enable writes to the RGBA channels
    gl.colorMask(1, 1, 1, 1);
    var texture = gl.createTexture();
    // Bind the texture to texture unit 0
    gl.bindTexture(bindingTarget, texture);
    // Set up texture parameters
    gl.texParameteri(bindingTarget, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(bindingTarget, gl.TEXTURE_MAG_FILTER, gl.NEAREST);

    var targets = [gl.TEXTURE_2D];
    if (bindingTarget == gl.TEXTURE_CUBE_MAP) {
        targets = [gl.TEXTURE_CUBE_MAP_POSITIVE_X,
                   gl.TEXTURE_CUBE_MAP_NEGATIVE_X,
                   gl.TEXTURE_CUBE_MAP_POSITIVE_Y,
                   gl.TEXTURE_CUBE_MAP_NEGATIVE_Y,
                   gl.TEXTURE_CUBE_MAP_POSITIVE_Z,
                   gl.TEXTURE_CUBE_MAP_NEGATIVE_Z];
    }
    // Upload the image into the texture
    for (var tt = 0; tt < targets.length; ++tt) {
        if (useTexSubImage2D) {
            // Initialize the texture to black first
            gl.texImage2D(targets[tt], 0, gl[internalFormat], bitmap.width,
                bitmap.height, 0, gl[pixelFormat], gl[pixelType], null);
            gl.texSubImage2D(targets[tt], 0, 0, 0, gl[pixelFormat],
                gl[pixelType], bitmap);
        } else {
            gl.texImage2D(targets[tt], 0, gl[internalFormat], gl[pixelFormat],
                gl[pixelType], bitmap);
        }
    }

    var width = gl.canvas.width;
    var height = gl.canvas.height;

    var loc;
    if (bindingTarget == gl.TEXTURE_CUBE_MAP) {
        loc = gl.getUniformLocation(program, "face");
    }

    var tolerance = opaqueTolerance;
    if (retVal.alpha != 0) {
        tolerance = transparentTolerance;
        if (testOptions.sourceName == "Blob")
            tolerance = transparentToleranceForBlob;
    }
    for (var tt = 0; tt < targets.length; ++tt) {
        if (bindingTarget == gl.TEXTURE_CUBE_MAP) {
            gl.uniform1i(loc, targets[tt]);
        }
        // Draw the triangles
        gl.clearColor(0, 0, 0, 1);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        gl.drawArrays(gl.TRIANGLES, 0, 6);
        // Check the buf
        var refBuf = getRefBuffer(testOptions, flipY, premultiplyAlpha);
        var buf = new Uint8Array(width * height * 4);
        gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, buf);
        checkPixels(buf, refBuf, tolerance, retVal);
    }
}

function runTestOnBindingTarget(bindingTarget, program, bitmaps, retVal,
                                testOptions) {
    var cases = [
        { sub: false, bitmap: bitmaps.defaultOption, flipY: false,
            premultiply: true, colorSpace: 'empty' },
        { sub: true, bitmap: bitmaps.defaultOption, flipY: false,
            premultiply: true, colorSpace: 'empty' },
        { sub: false, bitmap: bitmaps.noFlipYPremul, flipY: false,
            premultiply: true, colorSpace: 'empty' },
        { sub: true, bitmap: bitmaps.noFlipYPremul, flipY: false,
            premultiply: true, colorSpace: 'empty' },
        { sub: false, bitmap: bitmaps.noFlipYDefault, flipY: false,
            premultiply: true, colorSpace: 'empty' },
        { sub: true, bitmap: bitmaps.noFlipYDefault, flipY: false,
            premultiply: true, colorSpace: 'empty' },
        { sub: false, bitmap: bitmaps.noFlipYUnpremul, flipY: false,
            premultiply: false, colorSpace: 'empty' },
        { sub: true, bitmap: bitmaps.noFlipYUnpremul, flipY: false,
            premultiply: false, colorSpace: 'empty' },
        { sub: false, bitmap: bitmaps.flipYPremul, flipY: true,
            premultiply: true, colorSpace: 'empty' },
        { sub: true, bitmap: bitmaps.flipYPremul, flipY: true,
            premultiply: true, colorSpace: 'empty' },
        { sub: false, bitmap: bitmaps.flipYDefault, flipY: true,
            premultiply: true, colorSpace: 'empty' },
        { sub: true, bitmap: bitmaps.flipYDefault, flipY: true,
            premultiply: true, colorSpace: 'empty' },
        { sub: false, bitmap: bitmaps.flipYUnpremul, flipY: true,
            premultiply: false, colorSpace: 'empty' },
        { sub: true, bitmap: bitmaps.flipYUnpremul, flipY: true,
            premultiply: false, colorSpace: 'empty' },
        { sub: false, bitmap: bitmaps.colorSpaceDef, flipY: false,
            premultiply: true,
            colorSpace: retVal.colorSpaceEffect ? 'notprovided' : 'empty' },
        { sub: true, bitmap: bitmaps.colorSpaceDef, flipY: false,
            premultiply: true,
            colorSpace: retVal.colorSpaceEffect ? 'notprovided' : 'empty' },
        { sub: false, bitmap: bitmaps.colorSpaceNone, flipY: false,
            premultiply: true,
            colorSpace: retVal.colorSpaceEffect ? 'none' : 'empty' },
        { sub: true, bitmap: bitmaps.colorSpaceNone, flipY: false,
            premultiply: true,
            colorSpace: retVal.colorSpaceEffect ? 'none' : 'empty' },
        { sub: false, bitmap: bitmaps.colorSpaceDefault, flipY: false,
            premultiply: true,
            colorSpace: retVal.colorSpaceEffect ? 'default' : 'empty' },
        { sub: true, bitmap: bitmaps.colorSpaceDefault, flipY: false,
            premultiply: true,
            colorSpace: retVal.colorSpaceEffect ? 'default' : 'empty' },
    ];

    for (var i in cases) {
        runOneIteration(cases[i].sub, bindingTarget, program, cases[i].bitmap,
            cases[i].flipY, cases[i].premultiply, retVal, cases[i].colorSpace,
            testOptions);
    }
}

// createImageBitmap resize code has two separate code paths for premul and
// unpremul image sources when the resize quality is set to high.
function runTest(bitmaps, alphaVal, colorSpaceEffective, testOptions)
{
    var retVal = {testPassed: true, alpha: alphaVal,
        colorSpaceEffect: colorSpaceEffective};
    var program = tiu.setupTexturedQuad(gl, internalFormat);
    runTestOnBindingTarget(gl.TEXTURE_2D, program, bitmaps, retVal,
                           testOptions);
    program = tiu.setupTexturedQuadWithCubeMap(gl, internalFormat);
    runTestOnBindingTarget(gl.TEXTURE_CUBE_MAP, program, bitmaps, retVal,
                           testOptions);
    return retVal.testPassed;
}

function prepareResizedImageBitmapsAndRuntTest(testOptions) {
    var bitmaps = [];
    var imageSource= testOptions.imageSource;
    var options = {resizeWidth: testOptions.resizeWidth,
                   resizeHeight: testOptions.resizeHeight,
                   resizeQuality: testOptions.resizeQuality};
    var p1 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.defaultOption = imageBitmap });

    options.imageOrientation = "none";
    options.premultiplyAlpha = "premultiply";
    var p2 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.noFlipYPremul = imageBitmap });

    options.premultiplyAlpha = "default";
    var p3 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.noFlipYDefault = imageBitmap });

    options.premultiplyAlpha = "none";
    var p4 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.noFlipYUnpremul = imageBitmap });

    options.imageOrientation = "flipY";
    options.premultiplyAlpha = "premultiply";
    var p5 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.flipYPremul = imageBitmap });

    options.premultiplyAlpha = "default";
    var p6 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.flipYDefault = imageBitmap });

    options.premultiplyAlpha = "none";
    var p7 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.flipYUnpremul = imageBitmap });

    options = {resizeWidth: testOptions.resizeWidth,
               resizeHeight: testOptions.resizeHeight,
               resizeQuality: testOptions.resizeQuality};
    var p8 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.colorSpaceDef = imageBitmap });

    options.colorSpaceConversion = "none";
    var p9 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.colorSpaceNone = imageBitmap });

    options.colorSpaceConversion = "default";
    var p10 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.colorSpaceDefault = imageBitmap });

    return Promise.all([p1, p2, p3, p4, p5, p6, p7, p8, p9, p10]).then(
        function() {
            var alphaVal = 0.5;
            var testPassed = runTest(bitmaps, alphaVal, false, testOptions);
            if (!testPassed)
                assert_true(false, 'Test failed');
        }, function() {
            assert_true(false, 'Promise rejected');
        });
}

function prepareResizedImageBitmapsAndRuntTests(testOptions) {
    var resizeQualities = ["pixelated", "low", "medium", "high"];
    for (i = 0; i < resizeQualities.length; i++) {
        testOptions.resizeQuality = resizeQualities[i];
        promise_test(function() {
            return prepareResizedImageBitmapsAndRuntTest(testOptions);
        }, 'createImageBitmap(' + testOptions.sourceName + ') resize with ' +
           testOptions.resizeQuality + ' resize quality.');
    }
}

function prepareWebGLContext() {
    var canvas = document.createElement('canvas');
    canvas.width = 4;
    canvas.height = 4;
    document.body.appendChild(canvas);
    gl = canvas.getContext("webgl");
    gl.clearColor(0,0,0,1);
    gl.clearDepth(1);
}
