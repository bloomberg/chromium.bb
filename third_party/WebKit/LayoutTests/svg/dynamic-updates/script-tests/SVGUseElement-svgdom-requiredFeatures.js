// [Name] SVGUseElement-svgdom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var useElement = createSVGElement("use");
var defsElement = createSVGElement("defs");
var rectElement = createSVGElement("rect");
rectElement.setAttribute("width", "200");
rectElement.setAttribute("height", "200");
rectElement.setAttribute("id", "MyRect");
useElement.setAttributeNS(xlinkNS, "xlink:href", "#MyRect");

defsElement.appendChild(rectElement);
rootSVGElement.appendChild(defsElement);
rootSVGElement.appendChild(useElement);

function repaintTest() {
    debug("Check that SVGUseElement is initially displayed");
    shouldHaveBBox("useElement", "200", "200");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    useElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#BogusFeature");
    shouldHaveBBox("useElement", "0", "0");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    useElement.requiredFeatures.replaceItem("http://www.w3.org/TR/SVG11/feature#Shape", 0);
    shouldHaveBBox("useElement", "200", "200");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    useElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldHaveBBox("useElement", "200", "200");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    useElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#BogusFeature");
    shouldHaveBBox("useElement", "0", "0");
}

var successfullyParsed = true;
