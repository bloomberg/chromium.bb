description('Test that setting and getting grid-columns and grid-rows works as expected');

debug("Test getting |display| set through CSS");
var gridWithFixedElement = document.getElementById("gridWithFixedElement");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('grid-columns')", "'7px 11px'");
shouldBe("getComputedStyle(gridWithFixedElement, '').getPropertyValue('grid-rows')", "'17px 2px'");

var gridWithPercentElement = document.getElementById("gridWithPercentElement");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('grid-columns')", "'53% 99%'");
shouldBe("getComputedStyle(gridWithPercentElement, '').getPropertyValue('grid-rows')", "'27% 52%'");

var gridWithAutoElement = document.getElementById("gridWithAutoElement");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('grid-columns')", "'auto auto'");
shouldBe("getComputedStyle(gridWithAutoElement, '').getPropertyValue('grid-rows')", "'auto auto'");

var gridWithEMElement = document.getElementById("gridWithEMElement");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('grid-columns')", "'100px 120px'");
shouldBe("getComputedStyle(gridWithEMElement, '').getPropertyValue('grid-rows')", "'150px 170px'");

var gridWithThreeItems = document.getElementById("gridWithThreeItems");
shouldBe("getComputedStyle(gridWithThreeItems, '').getPropertyValue('grid-columns')", "'15px auto 100px'");
shouldBe("getComputedStyle(gridWithThreeItems, '').getPropertyValue('grid-rows')", "'120px 18px auto'");

var gridWithPercentAndViewportPercent = document.getElementById("gridWithPercentAndViewportPercent");
shouldBe("getComputedStyle(gridWithPercentAndViewportPercent, '').getPropertyValue('grid-columns')", "'50% 120px'");
shouldBe("getComputedStyle(gridWithPercentAndViewportPercent, '').getPropertyValue('grid-rows')", "'35% 168px'");

var gridWithFitContentAndFitAvailable = document.getElementById("gridWithFitContentAndFitAvailable");
shouldBe("getComputedStyle(gridWithFitContentAndFitAvailable, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(gridWithFitContentAndFitAvailable, '').getPropertyValue('grid-rows')", "'none'");

var gridWithMinMaxContent = document.getElementById("gridWithMinMaxContent");
shouldBe("getComputedStyle(gridWithMinMaxContent, '').getPropertyValue('grid-columns')", "'-webkit-min-content -webkit-max-content'");
shouldBe("getComputedStyle(gridWithMinMaxContent, '').getPropertyValue('grid-rows')", "'-webkit-max-content -webkit-min-content'");

var gridWithMinMaxAndFixed = document.getElementById("gridWithMinMaxAndFixed");
shouldBe("getComputedStyle(gridWithMinMaxAndFixed, '').getPropertyValue('grid-columns')", "'minmax(45px, 30%) 15px'");
shouldBe("getComputedStyle(gridWithMinMaxAndFixed, '').getPropertyValue('grid-rows')", "'120px minmax(35%, 10px)'");

var gridWithMinMaxAndMinMaxContent = document.getElementById("gridWithMinMaxAndMinMaxContent");
shouldBe("getComputedStyle(gridWithMinMaxAndMinMaxContent, '').getPropertyValue('grid-columns')", "'minmax(-webkit-min-content, 30%) 15px'");
shouldBe("getComputedStyle(gridWithMinMaxAndMinMaxContent, '').getPropertyValue('grid-rows')", "'120px minmax(35%, -webkit-max-content)'");

var gridWithFractionFraction = document.getElementById("gridWithFractionFraction");
shouldBe("getComputedStyle(gridWithFractionFraction, '').getPropertyValue('grid-columns')", "'1fr 2fr'");
shouldBe("getComputedStyle(gridWithFractionFraction, '').getPropertyValue('grid-rows')", "'3fr 4fr'");

var gridWithFractionMinMax = document.getElementById("gridWithFractionMinMax");
shouldBe("getComputedStyle(gridWithFractionMinMax, '').getPropertyValue('grid-columns')", "'minmax(-webkit-min-content, 45px) 2fr'");
shouldBe("getComputedStyle(gridWithFractionMinMax, '').getPropertyValue('grid-rows')", "'3fr minmax(14px, -webkit-max-content)'");

debug("");
debug("Test the initial value");
var element = document.createElement("div");
document.body.appendChild(element);
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

debug("");
debug("Test getting and setting display through JS");
element.style.gridColumns = "18px 22px";
element.style.gridRows = "66px 70px";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'18px 22px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'66px 70px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "55% 80%";
element.style.gridRows = "40% 63%";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'55% 80%'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'40% 63%'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "auto auto";
element.style.gridRows = "auto auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'auto auto'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'auto auto'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.gridColumns = "auto 16em 22px";
element.style.gridRows = "56% 10em auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'auto 160px 22px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'56% 100px auto'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.gridColumns = "16em minmax(16px, 20px)";
element.style.gridRows = "minmax(10%, 15%) auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'160px minmax(16px, 20px)'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'minmax(10%, 15%) auto'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.font = "10px Ahem";
element.style.gridColumns = "16em 2fr";
element.style.gridRows = "14fr auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'160px 2fr'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'14fr auto'");

debug("");
debug("Test getting wrong values set from CSS");
var gridWithNoneAndAuto = document.getElementById("gridWithNoneAndAuto");
shouldBe("getComputedStyle(gridWithNoneAndAuto, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(gridWithNoneAndAuto, '').getPropertyValue('grid-rows')", "'none'");

var gridWithNoneAndFixed = document.getElementById("gridWithNoneAndFixed");
shouldBe("getComputedStyle(gridWithNoneAndFixed, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(gridWithNoneAndFixed, '').getPropertyValue('grid-rows')", "'none'");

debug("");
debug("Test setting and getting wrong values from JS");
element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "none auto";
element.style.gridRows = "none auto";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "none 16em";
element.style.gridRows = "none 56%";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "none none";
element.style.gridRows = "none none";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "auto none";
element.style.gridRows = "auto none";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "auto none 16em";
element.style.gridRows = "auto 18em none";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "50% 12vw";
element.style.gridRows = "5% 85vh";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'50% 96px'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'5% 510px'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "-webkit-fit-content -webkit-fit-content";
element.style.gridRows = "-webkit-fit-available -webkit-fit-available";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

element = document.createElement("div");
document.body.appendChild(element);
element.style.gridColumns = "auto minmax(16px, auto)";
element.style.gridRows = "minmax(auto, 15%) 10vw";
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

function testInherit()
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.gridColumns = "50px 1fr 'last'";
    parentElement.style.gridRows = "101% 'middle' 45px";

    element = document.createElement("div");
    parentElement.appendChild(element);
    element.style.gridColumns = "inherit";
    element.style.gridRows = "inherit";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'50px 1fr last'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'101% middle 45px'");

    document.body.removeChild(parentElement);
}
debug("");
debug("Test setting grid-columns and grid-rows to 'inherit' through JS");
testInherit();

function testInitial()
{
    element = document.createElement("div");
    document.body.appendChild(element);
    element.style.gridColumns = "150% 'middle' 55px";
    element.style.gridRows = "1fr 'line' 2fr 'line'";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'150% middle 55px'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'1fr line 2fr line'");

    element.style.gridColumns = "initial";
    element.style.gridRows = "initial";
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-columns')", "'none'");
    shouldBe("getComputedStyle(element, '').getPropertyValue('grid-rows')", "'none'");

    document.body.removeChild(element);
}
debug("");
debug("Test setting grid-columns and grid-rows to 'initial' through JS");
testInitial();
