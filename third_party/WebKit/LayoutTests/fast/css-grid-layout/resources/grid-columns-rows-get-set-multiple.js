description('Test that setting and getting grid-definition-columns and grid-definition-rows works as expected');

debug("Test getting |display| set through CSS");
var gridWithFixedElement = document.getElementById("gridWithFixedElement");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('grid-definition-columns')", "'7px 11px'");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('grid-definition-rows')", "'17px 2px'");

var gridWithPercentElement = document.getElementById("gridWithPercentElement");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('grid-definition-columns')", "'53% 99%'");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('grid-definition-rows')", "'27% 52%'");

var gridWithAutoElement = document.getElementById("gridWithAutoElement");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('grid-definition-columns')", "'auto auto'");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('grid-definition-rows')", "'auto auto'");

var gridWithEMElement = document.getElementById("gridWithEMElement");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('grid-definition-columns')", "'100px 120px'");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('grid-definition-rows')", "'150px 170px'");

var gridWithThreeItems = document.getElementById("gridWithThreeItems");
shouldBe("getComputedStyle(gridWithThreeItems, '').getPropertyValue('grid-definition-columns')", "'15px auto 100px'");
shouldBe("getComputedStyle(gridWithThreeItems, '').getPropertyValue('grid-definition-rows')", "'120px 18px auto'");

var gridWithPercentAndViewportPercent = document.getElementById("gridWithPercentAndViewportPercent");
shouldBe("getComputedStyle(gridWithPercentAndViewportPercent, '').getPropertyValue('grid-definition-columns')", "'50% 120px'");
shouldBe("getComputedStyle(gridWithPercentAndViewportPercent, '').getPropertyValue('grid-definition-rows')", "'35% 168px'");

var gridWithFitContentAndFitAvailable = document.getElementById("gridWithFitContentAndFitAvailable");
shouldBe("getComputedStyle(gridWithFitContentAndFitAvailable, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(gridWithFitContentAndFitAvailable, '').getPropertyValue('grid-definition-rows')", "'none'");

var gridWithMinMaxContent = document.getElementById("gridWithMinMaxContent");
shouldBe("getComputedStyle(gridWithMinMaxContent, '').getPropertyValue('grid-definition-columns')", "'min-content max-content'");
shouldBe("getComputedStyle(gridWithMinMaxContent, '').getPropertyValue('grid-definition-rows')", "'max-content min-content'");

var gridWithMinMaxAndFixed = document.getElementById("gridWithMinMaxAndFixed");
shouldBe("getComputedStyle(gridWithMinMaxAndFixed, '').getPropertyValue('grid-definition-columns')", "'minmax(45px, 30%) 15px'");
shouldBe("getComputedStyle(gridWithMinMaxAndFixed, '').getPropertyValue('grid-definition-rows')", "'120px minmax(35%, 10px)'");

var gridWithMinMaxAndMinMaxContent = document.getElementById("gridWithMinMaxAndMinMaxContent");
shouldBe("getComputedStyle(gridWithMinMaxAndMinMaxContent, '').getPropertyValue('grid-definition-columns')", "'minmax(min-content, 30%) 15px'");
shouldBe("getComputedStyle(gridWithMinMaxAndMinMaxContent, '').getPropertyValue('grid-definition-rows')", "'120px minmax(35%, max-content)'");

var gridWithFractionFraction = document.getElementById("gridWithFractionFraction");
shouldBe("getComputedStyle(gridWithFractionFraction, '').getPropertyValue('grid-definition-columns')", "'1fr 2fr'");
shouldBe("getComputedStyle(gridWithFractionFraction, '').getPropertyValue('grid-definition-rows')", "'3fr 4fr'");

var gridWithFractionMinMax = document.getElementById("gridWithFractionMinMax");
shouldBe("getComputedStyle(gridWithFractionMinMax, '').getPropertyValue('grid-definition-columns')", "'minmax(min-content, 45px) 2fr'");
shouldBe("getComputedStyle(gridWithFractionMinMax, '').getPropertyValue('grid-definition-rows')", "'3fr minmax(14px, max-content)'");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

debug("");
debug("Test getting and setting display through JS");
element.style.gridDefinitionColumns = "18px 22px";
element.style.gridDefinitionRows = "66px 70px";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'18px 22px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'66px 70px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "55% 80%";
element.style.gridDefinitionRows = "40% 63%";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'55% 80%'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'40% 63%'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "auto auto";
element.style.gridDefinitionRows = "auto auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'auto auto'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'auto auto'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.gridDefinitionColumns = "auto 16em 22px";
element.style.gridDefinitionRows = "56% 10em auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'auto 160px 22px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'56% 100px auto'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.gridDefinitionColumns = "16em minmax(16px, 20px)";
element.style.gridDefinitionRows = "minmax(10%, 15%) auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'160px minmax(16px, 20px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'minmax(10%, 15%) auto'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.gridDefinitionColumns = "16em 2fr";
element.style.gridDefinitionRows = "14fr auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'160px 2fr'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'14fr auto'");

debug("");
debug("Test getting wrong values set from CSS");
var gridWithNoneAndAuto = document.getElementById("gridWithNoneAndAuto");
shouldBe("getComputedStyle(gridWithNoneAndAuto, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(gridWithNoneAndAuto, '').getPropertyValue('grid-definition-rows')", "'none'");

var gridWithNoneAndFixed = document.getElementById("gridWithNoneAndFixed");
shouldBe("getComputedStyle(gridWithNoneAndFixed, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(gridWithNoneAndFixed, '').getPropertyValue('grid-definition-rows')", "'none'");

debug("");
debug("Test setting and getting wrong values from JS");
element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "none auto";
element.style.gridDefinitionRows = "none auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "none 16em";
element.style.gridDefinitionRows = "none 56%";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "none none";
element.style.gridDefinitionRows = "none none";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "auto none";
element.style.gridDefinitionRows = "auto none";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "auto none 16em";
element.style.gridDefinitionRows = "auto 18em none";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "50% 12vw";
element.style.gridDefinitionRows = "5% 85vh";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'50% 96px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'5% 510px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "-webkit-fit-content -webkit-fit-content";
element.style.gridDefinitionRows = "-webkit-fit-available -webkit-fit-available";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridDefinitionColumns = "auto minmax(16px, auto)";
element.style.gridDefinitionRows = "minmax(auto, 15%) 10vw";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

function testInherit()
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.gridDefinitionColumns = "50px 1fr 'last'";
    parentElement.style.gridDefinitionRows = "101% 'middle' 45px";

    element = document.createElement("div");
    parentElement.appendChild(element);
    element.style.gridDefinitionColumns = "inherit";
    element.style.gridDefinitionRows = "inherit";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'50px 1fr last'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'101% middle 45px'");

    document.body.removeChild(parentElement);
}
debug("");
debug("Test setting grid-definition-columns and grid-definition-rows to 'inherit' through JS");
testInherit();

function testInitial()
{
    element = document.createElement("div");
    document.body.appendChild(element);
    element.style.gridDefinitionColumns = "150% 'middle' 55px";
    element.style.gridDefinitionRows = "1fr 'line' 2fr 'line'";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'150% middle 55px'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'1fr line 2fr line'");

    element.style.gridDefinitionColumns = "initial";
    element.style.gridDefinitionRows = "initial";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", "'none'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", "'none'");

    document.body.removeChild(element);
}
debug("");
debug("Test setting grid-definition-columns and grid-definition-rows to 'initial' through JS");
testInitial();
