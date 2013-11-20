description("Series of tests to ensure correct behaviour when canvas size is extremely large.");
var canvas = document.createElement('canvas')
var ctx = canvas.getContext('2d');

// WebIDL defines width and height as int. 2147483647 is int max.
var extremelyLargeNumber = 2147483647;
canvas.width = extremelyLargeNumber;
canvas.height = extremelyLargeNumber;

debug("check for crash on extremely large canvas size.");
useCanvasContext(ctx);
var imageData = ctx.getImageData(1, 1, 98, 98);
var imgdata = imageData.data;
// Blink returns zero color if the image buffer does not exist.
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "0");
shouldBe("imgdata[6]", "0");

debug("check for crash after resetting to the same size.");
canvas.width = extremelyLargeNumber;
useCanvasContext(ctx);
imageData = ctx.getImageData(1, 1, 98, 98);
imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "0");
shouldBe("imgdata[6]", "0");

// googol is parsed to 0.
var googol = Math.pow(10, 100);
debug("check for crash after resizing to googol.");
canvas.width = googol;
canvas.height = googol;
useCanvasContext(ctx);
imageData = ctx.getImageData(1, 1, 98, 98);
imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "0");
shouldBe("imgdata[6]", "0");

debug("check for crash after resetting to the same size.");
canvas.width = googol;
useCanvasContext(ctx);
imageData = ctx.getImageData(1, 1, 98, 98);
imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "0");
shouldBe("imgdata[6]", "0");

debug("check again for crash on extremely large canvas size.");
canvas.width = extremelyLargeNumber;
canvas.height = extremelyLargeNumber;
useCanvasContext(ctx);
imageData = ctx.getImageData(1, 1, 98, 98);
imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "0");
shouldBe("imgdata[6]", "0");

function useCanvasContext(ctx) {
    ctx.fillStyle = 'green';
    ctx.fillRect(0, 0, 100, 100);
    for(var i = 0; i < 100; i++) {
        // This API tries to create an image buffer if the image buffer is not created.
        ctx.getImageData(1, 1, 1, 1);
    }
    ctx.beginPath();
    ctx.rect(0,0,100,100);
    ctx.save();
    ctx.fillStyle = 'red';
    ctx.fillRect(0, 0, 100, 100);
    ctx.restore();
    ctx.fillStyle = 'green';
    ctx.fill();
}

debug("after resizing to normal size, the canvas must be in a valid state.");
canvas.width = 100;
canvas.height = 100;
ctx.fillStyle = 'blue';
ctx.fillRect(0, 0, 100, 100);
imageData = ctx.getImageData(1, 1, 98, 98);
imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "0");
shouldBe("imgdata[6]", "255");
