description("Test for supporting setTransform on canvas patterns");

function pixelValueAt(context, x, y) {
    var imageData = context.getImageData(x, y, 1, 1);
    return imageData.data;
}

function pixelToString(p) {
    return "[" + p[0] + ", " + p[1] + ", " + p[2] + ", " + p[3] + "]"
}

function pixelShouldBe(context, x, y, expectedPixelString) {
    var pixel = pixelValueAt(context, x, y);
    var expectedPixel = eval(expectedPixelString);

    var pixelString = "pixel " + x + ", " + y;
    if (areArraysEqual(pixel, expectedPixel)) {
        testPassed(pixelString + " is " + pixelToString(pixel));
    } else {
        testFailed(pixelString + " should be " + pixelToString(expectedPixel) + " was " + pixelToString(pixel));
    }
}

function fillWithColor(context, canvas, color1, color2) {
    context.save();
    context.fillStyle = color1;
    context.fillRect(0, 0, canvas.width / 2, canvas.height);
    context.fillStyle = color2;
    context.fillRect(canvas.width / 2, 0, canvas.width / 2, canvas.height);
    context.restore();
}

var canvas = document.createElement("canvas");
canvas.height = 100;
canvas.width = 100;
canvas.style.height = "100";
canvas.style.width = "100";

document.body.appendChild(canvas);

var patternImage = document.createElement("canvas");
patternImage.height = 10;
patternImage.width = 20;
var patternImageCtx = patternImage.getContext('2d');
fillWithColor(patternImageCtx, patternImage, "red", "green");
var greenPixel = pixelValueAt(patternImageCtx, 10, 0);


var ctx = canvas.getContext('2d');
var pattern = ctx.createPattern(patternImage, "repeat-x");
var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
var matrix = svgElement.createSVGMatrix();
matrix = matrix.translate(10, 0);
pattern.setTransform(matrix);

fillWithColor(ctx, canvas, "blue", "blue");

ctx.fillStyle = pattern;
ctx.translate(20, 20);
ctx.fillRect(0, 0, 10, 10);
pixelShouldBe(ctx, 20, 20, "greenPixel");
