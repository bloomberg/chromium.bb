function checkCanvasRect(buf, x, y, width, height, color, tolerance, bufWidth, retVal)
{
    for (var px = x; px < x + width; px++) {
        for (var py = y; py < y + height; py++) {
            var offset = (py * bufWidth + px) * 4;
            for (var j = 0; j < color.length; j++) {
                if (Math.abs(buf[offset + j] - color[j]) > tolerance) {
                    retVal.testPassed = false;
                    return;
                }
            }
        }
    }
}

function runOneIteration(useTexSubImage2D, bindingTarget, program, bitmap, flipY, premultiplyAlpha, retVal, colorspace = 'empty')
{
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    // Enable writes to the RGBA channels
    gl.colorMask(1, 1, 1, 0);
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
            gl.texImage2D(targets[tt], 0, gl[internalFormat], bitmap.width, bitmap.height, 0,
                          gl[pixelFormat], gl[pixelType], null);
            gl.texSubImage2D(targets[tt], 0, 0, 0, gl[pixelFormat], gl[pixelType], bitmap);
        } else {
            gl.texImage2D(targets[tt], 0, gl[internalFormat], gl[pixelFormat], gl[pixelType], bitmap);
        }
    }

    var width = gl.canvas.width;
    var halfWidth = Math.floor(width / 2);
    var quaterWidth = Math.floor(halfWidth / 2);
    var height = gl.canvas.height;
    var halfHeight = Math.floor(height / 2);
    var quaterHeight = Math.floor(halfHeight / 2);

    var top = flipY ? quaterHeight : (height - halfHeight + quaterHeight);
    var bottom = flipY ? (height - halfHeight + quaterHeight) : quaterHeight;

    var tl = redColor;
    var tr = premultiplyAlpha ? ((retVal.alpha == 0.5) ? halfRed : (retVal.alpha == 1) ? redColor : blackColor) : redColor;
    var bl = greenColor;
    var br = premultiplyAlpha ? ((retVal.alpha == 0.5) ? halfGreen : (retVal.alpha == 1) ? greenColor : blackColor) : greenColor;

    var blueColor = [0, 0, 255];
    if (colorspace == 'none') {
        tl = tr = bl = br = blueColor;
    } else if (colorspace == 'default' || colorspace == 'notprovided') {
        tl = tr = bl = br = redColor;
    }

    var loc;
    if (bindingTarget == gl.TEXTURE_CUBE_MAP) {
        loc = gl.getUniformLocation(program, "face");
    }

    var tolerance = (retVal.alpha == 0) ? 0 : 3;
    if (colorspace == 'default' || colorspace == 'none' || colorspace == 'notprovided')
        tolerance = 13; // For linux and win, the tolerance can be 8.
    for (var tt = 0; tt < targets.length; ++tt) {
        if (bindingTarget == gl.TEXTURE_CUBE_MAP) {
            gl.uniform1i(loc, targets[tt]);
        }
        // Draw the triangles
        gl.clearColor(0, 0, 0, 1);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        gl.drawArrays(gl.TRIANGLES, 0, 6);

        // Check the top pixel and bottom pixel and make sure they have
        // the right color.
        var buf = new Uint8Array(width * height * 4);
        gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, buf);
        checkCanvasRect(buf, quaterWidth, bottom, 2, 2, tl, tolerance, width, retVal);
        checkCanvasRect(buf, halfWidth + quaterWidth, bottom, 2, 2, tr, tolerance, width, retVal);
        checkCanvasRect(buf, quaterWidth, top, 2, 2, bl, tolerance, width, retVal);
        checkCanvasRect(buf, halfWidth + quaterWidth, top, 2, 2, br, tolerance, width, retVal);
    }
}

function runTestOnBindingTarget(bindingTarget, program, bitmaps, retVal) {
    var cases = [
        { sub: false, bitmap: bitmaps.defaultOption, flipY: false, premultiply: true, colorspace: 'empty' },
        { sub: true, bitmap: bitmaps.defaultOption, flipY: false, premultiply: true, colorspace: 'empty' },
        { sub: false, bitmap: bitmaps.noFlipYPremul, flipY: false, premultiply: true, colorspace: 'empty' },
        { sub: true, bitmap: bitmaps.noFlipYPremul, flipY: false, premultiply: true, colorspace: 'empty' },
        { sub: false, bitmap: bitmaps.noFlipYDefault, flipY: false, premultiply: true, colorspace: 'empty' },
        { sub: true, bitmap: bitmaps.noFlipYDefault, flipY: false, premultiply: true, colorspace: 'empty' },
        { sub: false, bitmap: bitmaps.noFlipYUnpremul, flipY: false, premultiply: false, colorspace: 'empty' },
        { sub: true, bitmap: bitmaps.noFlipYUnpremul, flipY: false, premultiply: false, colorspace: 'empty' },
        { sub: false, bitmap: bitmaps.flipYPremul, flipY: true, premultiply: true, colorspace: 'empty' },
        { sub: true, bitmap: bitmaps.flipYPremul, flipY: true, premultiply: true, colorspace: 'empty' },
        { sub: false, bitmap: bitmaps.flipYDefault, flipY: true, premultiply: true, colorspace: 'empty' },
        { sub: true, bitmap: bitmaps.flipYDefault, flipY: true, premultiply: true, colorspace: 'empty' },
        { sub: false, bitmap: bitmaps.flipYUnpremul, flipY: true, premultiply: false, colorspace: 'empty' },
        { sub: true, bitmap: bitmaps.flipYUnpremul, flipY: true, premultiply: false, colorspace: 'empty' },
        { sub: false, bitmap: bitmaps.colorspaceDef, flipY: false, premultiply: true, colorspace: retVal.colorspaceEffect ? 'notprovided' : 'empty' },
        { sub: true, bitmap: bitmaps.colorspaceDef, flipY: false, premultiply: true, colorspace: retVal.colorspaceEffect ? 'notprovided' : 'empty' },
        { sub: false, bitmap: bitmaps.colorspaceNone, flipY: false, premultiply: true, colorspace: retVal.colorspaceEffect ? 'none' : 'empty' },
        { sub: true, bitmap: bitmaps.colorspaceNone, flipY: false, premultiply: true, colorspace: retVal.colorspaceEffect ? 'none' : 'empty' },
        { sub: false, bitmap: bitmaps.colorspaceDefault, flipY: false, premultiply: true, colorspace: retVal.colorspaceEffect ? 'default' : 'empty' },
        { sub: true, bitmap: bitmaps.colorspaceDefault, flipY: false, premultiply: true, colorspace: retVal.colorspaceEffect ? 'default' : 'empty' },
    ];

    for (var i in cases) {
        runOneIteration(cases[i].sub, bindingTarget, program, cases[i].bitmap, cases[i].flipY,
            cases[i].premultiply, retVal, cases[i].colorspace);
    }
}

function runTest(bitmaps, alphaVal, colorspaceEffective)
{
    var retVal = {testPassed: true, alpha: alphaVal, colorspaceEffect: colorspaceEffective};
    var program = tiu.setupTexturedQuad(gl, internalFormat);
    runTestOnBindingTarget(gl.TEXTURE_2D, program, bitmaps, retVal);
    program = tiu.setupTexturedQuadWithCubeMap(gl, internalFormat);
    runTestOnBindingTarget(gl.TEXTURE_CUBE_MAP, program, bitmaps, retVal);
    return retVal.testPassed;
}
