description("Series of tests to ensure stroke() works with optional path parameter.");

var ctx = document.getElementById('canvas').getContext('2d');

function pixelDataAtPoint() {
  return ctx.getImageData(75, 75, 1, 1).data;
}

function checkResult(expectedColors, sigma) {
    for (var i = 0; i < 4; i++)
      shouldBeCloseTo("pixelDataAtPoint()[" + i + "]", expectedColors[i], sigma);
}

function drawRectangleOn(contextOrPath) {
  contextOrPath.rect(25, 25, 50, 50);
}

function formatName(path) {
  return 'stroke(' + (path ? 'path' : '') + ')';
}

function testStrokeWith(path) {
    debug('Testing ' + formatName(path));
    ctx.fillStyle = 'rgb(255,0,0)';
    ctx.beginPath();
    ctx.fillRect(0, 0, 100, 100);
    ctx.strokeStyle = 'rgb(0,255,0)';
    ctx.lineWidth = 5;
    if (path) {
      ctx.stroke(path);
    } else {
      ctx.beginPath();
      drawRectangleOn(ctx);
      ctx.stroke();
    }
    debug('');
    checkResult([0, 255, 0, 255], 5);
}

// Execute test.
function prepareTestScenario() {
    var path = new Path2D();
    drawRectangleOn(path);

    testStrokeWith();
    testStrokeWith(path);

    // Test exception cases.
    shouldThrow("ctx.stroke(null)");
    shouldThrow("ctx.stroke(undefined)");
    shouldThrow("ctx.stroke([])");
    shouldThrow("ctx.stroke({})");
}

// Run test and allow variation of results.
prepareTestScenario();
