description('Test that setting and getting grid-columns and grid-rows works as expected');

debug("Test getting grid-columns and grid-rows set through CSS");
var gridWithNoneElement = document.getElementById("gridWithNoneElement");
shouldBe("getComputedStyle(gridWithNoneElement, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(gridWithNoneElement, '').getPropertyValue('grid-rows')", "'none'");

var gridWithFixedElement = document.getElementById("gridWithFixedElement");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('grid-columns')", "'10px'");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('grid-rows')", "'15px'");

var gridWithPercentElement = document.getElementById("gridWithPercentElement");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('grid-columns')", "'53%'");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('grid-rows')", "'27%'");

var gridWithAutoElement = document.getElementById("gridWithAutoElement");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('grid-columns')", "'auto'");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('grid-rows')", "'auto'");

var gridWithEMElement = document.getElementById("gridWithEMElement");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('grid-columns')", "'100px'");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('grid-rows')", "'150px'");

var gridWithViewPortPercentageElement = document.getElementById("gridWithViewPortPercentageElement");
shouldBe("getComputedStyle(gridWithViewPortPercentageElement, '').getPropertyValue('grid-columns')", "'64px'");
shouldBe("getComputedStyle(gridWithViewPortPercentageElement, '').getPropertyValue('grid-rows')", "'60px'");

var gridWithMinMax = document.getElementById("gridWithMinMax");
shouldBe("getComputedStyle(gridWithMinMax, '').getPropertyValue('grid-columns')", "'minmax(10%, 15px)'");
shouldBe("getComputedStyle(gridWithMinMax, '').getPropertyValue('grid-rows')", "'minmax(20px, 50%)'");

var gridWithMinContent = document.getElementById("gridWithMinContent");
shouldBe("getComputedStyle(gridWithMinContent, '').getPropertyValue('grid-columns')", "'min-content'");
shouldBe("getComputedStyle(gridWithMinContent, '').getPropertyValue('grid-rows')", "'min-content'");

var gridWithMaxContent = document.getElementById("gridWithMaxContent");
shouldBe("getComputedStyle(gridWithMaxContent, '').getPropertyValue('grid-columns')", "'max-content'");
shouldBe("getComputedStyle(gridWithMaxContent, '').getPropertyValue('grid-rows')", "'max-content'");

var gridWithFraction = document.getElementById("gridWithFraction");
shouldBe("getComputedStyle(gridWithFraction, '').getPropertyValue('grid-columns')", "'1fr'");
shouldBe("getComputedStyle(gridWithFraction, '').getPropertyValue('grid-rows')", "'2fr'");

debug("");
debug("Test getting wrong values for grid-columns and grid-rows through CSS (they should resolve to the default: 'none')");
var gridWithFitContentElement = document.getElementById("gridWithFitContentElement");
shouldBe("getComputedStyle(gridWithFitContentElement, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(gridWithFitContentElement, '').getPropertyValue('grid-rows')", "'none'");

var gridWithFitAvailableElement = document.getElementById("gridWithFitAvailableElement");
shouldBe("getComputedStyle(gridWithFitAvailableElement, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(gridWithFitAvailableElement, '').getPropertyValue('grid-rows')", "'none'");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

debug("");
debug("Test getting and setting grid-columns and grid-rows through JS");
element.style.gridColumns = "18px";
element.style.gridRows = "66px";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'18px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'66px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "55%";
element.style.gridRows = "40%";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'55%'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'40%'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "auto";
element.style.gridRows = "auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'auto'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'auto'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "10vw";
element.style.gridRows = "25vh";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'80px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'150px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "min-content";
element.style.gridRows = "min-content";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'min-content'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'min-content'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "max-content";
element.style.gridRows = "max-content";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'max-content'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'max-content'");

debug("");
debug("Test getting and setting grid-columns and grid-rows to minmax() values through JS");
element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "minmax(55%, 45px)";
element.style.gridRows = "minmax(30px, 40%)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'minmax(55%, 45px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'minmax(30px, 40%)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.gridColumns = "minmax(22em, 8vh)";
element.style.gridRows = "minmax(10vw, 5em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'minmax(220px, 48px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'minmax(80px, 50px)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "minmax(min-content, 8vh)";
element.style.gridRows = "minmax(10vw, min-content)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'minmax(min-content, 48px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'minmax(80px, min-content)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.gridColumns = "minmax(22em, max-content)";
element.style.gridRows = "minmax(max-content, 5em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'minmax(220px, max-content)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'minmax(max-content, 50px)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.gridColumns = "minmax(22em, max-content)";
element.style.gridRows = "minmax(max-content, 5em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'minmax(220px, max-content)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'minmax(max-content, 50px)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "minmax(min-content, max-content)";
element.style.gridRows = "minmax(max-content, min-content)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'minmax(min-content, max-content)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'minmax(max-content, min-content)'");

// Unit comparison should be case-insensitive.
element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "3600Fr";
element.style.gridRows = "154fR";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'3600fr'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'154fr'");

// Float values are allowed.
element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "3.1459fr";
element.style.gridRows = "2.718fr";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'3.1459fr'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'2.718fr'");

// A leading '+' is allowed.
element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "+3fr";
element.style.gridRows = "+4fr";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'3fr'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'4fr'");

debug("");
debug("Test setting grid-columns and grid-rows to bad values through JS");
element = document.createElement("div");
document.body.appendChild(element);
// No comma.
element.style.gridColumns = "minmax(10px 20px)";
// Only 1 argument provided.
element.style.gridRows = "minmax(10px)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
// Nested minmax.
element.style.gridColumns = "minmax(minmax(10px, 20px), 20px)";
// Only 2 arguments are allowed.
element.style.gridRows = "minmax(10px, 20px, 30px)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
// No breadth value.
element.style.gridColumns = "minmax()";
// No comma.
element.style.gridRows = "minmax(30px 30% 30em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
// Auto is not allowed inside minmax.
element.style.gridColumns = "minmax(auto, 8vh)";
element.style.gridRows = "minmax(10vw, auto)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "-2fr";
element.style.gridRows = "3ffr";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "-2.05fr";
element.style.gridRows = "+-3fr";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "0fr";
element.style.gridRows = "1r";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = ".0000fr";
element.style.gridRows = "13 fr"; // A dimension doesn't allow spaces between the number and the unit.
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

element.style.gridColumns = "7.-fr";
element.style.gridRows = "-8,0fr";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

debug("");
debug("Test setting grid-columns and grid-rows back to 'none' through JS");
element.style.gridColumns = "18px";
element.style.gridRows = "66px";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'18px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'66px'");
element.style.gridColumns = "none";
element.style.gridRows = "none";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

function testInherit()
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.gridColumns = "50px 'last'";
    parentElement.style.gridRows = "'first' 101%";

    element = document.createElement("div");
    parentElement.appendChild(element);
    element.style.gridColumns = "inherit";
    element.style.gridRows = "inherit";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'50px last'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'first 101%'");

    document.body.removeChild(parentElement);
}
debug("");
debug("Test setting grid-columns and grid-rows to 'inherit' through JS");
testInherit();

function testInitial()
{
    element = document.createElement("div");
    document.body.appendChild(element);
    element.style.gridColumns = "150% 'last'";
    element.style.gridRows = "'first' 1fr";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'150% last'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'first 1fr'");

    element.style.gridColumns = "initial";
    element.style.gridRows = "initial";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

    document.body.removeChild(element);
}
debug("");
debug("Test setting grid-columns and grid-rows to 'initial' through JS");
testInitial();
