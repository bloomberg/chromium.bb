description("Test the behavior of isPointInStroke in Canvas with path object");
var ctx = document.createElement('canvas').getContext('2d');

document.body.appendChild(ctx.canvas);

ctx.strokeStyle = '#0ff';

// Create new path.
var path = new Path2D();
path.rect(20,20,100,100);

debug("Initial behavior: lineWidth = 1.0")
shouldBeTrue("ctx.isPointInStroke(path,20,20)");
shouldBeTrue("ctx.isPointInStroke(path,120,20)");
shouldBeTrue("ctx.isPointInStroke(path,20,120)");
shouldBeTrue("ctx.isPointInStroke(path,120,120)");
shouldBeTrue("ctx.isPointInStroke(path,70,20)");
shouldBeTrue("ctx.isPointInStroke(path,20,70)");
shouldBeTrue("ctx.isPointInStroke(path,120,70)");
shouldBeTrue("ctx.isPointInStroke(path,70,120)");
shouldBeFalse("ctx.isPointInStroke(path,22,22)");
shouldBeFalse("ctx.isPointInStroke(path,118,22)");
shouldBeFalse("ctx.isPointInStroke(path,22,118)");
shouldBeFalse("ctx.isPointInStroke(path,118,118)");
shouldBeFalse("ctx.isPointInStroke(path,70,18)");
shouldBeFalse("ctx.isPointInStroke(path,122,70)");
shouldBeFalse("ctx.isPointInStroke(path,70,122)");
shouldBeFalse("ctx.isPointInStroke(path,18,70)");
shouldBeFalse("ctx.isPointInStroke(path,NaN,122)");
shouldBeFalse("ctx.isPointInStroke(path,18,NaN)");
debug("");

debug("Check invalid type");
shouldThrow("ctx.isPointInStroke(null,70,20)");
shouldThrow("ctx.isPointInStroke(undefined,70,20)");
shouldThrow("ctx.isPointInStroke([],20,70)");
shouldThrow("ctx.isPointInStroke({},120,70)");
debug("");

debug("Set lineWidth = 10.0");
ctx.lineWidth = 10;
shouldBeTrue("ctx.isPointInStroke(path,22,22)");
shouldBeTrue("ctx.isPointInStroke(path,118,22)");
shouldBeTrue("ctx.isPointInStroke(path,22,118)");
shouldBeTrue("ctx.isPointInStroke(path,118,118)");
shouldBeTrue("ctx.isPointInStroke(path,70,18)");
shouldBeTrue("ctx.isPointInStroke(path,122,70)");
shouldBeTrue("ctx.isPointInStroke(path,70,122)");
shouldBeTrue("ctx.isPointInStroke(path,18,70)");
shouldBeFalse("ctx.isPointInStroke(path,26,70)");
shouldBeFalse("ctx.isPointInStroke(path,70,26)");
shouldBeFalse("ctx.isPointInStroke(path,70,114)");
shouldBeFalse("ctx.isPointInStroke(path,114,70)");
debug("");

debug("Check lineJoin = 'bevel'");
path = new Path2D();
path.moveTo(10,10);
path.lineTo(110,20);
path.lineTo(10,30);
ctx.lineJoin = "bevel";
shouldBeFalse("ctx.isPointInStroke(path,113,20)");
debug("");

debug("Check lineJoin = 'miter'");
ctx.miterLimit = 40.0;
ctx.lineJoin = "miter";
shouldBeTrue("ctx.isPointInStroke(path,113,20)");
debug("");

debug("Check miterLimit = 2.0");
ctx.miterLimit = 2.0;
shouldBeFalse("ctx.isPointInStroke(path,113,20)");
debug("");

debug("Check lineCap = 'butt'");
path = new Path2D();
path.moveTo(10,10);
path.lineTo(110,10);
ctx.lineCap = "butt";
shouldBeFalse("ctx.isPointInStroke(path,112,10)");
debug("");

debug("Check lineCap = 'round'");
ctx.lineCap = "round";
shouldBeTrue("ctx.isPointInStroke(path,112,10)");
shouldBeFalse("ctx.isPointInStroke(path,117,10)");
debug("");

debug("Check lineCap = 'square'");
ctx.lineCap = "square";
shouldBeTrue("ctx.isPointInStroke(path,112,10)");
shouldBeFalse("ctx.isPointInStroke(path,117,10)");
debug("");

debug("Check setLineDash([10,10])");
ctx.lineCap = "butt";
ctx.setLineDash([10,10]);
shouldBeTrue("ctx.isPointInStroke(path,15,10)");
shouldBeFalse("ctx.isPointInStroke(path,25,10)");
shouldBeTrue("ctx.isPointInStroke(path,35,10)");
debug("");

debug("Check dashOffset = 10");
ctx.lineDashOffset = 10;
shouldBeFalse("ctx.isPointInStroke(path,15,10)");
shouldBeTrue("ctx.isPointInStroke(path,25,10)");
shouldBeFalse("ctx.isPointInStroke(path,35,10)");
