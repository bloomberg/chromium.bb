description("Series of tests to ensure fill() works with path and winding rule parameters.");

var ctx = document.getElementById('canvas').getContext('2d');

function pixelDataAtPoint() {
  return ctx.getImageData(50, 50, 1, 1).data;
}

function checkResult(expectedColors, sigma) {
    for (var i = 0; i < 4; i++)
      shouldBeCloseTo("pixelDataAtPoint()[" + i + "]", expectedColors[i], sigma);
}

function drawRectanglesOn(contextOrPath) {
  contextOrPath.rect(0, 0, 100, 100);
  contextOrPath.rect(25, 25, 50, 50);
}

function formatName(fillRule, path) {
  return 'fill(' + (path ? 'path' : '') + (fillRule && path ? ', ' : '') +
          (fillRule ? '"' + fillRule + '"' : '') + ')';
}

function testFillWith(fillRule, path) {
    debug('Testing ' + formatName(fillRule, path));
    ctx.fillStyle = 'rgb(255,0,0)';
    ctx.beginPath();
    ctx.fillRect(0, 0, 100, 100);
    ctx.fillStyle = 'rgb(0,255,0)';
    if (path) {
      if (fillRule) {
        ctx.fill(path, fillRule);
      } else {
        ctx.fill(path);
      }
    } else {
      ctx.beginPath();
      drawRectanglesOn(ctx);
      if (fillRule) {
        ctx.fill(fillRule);
      } else {
        ctx.fill();
      }
    }
    if (fillRule == 'evenodd') {
      checkResult([255, 0, 0, 255], 5);
    } else {
      checkResult([0, 255, 0, 255], 5);
    }
    debug('');
}

// Execute test.
function prepareTestScenario() {
    fillRules = [undefined, 'nonzero', 'evenodd'];
    path = new Path2D();
    drawRectanglesOn(path);

    for (var i = 0; i < fillRules.length; i++) {
       testFillWith(fillRules[i]);
       testFillWith(fillRules[i], path);
    }

    // Test exception cases.
    shouldThrow("ctx.fill(null)");
    shouldThrow("ctx.fill(null, null)");
    shouldThrow("ctx.fill(null, 'nonzero')");
    shouldThrow("ctx.fill(path, null)");
    shouldThrow("ctx.fill([], 'nonzero')");
    shouldThrow("ctx.fill({}, 'nonzero')");
    shouldThrow("ctx.fill(null, 'evenodd')");
    shouldThrow("ctx.fill([], 'evenodd')");
    shouldThrow("ctx.fill({}, 'evenodd')");
    shouldThrow("ctx.fill('gazonk')");
    shouldThrow("ctx.fill(path, 'gazonk')");
    shouldThrow("ctx.fill(undefined)");
    shouldThrow("ctx.fill(undefined, undefined)");
    shouldThrow("ctx.fill(undefined, path)");
    shouldThrow("ctx.fill(path, undefined)");
}

// Run test and allow variation of results.
prepareTestScenario();
