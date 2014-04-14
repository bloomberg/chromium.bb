description("Series of tests to ensure clip() works with path and winding rule parameters.");

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
  return 'clip(' + (path ? 'path' : '') + (fillRule && path ? ', ' : '') +
          (fillRule ? '"' + fillRule + '"' : '') + ')';
}

function testClipWith(fillRule, path) {
    debug('Testing ' + formatName(fillRule, path));
    ctx.fillStyle = 'rgb(255,0,0)';
    ctx.beginPath();
    ctx.fillRect(0, 0, 100, 100);
    ctx.fillStyle = 'rgb(0,255,0)';
    if (path) {
      if (fillRule) {
        ctx.clip(path, fillRule);
      } else {
        ctx.clip(path);
      }
    } else {
      ctx.beginPath();
      drawRectanglesOn(ctx);
      if (fillRule) {
        ctx.clip(fillRule);
      } else {
        ctx.clip();
      }
    }
    ctx.beginPath();
    ctx.fillRect(0, 0, 100, 100);
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
       testClipWith(fillRules[i]);
       testClipWith(fillRules[i], path);
    }

    // Test exception cases.
    shouldThrow("ctx.clip(null)");
    shouldThrow("ctx.clip(null, null)");
    shouldThrow("ctx.clip(null, 'nonzero')");
    shouldThrow("ctx.clip([], 'nonzero')");
    shouldThrow("ctx.clip({}, 'nonzero')");
    shouldThrow("ctx.clip(null, 'evenodd')");
    shouldThrow("ctx.clip([], 'evenodd')");
    shouldThrow("ctx.clip({}, 'evenodd')");
    shouldThrow("ctx.clip('gazonk')");
    shouldThrow("ctx.clip(path, 'gazonk')");
}

// Run test and allow variation of results.
prepareTestScenario();
