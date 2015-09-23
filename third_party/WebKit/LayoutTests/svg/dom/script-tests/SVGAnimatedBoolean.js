description("This test checks the SVGAnimatedBoolean API - utilizing the preserveAlpha property of SVGFEConvolveMatrixElement");

var convElement = document.createElementNS("http://www.w3.org/2000/svg", "feConvolveMatrix");
debug("");
debug("Check initial preserveAlpha value");
shouldBe("convElement.preserveAlpha.baseVal", "false");

debug("");
debug("Set value to true");
shouldBe("convElement.preserveAlpha.baseVal = true", "true");

debug("");
debug("Caching baseVal in local variable");
var baseVal = convElement.preserveAlpha.baseVal;
shouldBe("baseVal", "true");

debug("");
debug("Modify local baseVal variable to true");
shouldBeFalse("baseVal = false");

debug("");
debug("Assure that convElement.preserveAlpha has not been changed, but the local baseVal variable");
shouldBe("baseVal", "false");
shouldBe("convElement.preserveAlpha.baseVal", "true");

debug("");
debug("Check assigning values of various types");
// ECMA-262, 9.2, "ToBoolean"
shouldBe("convElement.preserveAlpha.baseVal = convElement.preserveAlpha", "convElement.preserveAlpha");
shouldBe("convElement.preserveAlpha.baseVal", "true");
shouldBeNull("convElement.preserveAlpha.baseVal = null");
shouldBe("convElement.preserveAlpha.baseVal", "false");
shouldBe("convElement.preserveAlpha.baseVal = 'aString'", "'aString'");
shouldBe("convElement.preserveAlpha.baseVal", "true");
convElement.preserveAlpha.baseVal = false;
shouldBe("convElement.preserveAlpha.baseVal = convElement", "convElement");
shouldBe("convElement.preserveAlpha.baseVal", "true");

successfullyParsed = true;
