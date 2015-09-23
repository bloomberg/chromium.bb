description("Ensure correct behavior of canvas with drawImage+shadow after scaling. A blue and red checkered pattern should be displayed.");

function print(message, color)
{
    var paragraph = document.createElement("div");
    paragraph.appendChild(document.createTextNode(message));
    paragraph.style.fontFamily = "monospace";
    if (color)
        paragraph.style.color = color;
    document.getElementById("console").appendChild(paragraph);
}

function shouldBeAround(a, b)
{
    var evalA;
    try {
        evalA = eval(a);
    } catch(e) {
        evalA = e;
    }

    if (Math.abs(evalA - b) < 10)
        print("PASS " + a + " is around " + b , "green")
    else
        print("FAIL " + a + " is not around " + b + " (actual: " + evalA + ")", "red");
}

// Create auxiliary canvas to draw to and create an image from.
// This is done instead of simply loading an image from the file system
// because that would throw a SECURITY_ERR DOM Exception.
var aCanvas = document.createElement('canvas');
aCanvas.width = 10;
aCanvas.height = 10;
aCanvas.setAttribute('height', '10');
var aCtx = aCanvas.getContext('2d');
aCtx.fillStyle = 'rgba(0, 0, 255, 1)';
aCtx.fillRect(0, 0, 50, 50);

// Create the image object to be drawn on the master canvas.
var img = new Image();
img.onload = drawImageToCanvasAndCheckPixels;
img.src = aCanvas.toDataURL(); // set a data URI of the base64 encoded image as the source

aCanvas.width = 10;
aCtx.fillStyle = 'rgba(0, 0, 255, 0.5)';
aCtx.fillRect(0, 0, 50, 50);
// Create the image object to be drawn on the master canvas.
var transparentImg = new Image();
transparentImg.onload = drawImageToCanvasAndCheckPixels;
transparentImg.src = aCanvas.toDataURL(); // set a data URI of the base64 encoded image as the source

// Create master canvas.
var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '150');
canvas.setAttribute('height', '110');
var ctx = canvas.getContext('2d');

var imagesLoaded = 0;

function drawImageToCanvasAndCheckPixels() {
    imagesLoaded = imagesLoaded + 1;
    if (imagesLoaded == 2) {
        ctx.scale(2, 2);
        ctx.shadowOffsetX = 20;
        ctx.shadowOffsetY = 20;
        ctx.fillStyle = 'rgba(0, 0, 255, 1)';

        ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
        ctx.drawImage(img, 10, 10);

        ctx.shadowColor = 'rgba(255, 0, 0, 0.3)';
        ctx.drawImage(img, 10, 30);

        ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
        ctx.shadowBlur = 10;
        ctx.drawImage(img, 30, 10);

        ctx.shadowColor = 'rgba(255, 0, 0, 0.3)';
        ctx.drawImage(img, 30, 30);

        ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
        ctx.drawImage(transparentImg, 50, 10);

        ctx.shadowColor = 'rgba(255, 0, 0, 0.3)';
        ctx.drawImage(transparentImg, 50, 30);

        checkPixels();
    }
}

var d; // imageData.data

function checkPixels() {

    // Verify solid shadow.
    d = ctx.getImageData(40, 40, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBe('d[3]', '255');

    d = ctx.getImageData(59, 59, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBe('d[3]', '255');

    // Verify solid alpha shadow.
    d = ctx.getImageData(41, 81, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '76');

    d = ctx.getImageData(59, 99, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '76');

    // Verify blurry shadow.
    d = ctx.getImageData(90, 39, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '114');

    d = ctx.getImageData(90, 60, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '114');

    d = ctx.getImageData(79, 50, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '114');

    d = ctx.getImageData(100, 50, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '114');

    // Verify blurry alpha shadow.
    d = ctx.getImageData(90, 79, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '34');

    d = ctx.getImageData(90, 100, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '34');

    d = ctx.getImageData(79, 90, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '34');

    d = ctx.getImageData(100, 90, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '34');

    // Verify blurry shadow of image with alpha
    d = ctx.getImageData(130, 39, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '57');

    d = ctx.getImageData(130, 60, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '57');

    d = ctx.getImageData(119, 50, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '57');

    d = ctx.getImageData(140, 50, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '57');

    // Verify blurry alpha shadow of image with alpha.
    d = ctx.getImageData(130, 79, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '17');

    d = ctx.getImageData(130, 100, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '17');

    d = ctx.getImageData(119, 90, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '17');

    d = ctx.getImageData(140, 90, 1, 1).data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '17');
    finishJSTest();
}

window.jsTestIsAsync = true;
