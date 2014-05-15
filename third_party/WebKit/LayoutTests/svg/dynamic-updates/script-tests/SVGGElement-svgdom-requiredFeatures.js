// [Name] SVGGElement-svgdom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var gElement = createSVGElement("g");
var imageElement = createSVGElement("image");
imageElement.setAttribute("width", "200");
imageElement.setAttribute("height", "200");

gElement.appendChild(imageElement);
rootSVGElement.appendChild(gElement);

function repaintTest() {
    debug("Check that SVGGElement is initially displayed");
    shouldHaveBBox("gElement.firstElementChild", "200", "200");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    gElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#BogusFeature");
    shouldHaveBBox("gElement.firstElementChild", "0", "0");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    gElement.requiredFeatures.replaceItem("http://www.w3.org/TR/SVG11/feature#Shape", 0);
    shouldHaveBBox("gElement.firstElementChild", "200", "200");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    gElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldHaveBBox("gElement.firstElementChild", "200", "200");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    gElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#BogusFeature");
    shouldHaveBBox("gElement.firstElementChild", "0", "0");
}

var successfullyParsed = true;
