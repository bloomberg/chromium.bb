var refBufPremul = new Uint8Array([
    255, 0, 0, 255,   192, 63, 0, 255,   61, 192, 0, 255,     0, 255, 2, 255,
    192, 0, 60, 255,  157, 62, 62, 255,  96, 191, 67, 255,    62, 255, 70, 255,
    62, 0, 184, 255,  93, 62, 187, 255,  160, 190, 191, 255,  192, 255, 194, 255,
    0, 0, 252, 255,   58, 61, 255, 255,  195, 189, 255, 255,  255, 255, 255, 255]);

var refBufUnpremul = new Uint8Array([
    255, 0, 0, 255,   192, 63, 0, 255,   61, 192, 0, 255,     0, 255, 2, 255,
    192, 0, 60, 255,  157, 62, 62, 255,  96, 191, 67, 255,    62, 255, 70, 255,
    62, 0, 184, 255,  93, 62, 187, 255,  160, 190, 191, 255,  192, 255, 194, 255,
    0, 0, 252, 255,   58, 61, 255, 255,  195, 189, 255, 255,  255, 255, 255, 255]);

var refBufFlipY = new Uint8Array([
    0, 0, 252, 255,   58, 61, 255, 255,  195, 189, 255, 255,  255, 255, 255, 255,
    62, 0, 184, 255,  93, 62, 187, 255,  160, 190, 191, 255,  192, 255, 194, 255,
    192, 0, 60, 255,  157, 62, 62, 255,  96, 191, 67, 255,    62, 255, 70, 255,
    255, 0, 0, 255,   192, 63, 0, 255,   61, 192, 0, 255,     0, 255, 2, 255]);

var refBufUnpremulFlipY = new Uint8Array([
    0, 0, 252, 255,   58, 61, 255, 255,  195, 189, 255, 255,  255, 255, 255, 255,
    62, 0, 184, 255,  93, 62, 187, 255,  160, 190, 191, 255,  192, 255, 194, 255,
    192, 0, 60, 255,  157, 62, 62, 255,  96, 191, 67, 255,    62, 255, 70, 255,
    255, 0, 0, 255,   192, 63, 0, 255,   61, 192, 0, 255,     0, 255, 2, 255]);

function checkCanvas(buf, refBuf, tolerance, retVal)
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
                         flipY, premultiplyAlpha, retVal, colorSpace = 'empty')
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

    var tolerance = (retVal.alpha == 0) ? 0 : 10;
    for (var tt = 0; tt < targets.length; ++tt) {
        if (bindingTarget == gl.TEXTURE_CUBE_MAP) {
            gl.uniform1i(loc, targets[tt]);
        }
        // Draw the triangles
        gl.clearColor(0, 0, 0, 1);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        gl.drawArrays(gl.TRIANGLES, 0, 6);
        // Select the proper reference buffer
        var refBuf = refBufPremul;
        if (flipY && !premultiplyAlpha)
            refBuf = refBufUnpremulFlipY;
        else if (!premultiplyAlpha)
            refBuf = refBufUnpremul;
        else if (flipY)
            refBuf = refBufFlipY;
        // Check the pixels
        var buf = new Uint8Array(width * height * 4);
        gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, buf);
        checkCanvas(buf, refBuf, tolerance, retVal);
    }
}

function runTestOnBindingTarget(bindingTarget, program, bitmaps, retVal) {
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
                        cases[i].flipY, cases[i].premultiply, retVal,
                        cases[i].colorSpace);
    }
}

function runTest(bitmaps, alphaVal, colorSpaceEffective)
{
    var retVal = {testPassed: true, alpha: alphaVal,
                  colorSpaceEffect: colorSpaceEffective};
    var program = tiu.setupTexturedQuad(gl, internalFormat);
    runTestOnBindingTarget(gl.TEXTURE_2D, program, bitmaps, retVal);
    program = tiu.setupTexturedQuadWithCubeMap(gl, internalFormat);
    runTestOnBindingTarget(gl.TEXTURE_CUBE_MAP, program, bitmaps, retVal);
    return retVal.testPassed;
}
