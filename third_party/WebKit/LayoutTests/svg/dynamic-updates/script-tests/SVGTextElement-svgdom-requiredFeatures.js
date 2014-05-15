// [Name] SVGTextElement-svgdom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var textElement = createSVGElement("text");
textElement.appendChild(document.createTextNode('Text'));

rootSVGElement.appendChild(textElement);

function repaintTest() {
    debug("Check that SVGTextElement is initially displayed");
    shouldBeTrue("textElement.getBBox().width > 0");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    textElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#BogusFeature");
    shouldBe("textElement.getBBox().width", "0");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    textElement.requiredFeatures.replaceItem("http://www.w3.org/TR/SVG11/feature#Shape", 0);
    shouldBeTrue("textElement.getBBox().width > 0");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    textElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldBeTrue("textElement.getBBox().width > 0");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    textElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#BogusFeature");
    shouldBe("textElement.getBBox().width", "0");
}

var successfullyParsed = true;
