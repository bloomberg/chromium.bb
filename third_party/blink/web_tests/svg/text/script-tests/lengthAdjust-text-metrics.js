// Test that assures that SVGTextContentElement API takes lengthAdjust & co into account.
description("Complete test of the SVGTextContentElement API");

// Prepare testcase
var svgNS = "http://www.w3.org/2000/svg";

var svgRoot = document.createElementNS(svgNS, "svg");
document.documentElement.appendChild(svgRoot);

var svgText = document.createElementNS(svgNS, "text");
svgText.style.fontFamily = "Arial";
svgText.style.fontSize = "20px";
svgText.setAttribute("x", "10");
svgText.setAttribute("y", "20");
svgText.setAttribute("lengthAdjust", "spacingAndGlyphs");
svgText.setAttribute("textLength", "200");
svgText.appendChild(document.createTextNode("Test"));
svgRoot.appendChild(svgText);

// Build scale matrix, we need to handle the spacingAndGlyphs transformation on our own.
var startPosition = svgText.getStartPositionOfChar(0);
var scale = svgText.textLength.baseVal.value / svgText.getComputedTextLength();
var scaleMatrix = svgRoot.createSVGMatrix()
                         .translate(startPosition.x, startPosition.y)
                         .scaleNonUniform(scale, 1)
                         .translate(-startPosition.x, -startPosition.y);
var inverseScaleMatrix = scaleMatrix.inverse();

function transformPoint(point, matrix) {
    return point.matrixTransform(matrix);
}

function transformRect(rect, matrix) {
    var topLeft = svgRoot.createSVGPoint();
    topLeft.x = rect.x;
    topLeft.y = rect.y;
    topLeft = transformPoint(topLeft, matrix);

    var bottomRight = svgRoot.createSVGPoint();
    bottomRight.x = rect.x + rect.width;
    bottomRight.y = rect.y + rect.height;
    bottomRight = transformPoint(bottomRight, matrix);

    var newRect = svgRoot.createSVGRect();
    newRect.x = topLeft.x;
    newRect.y = topLeft.y;
    newRect.width = bottomRight.x - topLeft.x;
    newRect.height = bottomRight.y - topLeft.y;
    return newRect;
}

// Helper functions
function numberToString(number) {
    return number.toFixed(1);    
}

function lengthToString(number) {
    return numberToString(number * scale);
}

function pointToString(point) {
    point = point.matrixTransform(scaleMatrix);
    return "(" + numberToString(point.x) + "," + numberToString(point.y) + ")";
}

function rectToString(rect) {
    rect = transformRect(rect, scaleMatrix);
    return "(" + numberToString(rect.x) + "," + numberToString(rect.y) + ")-(" + numberToString(rect.width) + "x" + numberToString(rect.height) + ")";
}

debug("Test SVGTextContentElement SVG DOM properties");
shouldBeEqualToString("svgText.textLength.baseVal.value.toFixed(1)", "200.0");
shouldBe("svgText.lengthAdjust.baseVal", "SVGTextContentElement.LENGTHADJUST_SPACINGANDGLYPHS");

debug("");
debug("Test getNumberOfChars() API");
shouldBe("svgText.getNumberOfChars()", "4");

debug("");
debug("Test getComputedTextLength() API");
shouldBeEqualToString("lengthToString(svgText.getComputedTextLength())", "200.0");

debug("");
debug("Test getSubStringLength() API");
shouldBeEqualToString("lengthToString(svgText.getSubStringLength(0, 1))", "61.5");
shouldBeEqualToString("lengthToString(svgText.getSubStringLength(0, 2))", "117.9");
shouldBeEqualToString("lengthToString(svgText.getSubStringLength(0, 3))", "169.2");;
shouldBeEqualToString("lengthToString(svgText.getSubStringLength(0, 4))", "200.0");
shouldBeEqualToString("lengthToString(svgText.getSubStringLength(1, 1))", "56.4");
shouldBeEqualToString("lengthToString(svgText.getSubStringLength(1, 2))", "107.7");
shouldBeEqualToString("lengthToString(svgText.getSubStringLength(1, 3))", "138.5");
shouldBeEqualToString("lengthToString(svgText.getSubStringLength(2, 1))", "51.3");
shouldBeEqualToString("lengthToString(svgText.getSubStringLength(2, 2))", "82.1");
shouldBeEqualToString("lengthToString(svgText.getSubStringLength(3, 1))", "30.8");

debug("");
debug("Test getStartPositionOfChar() API");
shouldBeEqualToString("pointToString(svgText.getStartPositionOfChar(0))", "(10.0,20.0)");
shouldBeEqualToString("pointToString(svgText.getStartPositionOfChar(1))", "(71.5,20.0)");
shouldBeEqualToString("pointToString(svgText.getStartPositionOfChar(2))", "(127.9,20.0)");
shouldBeEqualToString("pointToString(svgText.getStartPositionOfChar(3))", "(179.2,20.0)");

debug("");
debug("Test getEndPositionOfChar() API");
shouldBeEqualToString("pointToString(svgText.getEndPositionOfChar(0))", "(71.5,20.0)");
shouldBeEqualToString("pointToString(svgText.getEndPositionOfChar(1))", "(127.9,20.0)");
shouldBeEqualToString("pointToString(svgText.getEndPositionOfChar(2))", "(179.2,20.0)");
shouldBeEqualToString("pointToString(svgText.getEndPositionOfChar(3))", "(210.0,20.0)");

debug("");
debug("Test getExtentOfChar() API");
shouldBeEqualToString("rectToString(svgText.getExtentOfChar(0))", "(10.0,1.9)-(61.5x22.3)");
shouldBeEqualToString("rectToString(svgText.getExtentOfChar(1))", "(71.5,1.9)-(56.4x22.3)");
shouldBeEqualToString("rectToString(svgText.getExtentOfChar(2))", "(127.9,1.9)-(51.3x22.3)");
shouldBeEqualToString("rectToString(svgText.getExtentOfChar(3))", "(179.2,1.9)-(30.8x22.3)");

debug("");
debug("Test getRotationOfChar() API");
shouldBeEqualToString("svgText.getRotationOfChar(0).toFixed(1)", "0.0");
shouldBeEqualToString("svgText.getRotationOfChar(1).toFixed(1)", "0.0");
shouldBeEqualToString("svgText.getRotationOfChar(2).toFixed(1)", "0.0");
shouldBeEqualToString("svgText.getRotationOfChar(3).toFixed(1)", "0.0");

var point = svgRoot.createSVGPoint();
point.y = 10.0;

debug("");
debug("Test getCharNumAtPosition() API");

point.x = 0.0;
point = point.matrixTransform(inverseScaleMatrix);
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "-1");

point.x = 9.9;
point = point.matrixTransform(inverseScaleMatrix);
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "-1");

point.x = 10.1;
point = point.matrixTransform(inverseScaleMatrix);
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "0");

point.x = 71.4;
point = point.matrixTransform(inverseScaleMatrix);
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "0");

point.x = 71.6;
point = point.matrixTransform(inverseScaleMatrix);
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "1");

point.x = 127.8;
point = point.matrixTransform(inverseScaleMatrix);
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "1");

point.x = 128.0;
point = point.matrixTransform(inverseScaleMatrix);
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "2");

point.x = 179.1;
point = point.matrixTransform(inverseScaleMatrix);
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "2");

point.x = 179.3;
point = point.matrixTransform(inverseScaleMatrix);
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "3");

point.x = 209.9;
point = point.matrixTransform(inverseScaleMatrix);
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "3");

point.x = 210.1;
point = point.matrixTransform(inverseScaleMatrix);
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "-1");

point.x = 250.0;
point = point.matrixTransform(inverseScaleMatrix);
debug("> Testing point=" + pointToString(point));
shouldBe("svgText.getCharNumAtPosition(point)", "-1");

// Cleanup
document.documentElement.removeChild(svgRoot);
var successfullyParsed = true;
