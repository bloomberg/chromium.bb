description("Series of tests to ensure correct results of the winding rule in isPointInPath.");

var tmpimg = document.createElement('canvas');
tmpimg.width = 200;
tmpimg.height = 200;
ctx = tmpimg.getContext('2d');

// Execute test.
function prepareTestScenario() {
    debug('Testing default isPointInPath');
    ctx.beginPath();
    ctx.rect(0, 0, 100, 100);
    ctx.rect(25, 25, 50, 50);
    shouldBeTrue("ctx.isPointInPath(50, 50)");
    shouldBeFalse("ctx.isPointInPath(NaN, 50)");
    shouldBeFalse("ctx.isPointInPath(50, NaN)");
    debug('');

    debug('Testing nonzero isPointInPath');
    ctx.beginPath();
    ctx.rect(0, 0, 100, 100);
    ctx.rect(25, 25, 50, 50);
    shouldBeTrue("ctx.isPointInPath(50, 50, 'nonzero')");
    debug('');
	
    debug('Testing evenodd isPointInPath');
    ctx.beginPath();
    ctx.rect(0, 0, 100, 100);
    ctx.rect(25, 25, 50, 50);
    shouldBeFalse("ctx.isPointInPath(50, 50, 'evenodd')");
    debug('');

    // reset path in context
    ctx.beginPath();

    debug('Testing default isPointInPath with Path object');
    path = new Path2D();
    path.rect(0, 0, 100, 100);
    path.rect(25, 25, 50, 50);
    shouldBeTrue("ctx.isPointInPath(path, 50, 50)");
    shouldBeFalse("ctx.isPointInPath(path, NaN, 50)");
    shouldBeFalse("ctx.isPointInPath(path, 50, NaN)");
    debug('');

    debug('Testing nonzero isPointInPath with Path object');
    path = new Path2D();
    path.rect(0, 0, 100, 100);
    path.rect(25, 25, 50, 50);
    shouldBeTrue("ctx.isPointInPath(path, 50, 50, 'nonzero')");
    debug('');

    debug('Testing evenodd isPointInPath with Path object');
    path = new Path2D();
    path.rect(0, 0, 100, 100);
    path.rect(25, 25, 50, 50);
    shouldBeFalse("ctx.isPointInPath(path, 50, 50, 'evenodd')");
    debug('');

    debug('Testing invalid enumeration isPointInPath (w/ and w/o Path object');
    shouldThrow("ctx.isPointInPath(path, 50, 50, 'gazonk')");
    shouldThrow("ctx.isPointInPath(50, 50, 'gazonk')");
    debug('');

    debug('Testing invalid type isPointInPath with Path object');
    shouldThrow("ctx.isPointInPath(null, 50, 50)");
    shouldThrow("ctx.isPointInPath(null, 50, 50, 'nonzero')");
    shouldThrow("ctx.isPointInPath(null, 50, 50, 'evenodd')");
    shouldThrow("ctx.isPointInPath(null, 50, 50, null)");
    shouldThrow("ctx.isPointInPath(path, 50, 50, null)");
    shouldThrow("ctx.isPointInPath(undefined, 50, 50)");
    shouldThrow("ctx.isPointInPath(undefined, 50, 50, 'nonzero')");
    shouldThrow("ctx.isPointInPath(undefined, 50, 50, 'evenodd')");
    shouldThrow("ctx.isPointInPath(undefined, 50, 50, undefined)");
    shouldThrow("ctx.isPointInPath(path, 50, 50, undefined)");
    shouldThrow("ctx.isPointInPath([], 50, 50)");
    shouldThrow("ctx.isPointInPath([], 50, 50, 'nonzero')");
    shouldThrow("ctx.isPointInPath([], 50, 50, 'evenodd')");
    shouldThrow("ctx.isPointInPath({}, 50, 50)");
    shouldThrow("ctx.isPointInPath({}, 50, 50, 'nonzero')");
    shouldThrow("ctx.isPointInPath({}, 50, 50, 'evenodd')");
    debug('');

    debug("Testing extremely large scale")
    ctx.save();
    ctx.scale(Number.MAX_VALUE, Number.MAX_VALUE);
    ctx.beginPath();
    ctx.rect(-10, -10, 20, 20);
    shouldBeTrue("ctx.isPointInPath(0, 0, 'nonzero')");
    shouldBeTrue("ctx.isPointInPath(0, 0, 'evenodd')");
    ctx.restore();

    debug("Check with non-invertible ctm.")
    ctx.save();
    ctx.scale(0, 0);
    ctx.beginPath();
    ctx.rect(-10, -10, 20, 20);
    shouldBeFalse("ctx.isPointInPath(0, 0, 'nonzero')");
    shouldBeFalse("ctx.isPointInPath(0, 0, 'evenodd')");
    ctx.restore();
}

// Run test and allow variation of results.
prepareTestScenario();
