(function() {

function checkColumnRowValues(gridItem, columnValue, rowValue)
{
    this.gridItem = gridItem;
    this.gridColumnValue = columnValue;
    this.gridRowValue = rowValue;

    var gridStartEndValues = columnValue.split("/")
    this.gridStartValue = gridStartEndValues[0].trim();
    this.gridEndValue = gridStartEndValues[1].trim();

    var gridBeforeAfterValues = rowValue.split("/")
    this.gridBeforeValue = gridBeforeAfterValues[0].trim();
    this.gridAfterValue = gridBeforeAfterValues[1].trim();

    shouldBe("getComputedStyle(gridItem, '').getPropertyValue('grid-column')", "gridColumnValue");
    shouldBe("getComputedStyle(gridItem, '').getPropertyValue('grid-start')", "gridStartValue");
    shouldBe("getComputedStyle(gridItem, '').getPropertyValue('grid-end')", "gridEndValue");
    shouldBe("getComputedStyle(gridItem, '').getPropertyValue('grid-row')", "gridRowValue");
    shouldBe("getComputedStyle(gridItem, '').getPropertyValue('grid-before')", "gridBeforeValue");
    shouldBe("getComputedStyle(gridItem, '').getPropertyValue('grid-after')", "gridAfterValue");
}

window.testColumnRowCSSParsing = function(id, columnValue, rowValue)
{
    var gridItem = document.getElementById(id);
    checkColumnRowValues(gridItem, columnValue, rowValue);
}

window.testColumnRowJSParsing = function(columnValue, rowValue, expectedColumnValue, expectedRowValue)
{
    var gridItem = document.createElement("div");
    document.body.appendChild(gridItem);
    gridItem.style.gridColumn = columnValue;
    gridItem.style.gridRow = rowValue;

    checkColumnRowValues(gridItem, expectedColumnValue ? expectedColumnValue : columnValue, expectedRowValue ? expectedRowValue : rowValue);

    document.body.removeChild(gridItem);
}

window.testStartBeforeJSParsing = function(startValue, beforeValue, expectedStartValue, expectedBeforeValue)
{
    var gridItem = document.createElement("div");
    document.body.appendChild(gridItem);
    gridItem.style.gridStart = startValue;
    gridItem.style.gridBefore = beforeValue;

    if (expectedStartValue === undefined)
        expectedStartValue = startValue;
    if (expectedBeforeValue === undefined)
        expectedBeforeValue = beforeValue;

    checkColumnRowValues(gridItem, expectedStartValue + " / auto", expectedBeforeValue + " / auto");

    document.body.removeChild(gridItem);
}

window.testEndAfterJSParsing = function(endValue, afterValue, expectedEndValue, expectedAfterValue)
{
    var gridItem = document.createElement("div");
    document.body.appendChild(gridItem);
    gridItem.style.gridEnd = endValue;
    gridItem.style.gridAfter = afterValue;

    if (expectedEndValue === undefined)
        expectedEndValue = endValue;
    if (expectedAfterValue === undefined)
        expectedAfterValue = afterValue;

    checkColumnRowValues(gridItem, "auto / " + expectedEndValue, "auto / " + expectedAfterValue);

    document.body.removeChild(gridItem);
}

window.testColumnRowInvalidJSParsing = function(columnValue, rowValue)
{
    var gridItem = document.createElement("div");
    document.body.appendChild(gridItem);
    gridItem.style.gridColumn = columnValue;
    gridItem.style.gridRow = rowValue;

    checkColumnRowValues(gridItem, "auto / auto", "auto / auto");

    document.body.removeChild(gridItem);
}

var placeholderParentStartValueForInherit = "6";
var placeholderParentEndValueForInherit = "span 2";
var placeholderParentColumnValueForInherit = placeholderParentStartValueForInherit + " / " + placeholderParentEndValueForInherit;
var placeholderParentBeforeValueForInherit = "span 1";
var placeholderParentAfterValueForInherit = "7";
var placeholderParentRowValueForInherit = placeholderParentBeforeValueForInherit + " / " + placeholderParentAfterValueForInherit;

var placeholderStartValueForInitial = "1";
var placeholderEndValueForInitial = "span 2";
var placeholderColumnValueForInitial = placeholderStartValueForInitial + " / " + placeholderEndValueForInitial;
var placeholderBeforeValueForInitial = "span 3";
var placeholderAfterValueForInitial = "5";
var placeholderRowValueForInitial = placeholderBeforeValueForInitial + " / " + placeholderAfterValueForInitial;

function setupInheritTest()
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.gridColumn = placeholderParentColumnValueForInherit;
    parentElement.style.gridRow = placeholderParentRowValueForInherit;

    var gridItem = document.createElement("div");
    parentElement.appendChild(gridItem);
    return parentElement;
}

function setupInitialTest()
{
    var gridItem = document.createElement("div");
    document.body.appendChild(gridItem);
    gridItem.style.gridColumn = placeholderColumnValueForInitial;
    gridItem.style.gridRow = placeholderRowValueForInitial;

    checkColumnRowValues(gridItem, placeholderColumnValueForInitial, placeholderRowValueForInitial);
    return gridItem;
}

window.testColumnRowInheritJSParsing = function(columnValue, rowValue)
{
    var parentElement = setupInheritTest();
    var gridItem = parentElement.firstChild;
    gridItem.style.gridColumn = columnValue;
    gridItem.style.gridRow = rowValue;

    checkColumnRowValues(gridItem, columnValue !== "inherit" ? columnValue : placeholderParentColumnValueForInherit, rowValue !== "inherit" ? rowValue : placeholderParentRowValueForInherit);

    document.body.removeChild(parentElement);
}

window.testStartBeforeInheritJSParsing = function(startValue, beforeValue)
{
    var parentElement = setupInheritTest();
    var gridItem = parentElement.firstChild;
    gridItem.style.gridStart = startValue;
    gridItem.style.gridBefore = beforeValue;

    // Initial value is 'auto' but we shouldn't touch the opposite grid line.
    var columnValueForInherit = (startValue !== "inherit" ? startValue : placeholderParentStartValueForInherit) + " / auto";
    var rowValueForInherit = (beforeValue !== "inherit" ? beforeValue : placeholderParentBeforeValueForInherit) + " / auto";
    checkColumnRowValues(parentElement.firstChild, columnValueForInherit, rowValueForInherit);

    document.body.removeChild(parentElement);
}

window.testEndAfterInheritJSParsing = function(endValue, afterValue)
{
    var parentElement = setupInheritTest();
    var gridItem = parentElement.firstChild;
    gridItem.style.gridEnd = endValue;
    gridItem.style.gridAfter = afterValue;

    // Initial value is 'auto' but we shouldn't touch the opposite grid line.
    var columnValueForInherit = "auto / " + (endValue !== "inherit" ? endValue : placeholderParentEndValueForInherit);
    var rowValueForInherit = "auto / " + (afterValue !== "inherit" ? afterValue : placeholderParentAfterValueForInherit);
    checkColumnRowValues(parentElement.firstChild, columnValueForInherit, rowValueForInherit);

    document.body.removeChild(parentElement);
}

window.testColumnRowInitialJSParsing = function()
{
    var gridItem = setupInitialTest();

    gridItem.style.gridColumn = "initial";
    checkColumnRowValues(gridItem, "auto / auto", placeholderRowValueForInitial);

    gridItem.style.gridRow = "initial";
    checkColumnRowValues(gridItem, "auto / auto", "auto / auto");

    document.body.removeChild(gridItem);
}

window.testStartBeforeInitialJSParsing = function()
{
    var gridItem = setupInitialTest();

    gridItem.style.gridStart = "initial";
    checkColumnRowValues(gridItem, "auto / " + placeholderEndValueForInitial, placeholderRowValueForInitial);

    gridItem.style.gridBefore = "initial";
    checkColumnRowValues(gridItem,  "auto / " + placeholderEndValueForInitial, "auto / " + placeholderAfterValueForInitial);

    document.body.removeChild(gridItem);
}

window.testEndAfterInitialJSParsing = function()
{
    var gridItem = setupInitialTest();

    gridItem.style.gridEnd = "initial";
    checkColumnRowValues(gridItem, placeholderStartValueForInitial + " / auto", placeholderRowValueForInitial);

    gridItem.style.gridAfter = "initial";
    checkColumnRowValues(gridItem, placeholderStartValueForInitial + " / auto", placeholderBeforeValueForInitial + " / auto");

    document.body.removeChild(gridItem);
}

})();
