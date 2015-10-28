jsTestIsAsync = true;
if (window.testRunner) {
    testRunner.waitUntilDone();
}

var numToBlobCalls = 9;
var numToDataURLCalls = 3;
var testImages = [];
var canvasCtxs = [];

// Create an original canvas with content
var canvas = document.createElement("canvas");
var ctx = canvas.getContext("2d");
ctx.fillStyle = "#EE21AF";
ctx.fillRect(0, 0, 250, 150);

function testIfAllImagesAreCorrect()
{
    // All resultant images should be the same as both async and main threads use the same image encoder
    var imageMatched = true;
    var firstImageData = canvasCtxs[0].getImageData(0, 0, 250, 150).data;
    for (var i = 1; i < (numToBlobCalls + numToDataURLCalls); i++) 
    {
        var nextImageData = canvasCtxs[i].getImageData(0, 0, 250, 150).data;
        for (var k = 0; k < firstImageData.length; k++) 
        {
            if (firstImageData[k]!=nextImageData[k]) 
            {
                imageMatched = false;
                break;
            }
        } 
        if (!imageMatched) 
            break;
    }
    if (imageMatched)
        testPassed("All images encoded by both async and main threads match one another");
    else 
        testFailed("Not all images encoded by async and main threads match one another");
     finishJSTest();
}

var counter = numToBlobCalls + numToDataURLCalls;
function onCanvasDrawCompleted(ctx_test) 
{
    counter = counter - 1;
    if (counter == 0) {
        testIfAllImagesAreCorrect();
        if (window.testRunner)
            testRunner.notifyDone();
    } 
}

function createTestCase(i)
{
    var canvas_test = document.createElement("canvas");
    var ctx_test = canvas_test.getContext("2d");
    canvasCtxs[i] = ctx_test;

    var newImg = new Image();
    newImg.onload = function() {
        ctx_test.drawImage(newImg, 0, 0, 250, 150);
        onCanvasDrawCompleted(ctx_test);
    }    
    testImages[i] = newImg;
}

for (var i = 0; i < (numToBlobCalls + numToDataURLCalls); i++) 
{
    createTestCase(i);
}
