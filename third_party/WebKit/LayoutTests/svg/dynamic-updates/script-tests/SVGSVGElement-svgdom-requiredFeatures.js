// [Name] SVGSVGElement-svgdom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var svgElement = rootSVGElement;

function repaintTest() {
    debug("Check that SVGSVGElement is initially displayed");
    shouldBe("svgElement.getBoundingClientRect().width", "300");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    svgElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#BogusFeature");
    shouldBe("svgElement.getBoundingClientRect().width", "0");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    svgElement.requiredFeatures.replaceItem("http://www.w3.org/TR/SVG11/feature#Shape", 0);
    shouldBe("svgElement.getBoundingClientRect().width", "300");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    svgElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldBe("svgElement.getBoundingClientRect().width", "300");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    svgElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#BogusFeature");
    shouldBe("svgElement.getBoundingClientRect().width", "0");
}


var successfullyParsed = true;
