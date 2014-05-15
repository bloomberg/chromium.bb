// [Name] SVGEllipseElement-dom-requiredFeatures.js
// [Expected rendering result] a series of PASS messages

createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("rx", "200");
ellipseElement.setAttribute("ry", "200");

rootSVGElement.appendChild(ellipseElement);

function repaintTest() {
    debug("Check that SVGEllipseElement is initially displayed");
    shouldHaveBBox("ellipseElement", "400", "400");
    debug("Check that setting requiredFeatures to something invalid makes it not render");
    ellipseElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#BogusFeature");
    shouldHaveBBox("ellipseElement", "0", "0");
    debug("Check that setting requiredFeatures to something valid makes it render again");
    ellipseElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Shape");
    shouldHaveBBox("ellipseElement", "400", "400");
    debug("Check that adding something valid to requiredFeatures keeps rendering the element");
    ellipseElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#Gradient");
    shouldHaveBBox("ellipseElement", "400", "400");
    debug("Check that adding something invalid to requiredFeatures makes it not render");
    ellipseElement.setAttribute("requiredFeatures", "http://www.w3.org/TR/SVG11/feature#BogusFeature");
    shouldHaveBBox("ellipseElement", "0", "0");
}

var successfullyParsed = true;
