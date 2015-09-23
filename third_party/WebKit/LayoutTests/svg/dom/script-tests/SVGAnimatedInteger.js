description("This test checks the SVGAnimatedInteger API - utilizing the targetX property of SVGFEConvolveMatrix");

var feConvolveMatrix = document.createElementNS("http://www.w3.org/2000/svg", "feConvolveMatrix");

debug("");
debug("Check initial targetX value");
shouldBeEqualToString("feConvolveMatrix.targetX.toString()", "[object SVGAnimatedInteger]");
shouldBeEqualToString("typeof(feConvolveMatrix.targetX.baseVal)", "number");
shouldBe("feConvolveMatrix.targetX.baseVal", "0");

debug("");
debug("Check that integers are static, caching value in a local variable and modifying it, should have no effect");
var numRef = feConvolveMatrix.targetX.baseVal;
numRef = 100;
shouldBe("numRef", "100");
shouldBe("feConvolveMatrix.targetX.baseVal", "0");

debug("");
debug("Check assigning various valid and invalid values");
shouldBe("feConvolveMatrix.targetX.baseVal = -1", "-1"); // Negative values are allowed from SVG DOM, but should lead to an error when rendering (disable the filter)
shouldBe("feConvolveMatrix.targetX.baseVal = 300", "300");
// ECMA-262, 9.5, "ToInt32"
shouldBe("feConvolveMatrix.targetX.baseVal = 'aString'", "'aString'");
shouldBe("feConvolveMatrix.targetX.baseVal", "0");
shouldBe("feConvolveMatrix.targetX.baseVal = feConvolveMatrix", "feConvolveMatrix");
shouldBe("feConvolveMatrix.targetX.baseVal", "0");
shouldBe("feConvolveMatrix.targetX.baseVal = 300", "300");

debug("");
debug("Check that the targetX value remained 300");
shouldBe("feConvolveMatrix.targetX.baseVal", "300");

successfullyParsed = true;
