// [Name] SVGPolylineElement-svgdom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var polylineElement = createSVGElement("polyline");
polylineElement.setAttribute("points", "0,0 200,0 200,200 0, 200");

rootSVGElement.appendChild(polylineElement);

function repaintTest() {
    debug("Check that SVGPolylineElement is initially displayed");
    shouldHaveBBox("polylineElement", "200", "200");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    polylineElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#BogusFeature");
    shouldHaveBBox("polylineElement", "0", "0");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    polylineElement.requiredFeatures.replaceItem("http://www.w3.org/TR/SVG11/feature#Shape", 0);
    shouldHaveBBox("polylineElement", "200", "200");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    polylineElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldHaveBBox("polylineElement", "200", "200");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    polylineElement.requiredFeatures.appendItem("http://www.w3.org/TR/SVG11/feature#BogusFeature");
    shouldHaveBBox("polylineElement", "0", "0");
}

var successfullyParsed = true;
