description('Test that setting and getting grid-definition-columns and grid-definition-rows works as expected');

debug("Test getting grid-definition-columns and grid-definition-rows set through CSS");
var gridWithNoneElement = document.getElementById("gridWithNoneElement");
shouldBe("getComputedStyle(gridWithNoneElement, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(gridWithNoneElement, '').getPropertyValue('grid-definition-rows')", "'none'");

var gridWithFixedElement = document.getElementById("gridWithFixedElement");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('grid-definition-columns')", "'10px'");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('grid-definition-rows')", "'15px'");

var gridWithPercentElement = document.getElementById("gridWithPercentElement");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('grid-definition-columns')", "'53%'");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('grid-definition-rows')", "'27%'");

var gridWithAutoElement = document.getElementById("gridWithAutoElement");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('grid-definition-columns')", "'auto'");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('grid-definition-rows')", "'auto'");

var gridWithEMElement = document.getElementById("gridWithEMElement");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('grid-definition-columns')", "'100px'");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('grid-definition-rows')", "'150px'");

var gridWithViewPortPercentageElement = document.getElementById("gridWithViewPortPercentageElement");
shouldBe("getComputedStyle(gridWithViewPortPercentageElement, '').getPropertyValue('grid-definition-columns')", "'64px'");
shouldBe("getComputedStyle(gridWithViewPortPercentageElement, '').getPropertyValue('grid-definition-rows')", "'60px'");

var gridWithMinMax = document.getElementById("gridWithMinMax");
shouldBe("getComputedStyle(gridWithMinMax, '').getPropertyValue('grid-definition-columns')", "'minmax(10%, 15px)'");
shouldBe("getComputedStyle(gridWithMinMax, '').getPropertyValue('grid-definition-rows')", "'minmax(20px, 50%)'");

var gridWithMinContent = document.getElementById("gridWithMinContent");
shouldBe("getComputedStyle(gridWithMinContent, '').getPropertyValue('grid-definition-columns')", "'min-content'");
shouldBe("getComputedStyle(gridWithMinContent, '').getPropertyValue('grid-definition-rows')", "'min-content'");

var gridWithMaxContent = document.getElementById("gridWithMaxContent");
shouldBe("getComputedStyle(gridWithMaxContent, '').getPropertyValue('grid-definition-columns')", "'max-content'");
shouldBe("getComputedStyle(gridWithMaxContent, '').getPropertyValue('grid-definition-rows')", "'max-content'");

var gridWithFraction = document.getElementById("gridWithFraction");
shouldBe("getComputedStyle(gridWithFraction, '').getPropertyValue('grid-definition-columns')", "'1fr'");
shouldBe("getComputedStyle(gridWithFraction, '').getPropertyValue('grid-definition-rows')", "'2fr'");

debug("");
debug("Test getting wrong values for grid-definition-columns and grid-definition-rows through CSS (they should resolve to the default: 'none')");
var gridWithFitContentElement = document.getElementById("gridWithFitContentElement");
shouldBe("getComputedStyle(gridWithFitContentElement, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(gridWithFitContentElement, '').getPropertyValue('grid-definition-rows')", "'none'");

var gridWithFitAvailableElement = document.getElementById("gridWithFitAvailableElement");
shouldBe("getComputedStyle(gridWithFitAvailableElement, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(gridWithFitAvailableElement, '').getPropertyValue('grid-definition-rows')", "'none'");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

debug("");
debug("Test getting and setting grid-definition-columns and grid-definition-rows through JS");
element.style.gridDefinitionColumns = "18px";
element.style.gridDefinitionRows = "66px";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'18px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'66px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "55%";
element.style.gridDefinitionRows = "40%";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'55%'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'40%'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "auto";
element.style.gridDefinitionRows = "auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'auto'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'auto'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "10vw";
element.style.gridDefinitionRows = "25vh";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'80px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'150px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "min-content";
element.style.gridDefinitionRows = "min-content";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'min-content'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'min-content'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "max-content";
element.style.gridDefinitionRows = "max-content";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'max-content'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'max-content'");

debug("");
debug("Test getting and setting grid-definition-columns and grid-definition-rows to minmax() values through JS");
element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "minmax(55%, 45px)";
element.style.gridDefinitionRows = "minmax(30px, 40%)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'minmax(55%, 45px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'minmax(30px, 40%)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.gridDefinitionColumns = "minmax(22em, 8vh)";
element.style.gridDefinitionRows = "minmax(10vw, 5em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'minmax(220px, 48px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'minmax(80px, 50px)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "minmax(min-content, 8vh)";
element.style.gridDefinitionRows = "minmax(10vw, min-content)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'minmax(min-content, 48px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'minmax(80px, min-content)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.gridDefinitionColumns = "minmax(22em, max-content)";
element.style.gridDefinitionRows = "minmax(max-content, 5em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'minmax(220px, max-content)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'minmax(max-content, 50px)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.gridDefinitionColumns = "minmax(22em, max-content)";
element.style.gridDefinitionRows = "minmax(max-content, 5em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'minmax(220px, max-content)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'minmax(max-content, 50px)'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "minmax(min-content, max-content)";
element.style.gridDefinitionRows = "minmax(max-content, min-content)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'minmax(min-content, max-content)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'minmax(max-content, min-content)'");

// Unit comparison should be case-insensitive.
element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "3600Fr";
element.style.gridDefinitionRows = "154fR";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'3600fr'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'154fr'");

// Float values are allowed.
element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "3.1459fr";
element.style.gridDefinitionRows = "2.718fr";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'3.1459fr'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'2.718fr'");

// A leading '+' is allowed.
element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "+3fr";
element.style.gridDefinitionRows = "+4fr";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'3fr'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'4fr'");

debug("");
debug("Test setting grid-definition-columns and grid-definition-rows to bad values through JS");
element = document.createElement("div");
document.body.appendChild(element);
// No comma.
element.style.gridDefinitionColumns = "minmax(10px 20px)";
// Only 1 argument provided.
element.style.gridDefinitionRows = "minmax(10px)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
// Nested minmax.
element.style.gridDefinitionColumns = "minmax(minmax(10px, 20px), 20px)";
// Only 2 arguments are allowed.
element.style.gridDefinitionRows = "minmax(10px, 20px, 30px)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
// No breadth value.
element.style.gridDefinitionColumns = "minmax()";
// No comma.
element.style.gridDefinitionRows = "minmax(30px 30% 30em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
// Auto is not allowed inside minmax.
element.style.gridDefinitionColumns = "minmax(auto, 8vh)";
element.style.gridDefinitionRows = "minmax(10vw, auto)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "-2fr";
element.style.gridDefinitionRows = "3ffr";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "-2.05fr";
element.style.gridDefinitionRows = "+-3fr";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "0fr";
element.style.gridDefinitionRows = "1r";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = ".0000fr";
element.style.gridDefinitionRows = "13 fr"; // A dimension doesn't allow spaces between the number and the unit.
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element.style.gridDefinitionColumns = "7.-fr";
element.style.gridDefinitionRows = "-8,0fr";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

// Negative values are not allowed.
element.style.gridDefinitionColumns = "-1px";
element.style.gridDefinitionRows = "-6em";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element.style.gridDefinitionColumns = "minmax(-1%, 32%)";
element.style.gridDefinitionRows = "minmax(2vw, -6em)";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

debug("");
debug("Test setting grid-definition-columns and grid-definition-rows back to 'none' through JS");
element.style.gridDefinitionColumns = "18px";
element.style.gridDefinitionRows = "66px";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'18px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'66px'");
element.style.gridDefinitionColumns = "none";
element.style.gridDefinitionRows = "none";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

function testInherit()
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.gridDefinitionColumns = "50px 'last'";
    parentElement.style.gridDefinitionRows = "'first' 101%";

    element = document.createElement("div");
    parentElement.appendChild(element);
    element.style.gridDefinitionColumns = "inherit";
    element.style.gridDefinitionRows = "inherit";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'50px last'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'first 101%'");

    document.body.removeChild(parentElement);
}
debug("");
debug("Test setting grid-definition-columns and grid-definition-rows to 'inherit' through JS");
testInherit();

function testInitial()
{
    element = document.createElement("div");
    document.body.appendChild(element);
    element.style.gridDefinitionColumns = "150% 'last'";
    element.style.gridDefinitionRows = "'first' 1fr";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'150% last'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'first 1fr'");

    element.style.gridDefinitionColumns = "initial";
    element.style.gridDefinitionRows = "initial";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

    document.body.removeChild(element);
}
debug("");
debug("Test setting grid-definition-columns and grid-definition-rows to 'initial' through JS");
testInitial();
