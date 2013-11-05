function testGridDefinitionsValues(element, columnValue, rowValue, computedColumnValue, computedRowValue)
{
    window.element = element;
    var elementID = element.id || "element";
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('grid-definition-columns')", computedColumnValue || columnValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('grid-definition-rows')", computedRowValue || rowValue);
}

function testGridDefinitionsSetJSValues(columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue)
{
    checkGridDefinitionsSetJSValues(true, columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue);
}

function testNonGridDefinitionsSetJSValues(columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue)
{
    checkGridDefinitionsSetJSValues(false, columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue);
}

function checkGridDefinitionsSetJSValues(useGrid, columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    if (useGrid) {
        element.style.display = "grid";
        element.style.width = "800px";
        element.style.height = "600px";
    }
    element.style.font = "10px Ahem"; // Used to resolve em font consistently.
    element.style.gridDefinitionColumns = columnValue;
    element.style.gridDefinitionRows = rowValue;
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('grid-definition-columns')", computedColumnValue || columnValue);
    shouldBeEqualToString("element.style.gridDefinitionColumns", jsColumnValue || columnValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('grid-definition-rows')", computedRowValue || rowValue);
    shouldBeEqualToString("element.style.gridDefinitionRows", jsRowValue || rowValue);
    document.body.removeChild(element);
}

function testGridDefinitionsSetBadJSValues(columnValue, rowValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    element.style.gridDefinitionColumns = columnValue;
    element.style.gridDefinitionRows = rowValue;
    // We can't use testSetJSValues as element.style.gridDefinitionRows returns "".
    testGridDefinitionsValues(element, "none", "none");
    document.body.removeChild(element);
}