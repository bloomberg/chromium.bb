description("Test addPath() method.");
var ctx = document.createElement('canvas').getContext('2d');

debug("Test addPath() with transform as identity matrix.")
ctx.beginPath();
var p1 = new Path();
p1.rect(0,0,100,100);
var p2 = new Path();
p2.rect(0,100,100,100);
var m = ctx.currentTransform;
p1.addPath(p2, m);
ctx.fillStyle = 'yellow';
ctx.currentPath = p1;
ctx.fill();
var imageData = ctx.getImageData(0, 100, 100, 100);
var imgdata = imageData.data;
shouldBe("imgdata[4]", "255");
shouldBe("imgdata[5]", "255");
shouldBe("imgdata[6]", "0");
shouldBe("imgdata[7]", "255");
debug("");

debug("Test addPath() with transform as translate(100, -100).")
ctx.beginPath();
var p3 = new Path();
p3.rect(0,0,100,100);
var p4 = new Path();
p4.rect(0,100,100,100);
m.a = 1; m.b = 0;
m.c = 0; m.d = 1;
m.e = 100; m.f = -100;
p3.addPath(p4, m);
ctx.fillStyle = 'yellow';
ctx.currentPath = p3;
ctx.fill();
imageData = ctx.getImageData(100, 0, 100, 100);
imgdata = imageData.data;
shouldBe("imgdata[4]", "255");
shouldBe("imgdata[5]", "255");
shouldBe("imgdata[6]", "0");
shouldBe("imgdata[7]", "255");
debug("");

debug("Test addPath() with non-invertible transform.")
ctx.beginPath();
var p5 = new Path();
p5.rect(0,0,100,100);
var p6 = new Path();
p6.rect(100,100,100,100);
m.a = 0; m.b = 0;
m.c = 0; m.d = 0;
m.e = 0; m.f = 0;
p5.addPath(p6, m);
ctx.fillStyle = 'yellow';
ctx.currentPath = p5;
ctx.fill();
imageData = ctx.getImageData(100, 100, 100, 100);
imgdata = imageData.data;
shouldNotBe("imgdata[4]", "255");
shouldNotBe("imgdata[5]", "255");
shouldBe("imgdata[6]", "0");
shouldNotBe("imgdata[7]", "255");
debug("");

debug("Test addPath() with transform as null or invalid type.")
ctx.beginPath();
var p7 = new Path();
p7.rect(0,0,100,100);
var p8 = new Path();
p8.rect(100,100,100,100);
p7.addPath(p8, null);
p7.addPath(p8, []);
p7.addPath(p8, {});
ctx.fillStyle = 'red';
ctx.currentPath = p7;
ctx.fill();
imageData = ctx.getImageData(100, 100, 100, 100);
imgdata = imageData.data;
shouldBe("imgdata[4]", "255");
shouldBe("imgdata[5]", "0");
shouldBe("imgdata[6]", "0");
shouldBe("imgdata[7]", "255");
debug("");

debug("Test addPath() with path as null and invalid type");
var p9 = new Path();
p9.rect(0,0,100,100);
shouldThrow("p7.addPath(null, m)");
shouldThrow("p7.addPath([], m)");
shouldThrow("p7.addPath({}, m)");
debug("");
